#include "concurrency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

QkChannel *qk_channel_new(int capacity) {
    if (capacity <= 0) capacity = 1;
    QkChannel *chan = (QkChannel *)qk_malloc(sizeof(QkChannel));
    chan->buffer = (Value *)qk_malloc(sizeof(Value) * capacity);
    chan->head = 0;
    chan->tail = 0;
    chan->count = 0;
    chan->capacity = capacity;
    chan->closed = false;
    chan->refcount = 1;

#ifdef _WIN32
    InitializeCriticalSection(&chan->lock);
    InitializeConditionVariable(&chan->not_empty);
    InitializeConditionVariable(&chan->not_full);
#else
    pthread_mutex_init(&chan->lock, NULL);
    pthread_cond_init(&chan->not_empty, NULL);
    pthread_cond_init(&chan->not_full, NULL);
#endif

    return chan;
}

void qk_channel_free(QkChannel *chan) {
    if (!chan) return;
    chan->refcount--;
    if (chan->refcount > 0) return;

    for (int i = 0; i < chan->count; i++) {
        int idx = (chan->head + i) % chan->capacity;
        val_free(chan->buffer[idx]);
    }
    if (chan->buffer) free(chan->buffer);

#ifdef _WIN32
    DeleteCriticalSection(&chan->lock);
#else
    pthread_mutex_destroy(&chan->lock);
    pthread_cond_destroy(&chan->not_empty);
    pthread_cond_destroy(&chan->not_full);
#endif

    free(chan);
}

void qk_channel_close(QkChannel *chan) {
    if (!chan) return;
#ifdef _WIN32
    EnterCriticalSection(&chan->lock);
    chan->closed = true;
    WakeAllConditionVariable(&chan->not_empty);
    WakeAllConditionVariable(&chan->not_full);
    LeaveCriticalSection(&chan->lock);
#else
    pthread_mutex_lock(&chan->lock);
    chan->closed = true;
    pthread_cond_broadcast(&chan->not_empty);
    pthread_cond_broadcast(&chan->not_full);
    pthread_mutex_unlock(&chan->lock);
#endif
}

bool qk_channel_send(QkChannel *chan, Value val) {
    if (!chan) return false;

#ifdef _WIN32
    EnterCriticalSection(&chan->lock);
    while (chan->count >= chan->capacity && !chan->closed) {
        SleepConditionVariableCS(&chan->not_full, &chan->lock, INFINITE);
    }
    if (chan->closed) {
        LeaveCriticalSection(&chan->lock);
        return false;
    }
    chan->buffer[chan->tail] = val_clone(val);
    chan->tail = (chan->tail + 1) % chan->capacity;
    chan->count++;
    WakeConditionVariable(&chan->not_empty);
    LeaveCriticalSection(&chan->lock);
#else
    pthread_mutex_lock(&chan->lock);
    while (chan->count >= chan->capacity && !chan->closed) {
        pthread_cond_wait(&chan->not_full, &chan->lock);
    }
    if (chan->closed) {
        pthread_mutex_unlock(&chan->lock);
        return false;
    }
    chan->buffer[chan->tail] = val_clone(val);
    chan->tail = (chan->tail + 1) % chan->capacity;
    chan->count++;
    pthread_cond_signal(&chan->not_empty);
    pthread_mutex_unlock(&chan->lock);
#endif

    return true;
}

bool qk_channel_recv(QkChannel *chan, Value *out_val) {
    if (!chan || !out_val) return false;

#ifdef _WIN32
    EnterCriticalSection(&chan->lock);
    while (chan->count == 0 && !chan->closed) {
        SleepConditionVariableCS(&chan->not_empty, &chan->lock, INFINITE);
    }
    if (chan->count == 0 && chan->closed) {
        LeaveCriticalSection(&chan->lock);
        return false;
    }
    *out_val = chan->buffer[chan->head];
    chan->head = (chan->head + 1) % chan->capacity;
    chan->count--;
    WakeConditionVariable(&chan->not_full);
    LeaveCriticalSection(&chan->lock);
#else
    pthread_mutex_lock(&chan->lock);
    while (chan->count == 0 && !chan->closed) {
        pthread_cond_wait(&chan->not_empty, &chan->lock);
    }
    if (chan->count == 0 && chan->closed) {
        pthread_mutex_unlock(&chan->lock);
        return false;
    }
    *out_val = chan->buffer[chan->head];
    chan->head = (chan->head + 1) % chan->capacity;
    chan->count--;
    pthread_cond_signal(&chan->not_full);
    pthread_mutex_unlock(&chan->lock);
#endif

    return true;
}

/* ============================================================
 * Thread Execution
 * ============================================================ */

static void execute_thread_job(QkThread *thread) {
    Interp thread_interp;
    interp_init(&thread_interp);

    if (thread->parent_env) {
        thread_interp.globals->parent = thread->parent_env;
    }

    Value ret = qk_call_value(&thread_interp, thread->fn_val, NULL, 0, 0);
    thread->result = val_clone(ret);
    val_free(ret);

    if (thread->parent_env) {
        thread_interp.globals->parent = NULL;
    }

    interp_free(&thread_interp);
    thread->completed = true;
}

#ifdef _WIN32
static DWORD WINAPI win32_thread_proc(LPVOID param) {
    QkThread *thread = (QkThread *)param;
    execute_thread_job(thread);
    return 0;
}
#else
static void *posix_thread_proc(void *param) {
    QkThread *thread = (QkThread *)param;
    execute_thread_job(thread);
    return NULL;
}
#endif

QkThread *qk_thread_spawn(Value fn_val) {
    QkThread *thread = (QkThread *)qk_malloc(sizeof(QkThread));
    thread->fn_val = val_clone(fn_val);
    thread->result = val_unit();
    thread->completed = false;
    thread->refcount = 1;

    Interp *cur = qk_get_current_interp();
    thread->parent_env = (cur && cur->globals) ? env_snapshot(cur->current_env ? cur->current_env : cur->globals) : NULL;

#ifdef _WIN32
    thread->handle = CreateThread(NULL, 0, win32_thread_proc, thread, 0, &thread->thread_id);
    if (!thread->handle) {
        if (thread->parent_env) env_free(thread->parent_env);
        val_free(thread->fn_val);
        free(thread);
        return NULL;
    }
#else
    int rc = pthread_create(&thread->thread, NULL, posix_thread_proc, thread);
    if (rc != 0) {
        if (thread->parent_env) env_free(thread->parent_env);
        val_free(thread->fn_val);
        free(thread);
        return NULL;
    }
#endif

    return thread;
}

Value qk_thread_join(QkThread *thread) {
    if (!thread) return val_err(val_string("Null thread"));

#ifdef _WIN32
    if (thread->handle) {
        WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#else
    pthread_join(thread->thread, NULL);
#endif

    return val_clone(thread->result);
}

void qk_thread_free(QkThread *thread) {
    if (!thread) return;
    thread->refcount--;
    if (thread->refcount > 0) return;

#ifdef _WIN32
    if (thread->handle) {
        WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#endif

    if (thread->parent_env) {
        env_free(thread->parent_env);
        thread->parent_env = NULL;
    }

    val_free(thread->fn_val);
    val_free(thread->result);
    free(thread);
}
