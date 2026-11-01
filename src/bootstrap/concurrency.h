#ifndef QUOKKA_CONCURRENCY_H
#define QUOKKA_CONCURRENCY_H

#include <stdbool.h>
#include <stdint.h>
#include "quokka.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct QkChannel {
    Value *buffer;
    int head;
    int tail;
    int count;
    int capacity;
    bool closed;
    int refcount;
#ifdef _WIN32
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
#else
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif
} QkChannel;

typedef struct QkThread {
    Value fn_val;
    Value result;
    bool completed;
    int refcount;
    Env *parent_env;
#ifdef _WIN32
    HANDLE handle;
    DWORD thread_id;
#else
    pthread_t thread;
#endif
} QkThread;

QkChannel *qk_channel_new(int capacity);
void       qk_channel_free(QkChannel *chan);
bool       qk_channel_send(QkChannel *chan, Value val);
bool       qk_channel_recv(QkChannel *chan, Value *out_val);
void       qk_channel_close(QkChannel *chan);

QkThread  *qk_thread_spawn(Value fn_val);
Value      qk_thread_join(QkThread *thread);
void       qk_thread_free(QkThread *thread);

#endif /* QUOKKA_CONCURRENCY_H */
