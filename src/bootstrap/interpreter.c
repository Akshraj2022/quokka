#include "quokka.h"
#include "concurrency.h"
#include "typecheck.h"
#include "vm.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define QK_SOCKET SOCKET
#define QK_INVALID_SOCKET INVALID_SOCKET
#define qk_closesocket closesocket
#define QK_LIB_HANDLE HMODULE
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <dlfcn.h>
#define QK_SOCKET int
#define QK_INVALID_SOCKET -1
#define qk_closesocket close
#define QK_LIB_HANDLE void*
#endif

/* Forward declaration — used by builtins that need to call QkFunctions */
static Interp *current_interp = NULL;

/* Helper: call a function or builtin value */
static Value call_value(Value callee, Value *args, int argc, int line) {
    if (callee.type == VAL_BUILTIN) {
        return callee.as.builtin(args, argc, line);
    } else if (callee.type == VAL_FN) {
        if (current_interp->call_depth >= QK_MAX_CALL_DEPTH) {
            fprintf(stderr, "[line %d] Runtime error: Maximum call stack depth exceeded.\n", line);
            current_interp->had_error = true;
            return val_unit();
        }
        current_interp->call_depth++;
        QkFunction *fn = &callee.as.fn;
        if (argc != fn->params.count) {
            fprintf(stderr, "[line %d] Runtime error: Expected %d arguments but got %d.\n",
                    line, fn->params.count, argc);
            current_interp->had_error = true;
            return val_unit();
        }
        /* Lexical scoping: new env's parent is the closure env, or globals if NULL */
        Env *call_env = env_new(fn->closure_env ? fn->closure_env : current_interp->globals);
        for (int i = 0; i < argc; i++) {
            env_define(call_env, fn->params.items[i].name, args[i], false);
        }
        Env *prev_env = current_interp->current_env;
        current_interp->current_env = call_env;

        bool prev_ret = current_interp->returning;
        Value prev_ret_val = current_interp->return_val;
        current_interp->returning = false;

        Value result = interp_exec(current_interp, fn->body);
        if (current_interp->returning) {
            result = current_interp->return_val;
        }

        current_interp->returning = prev_ret;
        current_interp->return_val = prev_ret_val;
        current_interp->current_env = prev_env;
        env_free(call_env);
        current_interp->call_depth--;
        return result;
    } else {
        fprintf(stderr, "[line %d] Runtime error: Not a function.\n", line);
        current_interp->had_error = true;
        return val_unit();
    }
}

Value qk_call_value(Interp *interp, Value callee, Value *args, int argc, int line) {
    Interp *prev = current_interp;
    current_interp = interp;
    Value res = call_value(callee, args, argc, line);
    current_interp = prev;
    return res;
}

/* ============================================================
 * Value constructors
 * ============================================================ */
Value val_int(int64_t v)    { Value val; val.type = VAL_INT;    val.as.int_val = v;   return val; }
Value val_float(double v)   { Value val; val.type = VAL_FLOAT;  val.as.float_val = v; return val; }
Value val_bool(bool v)      { Value val; val.type = VAL_BOOL;   val.as.bool_val = v;  return val; }
Value val_unit(void)        { Value val; val.type = VAL_UNIT;   return val; }

Value val_string(const char *s) {
    Value val; val.type = VAL_STRING;
    val.as.string_val = qk_strdup(s ? s : "");
    return val;
}

Value val_none(void) { Value val; val.type = VAL_NONE; return val; }

Value val_some(Value inner) {
    Value val; val.type = VAL_SOME;
    val.as.inner = qk_malloc(sizeof(Value));
    *val.as.inner = val_clone(inner);
    return val;
}

Value val_ok(Value inner) {
    Value val; val.type = VAL_OK;
    val.as.inner = qk_malloc(sizeof(Value));
    *val.as.inner = val_clone(inner);
    return val;
}

Value val_err(Value inner) {
    Value val; val.type = VAL_ERR;
    val.as.inner = qk_malloc(sizeof(Value));
    *val.as.inner = val_clone(inner);
    return val;
}

Value val_fn(QkFunction f)    { Value val; val.type = VAL_FN;      val.as.fn = f;      return val; }
Value val_builtin(BuiltinFn f){ Value val; val.type = VAL_BUILTIN; val.as.builtin = f; return val; }

Value val_list(void) {
    Value val; val.type = VAL_LIST;
    val.as.list = qk_malloc(sizeof(QkList));
    val.as.list->items = NULL;
    val.as.list->count = 0;
    val.as.list->cap   = 0;
    val.as.list->refcount = 1;
    return val;
}

Value val_struct(const char *name, int count, char **names, Value *values) {
    Value val; val.type = VAL_STRUCT;
    QkStructInstance *s = qk_malloc(sizeof(QkStructInstance));
    s->name = qk_strdup(name ? name : "Struct");
    s->field_count = count;
    s->field_names = qk_malloc(sizeof(char*) * (count > 0 ? count : 1));
    s->field_values = qk_malloc(sizeof(Value) * (count > 0 ? count : 1));
    s->refcount = 1;
    for (int i = 0; i < count; i++) {
        s->field_names[i] = qk_strdup(names[i]);
        s->field_values[i] = val_clone(values[i]);
    }
    val.as.struct_val = s;
    return val;
}

Value val_channel(QkChannel *chan) {
    Value val; val.type = VAL_CHANNEL;
    val.as.channel_val = chan;
    return val;
}

Value val_thread(QkThread *th) {
    Value val; val.type = VAL_THREAD;
    val.as.thread_val = th;
    return val;
}

/* ============================================================
 * Value operations
 * ============================================================ */
const char *val_type_name(Value v) {
    switch (v.type) {
        case VAL_INT:     return "Int";
        case VAL_FLOAT:   return "Float";
        case VAL_BOOL:    return "Bool";
        case VAL_STRING:  return "String";
        case VAL_NONE:
        case VAL_SOME:    return "Option";
        case VAL_OK:
        case VAL_ERR:     return "Result";
        case VAL_FN:
        case VAL_BUILTIN: return "Function";
        case VAL_UNIT:    return "Unit";
        case VAL_LIST:    return "List";
        case VAL_STRUCT:  return v.as.struct_val && v.as.struct_val->name ? v.as.struct_val->name : "Struct";
        case VAL_CHANNEL: return "Channel";
        case VAL_THREAD:  return "Thread";
        default:          return "Unknown";
    }
}

void val_free(Value v) {
    switch (v.type) {
        case VAL_STRING:
            if (v.as.string_val) { free(v.as.string_val); }
            break;
        case VAL_SOME:
        case VAL_OK:
        case VAL_ERR:
            if (v.as.inner) { val_free(*v.as.inner); free(v.as.inner); }
            break;
        case VAL_FN:
            if (v.as.fn.name) { free(v.as.fn.name); }
            if (v.as.fn.closure_env && !v.as.fn.name) {
                env_free(v.as.fn.closure_env);
            }
            break;
        case VAL_LIST:
            if (v.as.list) {
                v.as.list->refcount--;
                if (v.as.list->refcount <= 0) {
                    for (int i = 0; i < v.as.list->count; i++) {
                        val_free(v.as.list->items[i]);
                    }
                    if (v.as.list->items) free(v.as.list->items);
                    free(v.as.list);
                }
            }
            break;
        case VAL_STRUCT:
            if (v.as.struct_val) {
                v.as.struct_val->refcount--;
                if (v.as.struct_val->refcount <= 0) {
                    if (v.as.struct_val->name) free(v.as.struct_val->name);
                    for (int i = 0; i < v.as.struct_val->field_count; i++) {
                        if (v.as.struct_val->field_names[i]) free(v.as.struct_val->field_names[i]);
                        val_free(v.as.struct_val->field_values[i]);
                    }
                    if (v.as.struct_val->field_names) free(v.as.struct_val->field_names);
                    if (v.as.struct_val->field_values) free(v.as.struct_val->field_values);
                    free(v.as.struct_val);
                }
            }
            break;
        case VAL_CHANNEL:
            if (v.as.channel_val) qk_channel_free(v.as.channel_val);
            break;
        case VAL_THREAD:
            if (v.as.thread_val) qk_thread_free(v.as.thread_val);
            break;
        default:
            break;
    }
}

void val_print(Value v) {
    switch (v.type) {
        case VAL_INT:    printf("%lld", (long long)v.as.int_val); break;
        case VAL_FLOAT:  printf("%g", v.as.float_val); break;
        case VAL_BOOL:   printf(v.as.bool_val ? "true" : "false"); break;
        case VAL_STRING: printf("%s", v.as.string_val); break;
        case VAL_NONE:   printf("None"); break;
        case VAL_SOME:   printf("Some("); val_print(*v.as.inner); printf(")"); break;
        case VAL_OK:     printf("Ok(");   val_print(*v.as.inner); printf(")"); break;
        case VAL_ERR:    printf("Err(");  val_print(*v.as.inner); printf(")"); break;
        case VAL_FN:     printf("<fn %s>", v.as.fn.name ? v.as.fn.name : "lambda"); break;
        case VAL_BUILTIN:printf("<builtin>"); break;
        case VAL_UNIT:   printf("()"); break;
        case VAL_AST_NODE: printf("<ast_node>"); break;
        case VAL_LIST: {
            printf("[");
            for (int i = 0; i < v.as.list->count; i++) {
                val_print(v.as.list->items[i]);
                if (i < v.as.list->count - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case VAL_STRUCT: {
            printf("%s {", v.as.struct_val && v.as.struct_val->name ? v.as.struct_val->name : "struct");
            if (v.as.struct_val) {
                for (int i = 0; i < v.as.struct_val->field_count; i++) {
                    printf(" %s: ", v.as.struct_val->field_names[i]);
                    val_print(v.as.struct_val->field_values[i]);
                    if (i < v.as.struct_val->field_count - 1) printf(",");
                }
            }
            printf(" }");
            break;
        }
        case VAL_CHANNEL: printf("<channel>"); break;
        case VAL_THREAD:  printf("<thread>"); break;
    }
}

bool val_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_INT:     return a.as.int_val == b.as.int_val;
        case VAL_FLOAT:   return a.as.float_val == b.as.float_val;
        case VAL_BOOL:    return a.as.bool_val == b.as.bool_val;
        case VAL_STRING:  return (a.as.string_val && b.as.string_val) ? strcmp(a.as.string_val, b.as.string_val) == 0 : (a.as.string_val == b.as.string_val);
        case VAL_NONE:    return true;
        case VAL_SOME:
        case VAL_OK:
        case VAL_ERR:     return val_equal(*a.as.inner, *b.as.inner);
        case VAL_UNIT:    return true;
        case VAL_FN:      return false;
        case VAL_BUILTIN: return a.as.builtin == b.as.builtin;
        case VAL_LIST: {
            if (a.as.list->count != b.as.list->count) return false;
            for (int i = 0; i < a.as.list->count; i++) {
                if (!val_equal(a.as.list->items[i], b.as.list->items[i])) return false;
            }
            return true;
        }
        case VAL_STRUCT: {
            if (a.as.struct_val == b.as.struct_val) return true;
            if (!a.as.struct_val || !b.as.struct_val) return false;
            if (strcmp(a.as.struct_val->name, b.as.struct_val->name) != 0) return false;
            if (a.as.struct_val->field_count != b.as.struct_val->field_count) return false;
            for (int i = 0; i < a.as.struct_val->field_count; i++) {
                if (strcmp(a.as.struct_val->field_names[i], b.as.struct_val->field_names[i]) != 0) return false;
                if (!val_equal(a.as.struct_val->field_values[i], b.as.struct_val->field_values[i])) return false;
            }
            return true;
        }
        case VAL_CHANNEL: return a.as.channel_val == b.as.channel_val;
        case VAL_THREAD:  return a.as.thread_val == b.as.thread_val;
        default: return false;
    }
}

Value val_clone(Value v) {
    switch (v.type) {
        case VAL_INT: case VAL_FLOAT: case VAL_BOOL:
        case VAL_NONE: case VAL_UNIT: case VAL_BUILTIN:
            return v;
        case VAL_STRING: return val_string(v.as.string_val);
        case VAL_SOME:   return val_some(*v.as.inner);
        case VAL_OK:     return val_ok(*v.as.inner);
        case VAL_ERR:    return val_err(*v.as.inner);
        case VAL_FN: {
            Value nv; nv.type = VAL_FN; nv.as.fn = v.as.fn;
            nv.as.fn.name = v.as.fn.name ? qk_strdup(v.as.fn.name) : NULL;
            if (v.as.fn.closure_env && !v.as.fn.name) {
                v.as.fn.closure_env->refcount++;
            }
            return nv;
        }
        case VAL_LIST:
            if (v.as.list) v.as.list->refcount++;
            return v;
        case VAL_STRUCT:
            if (v.as.struct_val) v.as.struct_val->refcount++;
            return v;
        case VAL_CHANNEL:
            if (v.as.channel_val) v.as.channel_val->refcount++;
            return v;
        case VAL_THREAD:
            if (v.as.thread_val) v.as.thread_val->refcount++;
            return v;
        default: return v;
    }
}

void list_push(Value *list, Value item) {
    if (list->type != VAL_LIST) return;
    QkList *l = list->as.list;
    if (l->count >= l->cap) {
        int new_cap = l->cap == 0 ? 8 : l->cap * 2;
        Value *new_items = realloc(l->items, sizeof(Value) * new_cap);
        if (!new_items) qk_fatal("Out of memory expanding list");
        l->items = new_items;
        l->cap = new_cap;
    }
    l->items[l->count++] = item;
}

/* ============================================================
 * Environment
 * ============================================================ */
Env *env_new(Env *parent) {
    Env *env = malloc(sizeof(Env));
    if (!env) qk_fatal("Out of memory allocating environment");
    env->parent      = parent;
    env->bindings    = NULL;
    env->refcount    = 1;
    env->is_snapshot = false;
    return env;
}

void env_define(Env *env, const char *name, Value val, bool is_mut) {
    /* Check for existing binding in THIS scope (not parents) */
    Binding *curr = env->bindings;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            /* Shadow: allow re-definition in same scope (used by NODE_SHADOW) */
            val_free(curr->value);
            curr->value  = val_clone(val);
            curr->is_mut = is_mut;
            return;
        }
        curr = curr->next;
    }
    Binding *b = qk_malloc(sizeof(Binding));
    b->name   = qk_strdup(name);
    b->value  = val_clone(val);
    b->is_mut = is_mut;
    b->next   = env->bindings;
    env->bindings = b;
}

Value *env_get(Env *env, const char *name) {
    while (env) {
        Binding *curr = env->bindings;
        while (curr) {
            if (strcmp(curr->name, name) == 0) return &curr->value;
            curr = curr->next;
        }
        env = env->parent;
    }
    return NULL;
}

bool env_set(Env *env, const char *name, Value val) {
    while (env) {
        Binding *curr = env->bindings;
        while (curr) {
            if (strcmp(curr->name, name) == 0) {
                if (!curr->is_mut) return false;
                val_free(curr->value);
                curr->value = val_clone(val);
                return true;
            }
            curr = curr->next;
        }
        env = env->parent;
    }
    return false;
}

Env *env_snapshot(Env *env) {
    if (!env) return NULL;
    Env *new_env = qk_malloc(sizeof(Env));
    new_env->parent      = env_snapshot(env->parent);
    new_env->bindings    = NULL;
    new_env->refcount    = 1;
    new_env->is_snapshot = true;

    Binding *curr = env->bindings;
    Binding *last = NULL;
    while (curr) {
        Binding *b = qk_malloc(sizeof(Binding));
        b->name   = qk_strdup(curr->name);
        b->value  = val_clone(curr->value);
        b->is_mut = curr->is_mut;
        b->next   = NULL;
        if (!new_env->bindings) new_env->bindings = b;
        else                    last->next = b;
        last = b;
        curr = curr->next;
    }
    return new_env;
}

void env_free(Env *env) {
    if (!env) return;
    env->refcount--;
    if (env->refcount > 0) return;
    Binding *curr = env->bindings;
    while (curr) {
        Binding *next = curr->next;
        if (curr->name) free(curr->name);
        val_free(curr->value);
        free(curr);
        curr = next;
    }
    if (env->is_snapshot && env->parent) {
        env_free(env->parent);
    }
    free(env);
}

/* ============================================================
 * Built-in functions
 * ============================================================ */
static Value builtin_print(Value *args, int argc, int line) {
    (void)line;
    for (int i = 0; i < argc; i++) val_print(args[i]);
    fflush(stdout);
    return val_unit();
}

static Value builtin_println(Value *args, int argc, int line) {
    (void)line;
    for (int i = 0; i < argc; i++) val_print(args[i]);
    printf("\n");
    fflush(stdout);
    return val_unit();
}

static Value builtin_to_string(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1) return val_string("");
    char buf[512];
    switch (args[0].type) {
        case VAL_INT:    snprintf(buf, sizeof(buf), "%lld", (long long)args[0].as.int_val); return val_string(buf);
        case VAL_FLOAT:  snprintf(buf, sizeof(buf), "%g", args[0].as.float_val);            return val_string(buf);
        case VAL_BOOL:   return val_string(args[0].as.bool_val ? "true" : "false");
        case VAL_STRING: return val_clone(args[0]);
        case VAL_UNIT:   return val_string("()");
        case VAL_NONE:   return val_string("None");
        case VAL_SOME: {
            Value s = builtin_to_string(args[0].as.inner, 1, line);
            snprintf(buf, sizeof(buf), "Some(%s)", s.as.string_val);
            val_free(s);
            return val_string(buf);
        }
        case VAL_OK: {
            Value s = builtin_to_string(args[0].as.inner, 1, line);
            snprintf(buf, sizeof(buf), "Ok(%s)", s.as.string_val);
            val_free(s);
            return val_string(buf);
        }
        case VAL_ERR: {
            Value s = builtin_to_string(args[0].as.inner, 1, line);
            snprintf(buf, sizeof(buf), "Err(%s)", s.as.string_val);
            val_free(s);
            return val_string(buf);
        }
        case VAL_STRUCT: snprintf(buf, sizeof(buf), "<struct %s>", args[0].as.struct_val && args[0].as.struct_val->name ? args[0].as.struct_val->name : ""); return val_string(buf);
        default:         return val_string("<value>");
    }
}

static Value builtin_to_int(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) return val_err(val_string("invalid integer"));
    char *end;
    long long v = strtoll(args[0].as.string_val, &end, 10);
    if (*end != '\0') return val_err(val_string("invalid integer"));
    return val_ok(val_int((int64_t)v));
}

static Value builtin_to_float(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) return val_err(val_string("invalid float"));
    char *end;
    double v = strtod(args[0].as.string_val, &end);
    if (*end != '\0') return val_err(val_string("invalid float"));
    return val_ok(val_float(v));
}

static Value builtin_len(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1) return val_int(0);
    if (args[0].type == VAL_LIST) {
        return val_int(args[0].as.list->count);
    } else if (args[0].type == VAL_STRING) {
        return val_int(strlen(args[0].as.string_val));
    }
    return val_int(0);
}

static Value builtin_type_of(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1) return val_string("Unknown");
    return val_string(val_type_name(args[0]));
}

static Value builtin_unwrap_or(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2) return val_unit();
    if (args[0].type == VAL_SOME) return val_clone(*args[0].as.inner);
    if (args[0].type == VAL_OK)   return val_clone(*args[0].as.inner);
    return val_clone(args[1]);
}

static Value builtin_map(Value *args, int argc, int line) {
    if (argc != 2) return val_unit();
    if (args[0].type == VAL_SOME) {
        Value inner = *args[0].as.inner;
        Value res = call_value(args[1], &inner, 1, line);
        Value out = val_some(res);
        val_free(res);
        return out;
    } else if (args[0].type == VAL_NONE) {
        return val_none();
    } else if (args[0].type == VAL_OK) {
        Value inner = *args[0].as.inner;
        Value res = call_value(args[1], &inner, 1, line);
        Value out = val_ok(res);
        val_free(res);
        return out;
    } else if (args[0].type == VAL_ERR) {
        return val_clone(args[0]);
    }
    return val_unit();
}

static Value builtin_and_then(Value *args, int argc, int line) {
    if (argc != 2) return val_unit();
    if (args[0].type == VAL_SOME) {
        Value inner = *args[0].as.inner;
        return call_value(args[1], &inner, 1, line);
    } else if (args[0].type == VAL_NONE) {
        return val_none();
    } else if (args[0].type == VAL_OK) {
        Value inner = *args[0].as.inner;
        return call_value(args[1], &inner, 1, line);
    } else if (args[0].type == VAL_ERR) {
        return val_clone(args[0]);
    }
    return val_unit();
}

static Value builtin_map_err(Value *args, int argc, int line) {
    if (argc != 2) return val_unit();
    if (args[0].type == VAL_ERR) {
        Value inner = *args[0].as.inner;
        Value res = call_value(args[1], &inner, 1, line);
        Value out = val_err(res);
        val_free(res);
        return out;
    } else if (args[0].type == VAL_OK) {
        return val_clone(args[0]);
    }
    return val_unit();
}

static Value builtin_push(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_LIST) return val_unit();
    list_push(&args[0], val_clone(args[1]));
    return val_unit();
}

static Value builtin_range(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT)
        return val_list();
    Value list = val_list();
    for (int64_t i = args[0].as.int_val; i < args[1].as.int_val; i++)
        list_push(&list, val_int(i));
    return list;
}

static Value builtin_input(Value *args, int argc, int line) {
    (void)args; (void)argc; (void)line;
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\r\n")] = 0;
        return val_string(buf);
    }
    return val_string("");
}

static Value builtin_substring(Value *args, int argc, int line) {
    (void)line;
    if (argc != 3 || args[0].type != VAL_STRING || args[1].type != VAL_INT || args[2].type != VAL_INT)
        return val_string("");
    int64_t start = args[1].as.int_val;
    int64_t end = args[2].as.int_val;
    int64_t len = (int64_t)strlen(args[0].as.string_val);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return val_string("");
    size_t sub_len = (size_t)(end - start);
    char *buf = qk_malloc(sub_len + 1);
    memcpy(buf, args[0].as.string_val + start, sub_len);
    buf[sub_len] = '\0';
    Value res = val_string(buf);
    free(buf);
    return res;
}

static Value builtin_file_read(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) return val_err(val_string("Expected string path"));
    FILE *f = fopen(args[0].as.string_val, "rb");
    if (!f) return val_err(val_string("Cannot open file"));
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 0) { fclose(f); return val_err(val_string("Invalid file size")); }
    char *string = malloc(fsize + 1);
    if (!string) { fclose(f); return val_err(val_string("Out of memory reading file")); }
    size_t nread = (fsize > 0) ? fread(string, 1, fsize, f) : 0;
    fclose(f);
    string[nread] = 0;
    Value res = val_string(string);
    free(string);
    return val_ok(res);
}

static Value builtin_file_write(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return val_err(val_string("Expected (path, content)"));
    FILE *f = fopen(args[0].as.string_val, "wb");
    if (!f) return val_err(val_string("Cannot open file"));
    size_t to_write = strlen(args[1].as.string_val);
    size_t written = fwrite(args[1].as.string_val, 1, to_write, f);
    fclose(f);
    if (written != to_write) return val_err(val_string("Failed to write all bytes to file"));
    return val_ok(val_bool(true));
}

#include <stdlib.h>
#include <string.h>

extern bool joey_execute_job(const char* job_file);
static Value builtin_joey_run_native(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) return val_err(val_string("Expected job file string"));
    bool ok = joey_execute_job(args[0].as.string_val);
    Value list = val_list();
    Value status_pair = val_list(); list_push(&status_pair, val_string("status")); list_push(&status_pair, val_int(ok ? 0 : 1));
    Value out_pair = val_list(); list_push(&out_pair, val_string("output")); list_push(&out_pair, val_string(""));
    list_push(&list, status_pair); list_push(&list, out_pair);
    Value ok_val = val_ok(list);
    val_free(list);
    return ok_val;
}

static Value builtin_exec(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_LIST) return val_err(val_string("Expected list of string args"));
    
    // Naive concatenation for demonstration, but user requested no string concat for execution...
    // Actually, in C we should use _spawnvp or similar for safe execution, but for simplicity in Windows stage-0:
    char cmd[8192] = {0};
    for (int i = 0; i < args[0].as.list->count; i++) {
        if (args[0].as.list->items[i].type != VAL_STRING) return val_err(val_string("Args must be strings"));
        const char *arg = args[0].as.list->items[i].as.string_val;
        bool has_space = strchr(arg, ' ') != NULL;
        if (strlen(cmd) + strlen(arg) + 8 >= sizeof(cmd)) return val_err(val_string("Command buffer overflow"));
        if (strlen(cmd) + strlen(arg) + 8 >= sizeof(cmd)) return val_err(val_string("Command buffer overflow"));
        if (has_space) strcat(cmd, "\"");
        strcat(cmd, arg);
        if (has_space) strcat(cmd, "\"");
        strcat(cmd, " ");
    }
    
    // Redirect stdout to a temp file
    strcat(cmd, " > .qka_exec_out 2> .qka_exec_err");
    int status = system(cmd);
    
    FILE *f = fopen(".qka_exec_out", "rb");
    char *out_str = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        out_str = malloc(fsize + 1);
        if (out_str) {
            size_t n = (fsize > 0) ? fread(out_str, 1, fsize, f) : 0;
            out_str[n] = 0;
        }
        fclose(f);
    }
    
    FILE *ferr = fopen(".qka_exec_err", "rb");
    char *err_str = NULL;
    if (ferr) {
        fseek(ferr, 0, SEEK_END);
        long fsize = ftell(ferr);
        fseek(ferr, 0, SEEK_SET);
        err_str = malloc(fsize + 1);
        if (err_str) {
            size_t n = (fsize > 0) ? fread(err_str, 1, fsize, ferr) : 0;
            err_str[n] = 0;
        }
        fclose(ferr);
    }

    Value map = val_list(); // Representing dict as list of [key, val] pairs
    Value k_status = val_string("status");
    Value v_status = val_int(status);
    Value pair1 = val_list(); list_push(&pair1, k_status); list_push(&pair1, v_status);
    list_push(&map, pair1);
    
    Value k_out = val_string("stdout");
    Value v_out = out_str ? val_string(out_str) : val_string("");
    if (out_str) free(out_str);
    Value pair2 = val_list(); list_push(&pair2, k_out); list_push(&pair2, v_out);
    list_push(&map, pair2);
    
    Value k_err = val_string("stderr");
    Value v_err = err_str ? val_string(err_str) : val_string("");
    if (err_str) free(err_str);
    Value pair3 = val_list(); list_push(&pair3, k_err); list_push(&pair3, v_err);
    list_push(&map, pair3);
    
    return val_ok(map);
}

static void qk_socket_init(void) {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        initialized = true;
    }
#endif
}

static Value builtin_socket_connect(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_INT)
        return val_err(val_string("Expected socket_connect(host: string, port: int)"));
    qk_socket_init();
    const char *host = args[0].as.string_val;
    int port = (int)args[1].as.int_val;

    QK_SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == QK_INVALID_SOCKET) return val_err(val_string("Failed to create socket"));

    struct hostent *he = gethostbyname(host);
    if (!he) {
        qk_closesocket(s);
        return val_err(val_string("Could not resolve host"));
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        qk_closesocket(s);
        return val_err(val_string("Failed to connect to host"));
    }
    return val_ok(val_int((int64_t)s));
}

static Value builtin_socket_listen(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_INT)
        return val_err(val_string("Expected socket_listen(port: int)"));
    qk_socket_init();
    int port = (int)args[0].as.int_val;

    QK_SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == QK_INVALID_SOCKET) return val_err(val_string("Failed to create socket"));

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        qk_closesocket(s);
        return val_err(val_string("Failed to bind socket"));
    }
    if (listen(s, 16) != 0) {
        qk_closesocket(s);
        return val_err(val_string("Failed to listen on socket"));
    }
    return val_ok(val_int((int64_t)s));
}

static Value builtin_socket_accept(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_INT)
        return val_err(val_string("Expected socket_accept(sockfd: int)"));
    QK_SOCKET s = (QK_SOCKET)args[0].as.int_val;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    QK_SOCKET client_fd = accept(s, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == QK_INVALID_SOCKET) return val_err(val_string("Failed to accept connection"));
    return val_ok(val_int((int64_t)client_fd));
}

static Value builtin_socket_send(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING)
        return val_err(val_string("Expected socket_send(sockfd: int, data: string)"));
    QK_SOCKET s = (QK_SOCKET)args[0].as.int_val;
    const char *data = args[1].as.string_val;
    int len = (int)strlen(data);
    int sent = send(s, data, len, 0);
    if (sent < 0) return val_err(val_string("Failed to send data"));
    return val_ok(val_int((int64_t)sent));
}

static Value builtin_socket_recv(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT)
        return val_err(val_string("Expected socket_recv(sockfd: int, max_bytes: int)"));
    QK_SOCKET s = (QK_SOCKET)args[0].as.int_val;
    int max_bytes = (int)args[1].as.int_val;
    if (max_bytes <= 0 || max_bytes > 1048576) max_bytes = 4096;
    char *buf = qk_malloc(max_bytes + 1);
    int bytes_read = recv(s, buf, max_bytes, 0);
    if (bytes_read < 0) {
        free(buf);
        return val_err(val_string("Failed to receive data"));
    }
    buf[bytes_read] = '\0';
    Value out = val_ok(val_string(buf));
    free(buf);
    return out;
}

static Value builtin_socket_close(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_INT)
        return val_err(val_string("Expected socket_close(sockfd: int)"));
    QK_SOCKET s = (QK_SOCKET)args[0].as.int_val;
    qk_closesocket(s);
    return val_ok(val_bool(true));
}

#define MAX_FFI_HANDLES 32
static QK_LIB_HANDLE ffi_handles[MAX_FFI_HANDLES] = {0};

static Value builtin_ffi_open(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING)
        return val_err(val_string("Expected ffi_open(path: string)"));
    const char *path = args[0].as.string_val;
    QK_LIB_HANDLE handle = NULL;
#ifdef _WIN32
    handle = LoadLibraryA(path);
#else
    handle = dlopen(path, RTLD_LAZY);
#endif
    if (!handle) return val_err(val_string("Failed to load library"));
    for (int i = 0; i < MAX_FFI_HANDLES; i++) {
        if (!ffi_handles[i]) {
            ffi_handles[i] = handle;
            return val_ok(val_int(i + 1));
        }
    }
    return val_err(val_string("FFI handle table full"));
}

static Value builtin_ffi_close(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_INT)
        return val_err(val_string("Expected ffi_close(handle: int)"));
    int idx = (int)args[0].as.int_val - 1;
    if (idx < 0 || idx >= MAX_FFI_HANDLES || !ffi_handles[idx])
        return val_err(val_string("Invalid FFI handle"));
#ifdef _WIN32
    FreeLibrary(ffi_handles[idx]);
#else
    dlclose(ffi_handles[idx]);
#endif
    ffi_handles[idx] = NULL;
    return val_ok(val_bool(true));
}

static Value builtin_ffi_available(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING)
        return val_bool(false);
    int idx = (int)args[0].as.int_val - 1;
    if (idx < 0 || idx >= MAX_FFI_HANDLES || !ffi_handles[idx])
        return val_bool(false);
#ifdef _WIN32
    FARPROC sym = GetProcAddress(ffi_handles[idx], args[1].as.string_val);
#else
    void *sym = dlsym(ffi_handles[idx], args[1].as.string_val);
#endif
    return val_bool(sym != NULL);
}

static Value builtin_channel_new(Value *args, int argc, int line) {
    (void)line;
    int cap = 1;
    if (argc >= 1 && args[0].type == VAL_INT) {
        cap = (int)args[0].as.int_val;
    }
    QkChannel *chan = qk_channel_new(cap);
    return val_channel(chan);
}

static Value builtin_channel_send(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_CHANNEL) {
        return val_err(val_string("channel_send expects Channel and Value"));
    }
    bool ok = qk_channel_send(args[0].as.channel_val, args[1]);
    if (!ok) return val_err(val_string("Channel closed"));
    return val_ok(val_bool(true));
}

static Value builtin_channel_recv(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_CHANNEL) {
        return val_err(val_string("channel_recv expects Channel"));
    }
    Value out_val = val_unit();
    bool ok = qk_channel_recv(args[0].as.channel_val, &out_val);
    if (!ok) return val_err(val_string("Channel closed"));
    Value res = val_ok(out_val);
    val_free(out_val);
    return res;
}

static Value builtin_channel_close(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_CHANNEL) {
        return val_err(val_string("channel_close expects Channel"));
    }
    qk_channel_close(args[0].as.channel_val);
    return val_ok(val_bool(true));
}

static Value builtin_thread_spawn(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || (args[0].type != VAL_FN && args[0].type != VAL_BUILTIN)) {
        return val_err(val_string("thread_spawn expects Function"));
    }
    QkThread *th = qk_thread_spawn(args[0]);
    if (!th) return val_err(val_string("Failed to spawn thread"));
    return val_thread(th);
}

static Value builtin_thread_join(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_THREAD) {
        return val_err(val_string("thread_join expects Thread"));
    }
    Value res = qk_thread_join(args[0].as.thread_val);
    Value ok_res = val_ok(res);
    val_free(res);
    return ok_res;
}

static Value builtin_typecheck(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) {
        return val_err(val_string("typecheck expects file path string"));
    }
    char *src = read_file(args[0].as.string_val);
    if (!src) return val_err(val_string("Could not read file"));
    Parser p;
    parser_init(&p, src);
    Node *ast = parser_parse(&p);
    free(src);
    if (!ast || p.had_error) {
        if (ast) node_free(ast);
        return val_err(val_string("Parse error"));
    }
    bool ok = qk_typecheck_program(ast);
    node_free(ast);
    return val_ok(val_bool(ok));
}

static Value builtin_vm_eval(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) {
        return val_err(val_string("vm_eval expects file path string"));
    }
    char *src = read_file(args[0].as.string_val);
    if (!src) return val_err(val_string("Could not read file"));
    Parser p;
    parser_init(&p, src);
    Node *ast = parser_parse(&p);
    free(src);
    if (!ast || p.had_error) {
        if (ast) node_free(ast);
        return val_err(val_string("Parse error"));
    }
    QkFunctionObj *fn = qk_compile_ast(ast);
    node_free(ast);
    if (!fn) return val_err(val_string("Compilation failed"));

    QkVM vm;
    qk_vm_init(&vm, current_interp);
    QkVMResult res = qk_vm_run(&vm, fn);
    qk_function_obj_free(fn);
    qk_vm_free(&vm);

    return (res == VM_RESULT_OK) ? val_ok(val_bool(true)) : val_err(val_string("VM execution error"));
}

static Value builtin_list_set(Value *args, int argc, int line) {
    (void)line;
    if (argc != 3 || args[0].type != VAL_LIST || args[1].type != VAL_INT)
        return val_unit();
    int idx = (int)args[1].as.int_val;
    if (idx >= 0 && idx < args[0].as.list->count) {
        val_free(args[0].as.list->items[idx]);
        args[0].as.list->items[idx] = val_clone(args[2]);
    }
    return val_unit();
}


#include "sha256.h"



static Value val_ast_node(Node *node) {
    Value v; v.type = VAL_AST_NODE; v.as.ast_node = node; return v;
}

static Value builtin_parse_native(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) return val_unit();
    Parser parser;
    parser_init(&parser, args[0].as.string_val);
    Node *program = parser_parse(&parser);
    if (parser.had_error) return val_unit();
    return val_ast_node(program);
}

extern Value interp_exec(Interp *interp, Node *node);

static Value builtin_eval_native(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_AST_NODE) return val_unit();
    return interp_exec(current_interp, args[0].as.ast_node);
}


static Value ast_to_list(Node *node);
static void serialize_value(Value val, char *buf);

static Value ast_to_list(Node *node) {
    if (!node) {
        return val_none();
    }
    Value list = val_list();
    switch (node->type) {
        case NODE_INT_LIT:
            list_push(&list, val_string("Int"));
            list_push(&list, val_int(node->as.int_val));
            break;
        case NODE_FLOAT_LIT:
            list_push(&list, val_string("Float"));
            list_push(&list, val_float(node->as.float_val));
            break;
        case NODE_STRING_LIT:
            list_push(&list, val_string("String"));
            list_push(&list, val_string(node->as.string_val));
            break;
        case NODE_BOOL_LIT:
            list_push(&list, val_string("Bool"));
            list_push(&list, val_bool(node->as.bool_val));
            break;
        case NODE_IDENT:
            list_push(&list, val_string("Ident"));
            list_push(&list, val_string(node->as.name));
            break;
        case NODE_LET:
            list_push(&list, val_string("Let"));
            list_push(&list, val_string(node->as.let_bind.name));
            list_push(&list, val_bool(node->as.let_bind.is_mut));
            list_push(&list, ast_to_list(node->as.let_bind.value));
            break;
        case NODE_SHADOW:
            list_push(&list, val_string("Shadow"));
            list_push(&list, val_string(node->as.shadow.name));
            list_push(&list, ast_to_list(node->as.shadow.value));
            break;
        case NODE_ASSIGN:
            list_push(&list, val_string("Assign"));
            list_push(&list, val_string(node->as.assign.name));
            list_push(&list, ast_to_list(node->as.assign.value));
            break;
        case NODE_IF:
            list_push(&list, val_string("If"));
            list_push(&list, ast_to_list(node->as.if_expr.cond));
            list_push(&list, ast_to_list(node->as.if_expr.then_b));
            if (node->as.if_expr.else_b) {
                list_push(&list, ast_to_list(node->as.if_expr.else_b));
            } else {
                Value no = val_list();
                list_push(&no, val_string("None"));
                list_push(&list, no);
            }
            break;
        case NODE_WHILE:
            list_push(&list, val_string("While"));
            list_push(&list, ast_to_list(node->as.while_loop.cond));
            list_push(&list, ast_to_list(node->as.while_loop.body));
            break;
        case NODE_FOR:
            list_push(&list, val_string("For"));
            list_push(&list, val_string(node->as.for_loop.var_name));
            list_push(&list, ast_to_list(node->as.for_loop.iter));
            list_push(&list, ast_to_list(node->as.for_loop.body));
            break;
        case NODE_BREAK:
            list_push(&list, val_string("Break"));
            break;
        case NODE_CONTINUE:
            list_push(&list, val_string("Continue"));
            break;
        case NODE_BLOCK: {
            list_push(&list, val_string("Block"));
            Value stmts = val_list();
            for (int i = 0; i < node->as.block.stmts.count; i++) {
                list_push(&stmts, ast_to_list(node->as.block.stmts.items[i]));
            }
            list_push(&list, stmts);
            break;
        }
        case NODE_RETURN:
            list_push(&list, val_string("Return"));
            list_push(&list, ast_to_list(node->as.ret.value));
            break;
        case NODE_BINARY: {
            list_push(&list, val_string("Binary"));
            const char *op = "";
            switch (node->as.binary.op) {
                case TOK_PLUS: op = "+"; break;
                case TOK_MINUS: op = "-"; break;
                case TOK_STAR: op = "*"; break;
                case TOK_SLASH: op = "/"; break;
                case TOK_PERCENT: op = "%"; break;
                case TOK_EQ: op = "=="; break;
                case TOK_NEQ: op = "!="; break;
                case TOK_LT: op = "<"; break;
                case TOK_GT: op = ">"; break;
                case TOK_LEQ: op = "<="; break;
                case TOK_GEQ: op = ">="; break;
                case TOK_AND: op = "and"; break;
                case TOK_OR: op = "or"; break;
                case TOK_CONCAT: op = "++"; break;
                default: op = "?"; break;
            }
            list_push(&list, val_string(op));
            list_push(&list, ast_to_list(node->as.binary.left));
            list_push(&list, ast_to_list(node->as.binary.right));
            break;
        }
        case NODE_UNARY: {
            list_push(&list, val_string("Unary"));
            list_push(&list, val_string(node->as.unary.op == TOK_MINUS ? "-" : "not"));
            list_push(&list, ast_to_list(node->as.unary.operand));
            break;
        }
        case NODE_CALL: {
            list_push(&list, val_string("Call"));
            Node ident_node; ident_node.type = NODE_IDENT; ident_node.as.name = node->as.call.fn_name;
            list_push(&list, ast_to_list(&ident_node));
            Value args = val_list();
            for (int i = 0; i < node->as.call.args.count; i++) {
                list_push(&args, ast_to_list(node->as.call.args.items[i]));
            }
            list_push(&list, args);
            break;
        }
        case NODE_FN_DEF: {
            list_push(&list, val_string("Fn"));
            list_push(&list, val_string(node->as.fn_def.name));
            Value params = val_list();
            for (int i = 0; i < node->as.fn_def.params.count; i++) {
                list_push(&params, val_string(node->as.fn_def.params.items[i].name));
            }
            list_push(&list, params);
            list_push(&list, ast_to_list(node->as.fn_def.body));
            break;
        }
        case NODE_LAMBDA: {
            list_push(&list, val_string("Fn"));
            list_push(&list, val_string(""));
            Value params = val_list();
            for (int i = 0; i < node->as.lambda.params.count; i++) {
                list_push(&params, val_string(node->as.lambda.params.items[i].name));
            }
            list_push(&list, params);
            list_push(&list, ast_to_list(node->as.lambda.body));
            break;
        }
        case NODE_SOME:
            list_push(&list, val_string("Some"));
            list_push(&list, ast_to_list(node->as.some_val.value));
            break;
        case NODE_NONE:
            list_push(&list, val_string("None"));
            break;
        case NODE_OK:
            list_push(&list, val_string("Ok"));
            list_push(&list, ast_to_list(node->as.ok_val.value));
            break;
        case NODE_ERR:
            list_push(&list, val_string("Err"));
            list_push(&list, ast_to_list(node->as.err_val.value));
            break;
        case NODE_MATCH: {
            list_push(&list, val_string("Match"));
            list_push(&list, ast_to_list(node->as.match_expr.subject));
            Value arms = val_list();
            for (int i = 0; i < node->as.match_expr.arm_count; i++) {
                Value arm = val_list();
                const char *pat_type = node->as.match_expr.arms[i].pattern;
                list_push(&arm, val_string(pat_type));
                list_push(&arm, val_string(node->as.match_expr.arms[i].binding ? node->as.match_expr.arms[i].binding : ""));
                list_push(&arm, ast_to_list(node->as.match_expr.arms[i].body));
                list_push(&arms, arm);
            }
            list_push(&list, arms);
            break;
        }
        case NODE_LIST_LIT: {
            list_push(&list, val_string("ListLit"));
            Value elems = val_list();
            for (int i = 0; i < node->as.list_lit.elements.count; i++) {
                list_push(&elems, ast_to_list(node->as.list_lit.elements.items[i]));
            }
            list_push(&list, elems);
            break;
        }
        case NODE_INDEX:
            list_push(&list, val_string("Index"));
            list_push(&list, ast_to_list(node->as.index.array));
            list_push(&list, ast_to_list(node->as.index.index));
            break;
        case NODE_UNWRAP:
            list_push(&list, val_string("Unwrap"));
            list_push(&list, ast_to_list(node->as.unwrap.operand));
            break;
        case NODE_PROGRAM: {
            list_push(&list, val_string("Program"));
            Value stmts = val_list();
            for (int i = 0; i < node->as.program.count; i++) {
                list_push(&stmts, ast_to_list(node->as.program.items[i]));
            }
            list_push(&list, stmts);
            break;
        }
        case NODE_IMPORT:
            list_push(&list, val_string("Import"));
            list_push(&list, val_string(node->as.import_stmt.path ? node->as.import_stmt.path : ""));
            break;
        case NODE_STRUCT_DEF:
        case NODE_STRUCT_CONSTRUCT:
            list_push(&list, val_string("StructDef"));
            list_push(&list, val_string(node->as.struct_def.name ? node->as.struct_def.name : ""));
            break;
        case NODE_GET_FIELD:
            list_push(&list, val_string("GetField"));
            list_push(&list, ast_to_list(node->as.get_field.object));
            list_push(&list, val_string(node->as.get_field.field ? node->as.get_field.field : ""));
            break;
        case NODE_SET_FIELD:
            list_push(&list, val_string("SetField"));
            list_push(&list, ast_to_list(node->as.set_field.object));
            list_push(&list, val_string(node->as.set_field.field ? node->as.set_field.field : ""));
            list_push(&list, ast_to_list(node->as.set_field.value));
            break;
    }
    return list;
}

static void serialize_value(Value val, char *buf) {
    if (val.type == VAL_NONE) { strcat(buf, "[\"None\"]"); return; }
    if (val.type == VAL_STRING) {
        strcat(buf, "\"");
        strcat(buf, val.as.string_val);
        strcat(buf, "\"");
    } else if (val.type == VAL_INT) {
        char num[32]; sprintf(num, "%lld", (long long)val.as.int_val); strcat(buf, num);
    } else if (val.type == VAL_FLOAT) {
        char num[64]; sprintf(num, "%g", val.as.float_val); strcat(buf, num);
    } else if (val.type == VAL_BOOL) {
        strcat(buf, val.as.bool_val ? "true" : "false");
    } else if (val.type == VAL_LIST) {
        strcat(buf, "[");
        for (int i = 0; i < val.as.list->count; i++) {
            serialize_value(val.as.list->items[i], buf);
            if (i < val.as.list->count - 1) strcat(buf, ",");
        }
        strcat(buf, "]");
    } else {
        strcat(buf, "null");
    }
}

static Value builtin_serialize_ast(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1) return val_string("None");
    char *buf = malloc(10 * 1024 * 1024);
    buf[0] = 0;
    
    if (args[0].type == VAL_AST_NODE) {
        Value list = ast_to_list(args[0].as.ast_node);
        serialize_value(list, buf);
    } else if (args[0].type == VAL_LIST) {
        serialize_value(args[0], buf);
    } else {
        strcat(buf, "None");
    }
    
    Value res = val_string(buf);
    free(buf);
    return res;
}

static Value builtin_hash_sha256(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_STRING) return val_string("");
    
    SHA256_CTX ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)args[0].as.string_val, strlen(args[0].as.string_val));
    sha256_final(&ctx, hash);
    
    char hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hex + (i * 2), "%02x", hash[i]);
    }
    hex[64] = 0;
    
    return val_string(hex);
}


bool capture_mode = false;
static Value builtin_set_capture_mode(Value *args, int argc, int line) {
    if (argc == 1 && args[0].type == VAL_BOOL) {
        capture_mode = args[0].as.bool_val;
    }
    return val_unit();
}

char captured_output[65536] = {0};

static Value builtin_capture_println(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1) return val_unit();
    /* unused buf removed */
    if (args[0].type == VAL_STRING) {
        strncat(captured_output, args[0].as.string_val, sizeof(captured_output) - strlen(captured_output) - 1);
    } else {
        Value s = builtin_to_string(args, argc, line);
        if (s.type == VAL_STRING) {
            strncat(captured_output, s.as.string_val, sizeof(captured_output) - strlen(captured_output) - 1);
        }
        val_free(s);
    }
    strncat(captured_output, "\n", sizeof(captured_output) - strlen(captured_output) - 1);
    return val_unit();
}

static Value builtin_get_captured(Value *args, int argc, int line) {
    (void)args; (void)argc; (void)line;
    return val_string(captured_output);
}

static Value builtin_clear_captured(Value *args, int argc, int line) {
    (void)args; (void)argc; (void)line;
    captured_output[0] = '\0';
    return val_unit();
}

static Value builtin_os_name(Value *args, int argc, int line) {
    if (argc != 0) {
        fprintf(stderr, "[line %d] Runtime error: os_name expects 0 arguments\n", line);
        return val_unit();
    }
#if defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__) || defined(__MINGW32__)
    return val_string("windows");
#elif defined(__APPLE__)
    return val_string("macos");
#elif defined(__linux__)
    return val_string("linux");
#else
    return val_string("unknown");
#endif
}

static Value builtin_arch_name(Value *args, int argc, int line) {
    if (argc != 0) {
        fprintf(stderr, "[line %d] Runtime error: arch_name expects 0 arguments\n", line);
        return val_unit();
    }
#if defined(__x86_64__) || defined(_M_X64)
    return val_string("x86_64");
#elif defined(__i386__) || defined(_M_IX86)
    return val_string("x86");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return val_string("arm64");
#elif defined(__arm__) || defined(_M_ARM)
    return val_string("arm");
#else
    return val_string("unknown");
#endif
}

/* ============================================================
 * Interpreter
 * ============================================================ */
void interp_register_builtins(Interp *interp) {
    env_define(interp->globals, "os_name",    val_builtin(builtin_os_name),   false);
    env_define(interp->globals, "arch_name",  val_builtin(builtin_arch_name), false);
    env_define(interp->globals, "print",      val_builtin(builtin_print),     false);
    env_define(interp->globals, "println",    val_builtin(builtin_println),   false);
    env_define(interp->globals, "to_string",  val_builtin(builtin_to_string), false);
    env_define(interp->globals, "to_int",     val_builtin(builtin_to_int),    false);
    env_define(interp->globals, "to_float",   val_builtin(builtin_to_float),  false);
    env_define(interp->globals, "len",        val_builtin(builtin_len),       false);
    env_define(interp->globals, "type_of",    val_builtin(builtin_type_of),   false);
    env_define(interp->globals, "unwrap_or",  val_builtin(builtin_unwrap_or), false);
    env_define(interp->globals, "map",        val_builtin(builtin_map),       false);
    env_define(interp->globals, "and_then",   val_builtin(builtin_and_then),  false);
    env_define(interp->globals, "map_err",    val_builtin(builtin_map_err),   false);
    env_define(interp->globals, "push",       val_builtin(builtin_push),      false);
    env_define(interp->globals, "list_set",   val_builtin(builtin_list_set),  false);
    env_define(interp->globals, "range",      val_builtin(builtin_range),     false);
    env_define(interp->globals, "input",      val_builtin(builtin_input),     false);
    env_define(interp->globals, "substring",  val_builtin(builtin_substring), false);
    env_define(interp->globals, "file_read",  val_builtin(builtin_file_read), false);
    env_define(interp->globals, "file_write", val_builtin(builtin_file_write),false);
extern Value builtin_joey_start_gui(Value *args, int argc, int line);
    env_define(interp->globals, "joey_start_gui", val_builtin(builtin_joey_start_gui), false);
    env_define(interp->globals, "joey_run_native", val_builtin(builtin_joey_run_native), false);
    env_define(interp->globals, "exec",       val_builtin(builtin_exec),      false);
    
    env_define(interp->globals, "socket_connect", val_builtin(builtin_socket_connect), false);
    env_define(interp->globals, "socket_listen",  val_builtin(builtin_socket_listen),  false);
    env_define(interp->globals, "socket_accept",  val_builtin(builtin_socket_accept),  false);
    env_define(interp->globals, "socket_send",    val_builtin(builtin_socket_send),    false);
    env_define(interp->globals, "socket_recv",    val_builtin(builtin_socket_recv),    false);
    env_define(interp->globals, "socket_close",   val_builtin(builtin_socket_close),   false);
    env_define(interp->globals, "ffi_open",       val_builtin(builtin_ffi_open),       false);
    env_define(interp->globals, "ffi_close",      val_builtin(builtin_ffi_close),      false);
    env_define(interp->globals, "ffi_available",  val_builtin(builtin_ffi_available),  false);

    env_define(interp->globals, "channel_new",   val_builtin(builtin_channel_new),   false);
    env_define(interp->globals, "channel_send",  val_builtin(builtin_channel_send),  false);
    env_define(interp->globals, "channel_recv",  val_builtin(builtin_channel_recv),  false);
    env_define(interp->globals, "channel_close", val_builtin(builtin_channel_close), false);
    env_define(interp->globals, "thread_spawn",  val_builtin(builtin_thread_spawn),  false);
    env_define(interp->globals, "thread_join",   val_builtin(builtin_thread_join),   false);
    env_define(interp->globals, "typecheck",     val_builtin(builtin_typecheck),     false);
    env_define(interp->globals, "vm_eval",       val_builtin(builtin_vm_eval),       false);

    env_define(interp->globals, "parse_native", val_builtin(builtin_parse_native), false);
    env_define(interp->globals, "eval_native", val_builtin(builtin_eval_native), false);
    env_define(interp->globals, "serialize_ast", val_builtin(builtin_serialize_ast), false);

    env_define(interp->globals, "hash_sha256",val_builtin(builtin_hash_sha256),false);
    env_define(interp->globals, "capture_println",val_builtin(builtin_capture_println),false);
    env_define(interp->globals, "get_captured",val_builtin(builtin_get_captured),false);
        env_define(interp->globals, "set_capture_mode",val_builtin(builtin_set_capture_mode),false);
    env_define(interp->globals, "clear_captured",val_builtin(builtin_clear_captured),false);
}

void interp_init(Interp *interp) {
    interp->globals     = env_new(NULL);
    interp->current_env = interp->globals;
    interp->had_error   = false;
    interp->returning   = false;
    interp->return_val  = val_unit();
    interp->breaking    = false;
    interp->continuing  = false;
    interp->call_depth  = 0;
    interp->imported_modules = NULL;
    interp->module_count     = 0;
    interp->module_cap       = 0;
    current_interp = interp;
    interp_register_builtins(interp);
}

void interp_free(Interp *interp) {
    if (!interp) return;
    val_free(interp->return_val);
    interp->return_val = val_unit();
    if (interp->imported_modules) {
        for (int i = 0; i < interp->module_count; i++) {
            if (interp->imported_modules[i]) free(interp->imported_modules[i]);
        }
        free(interp->imported_modules);
        interp->imported_modules = NULL;
        interp->module_count = 0;
        interp->module_cap = 0;
    }
    if (interp->globals) {
        env_free(interp->globals);
        interp->globals = NULL;
    }
    interp->current_env = NULL;
}

/* ---- The main evaluation function ---- */
Value interp_exec(Interp *interp, Node *node) {
    if (!node || interp->had_error || interp->returning ||
        interp->breaking || interp->continuing)
        return val_unit();

    current_interp = interp;

    switch (node->type) {

    case NODE_INT_LIT:    return val_int(node->as.int_val);
    case NODE_FLOAT_LIT:  return val_float(node->as.float_val);
    case NODE_STRING_LIT: return val_string(node->as.string_val);
    case NODE_BOOL_LIT:   return val_bool(node->as.bool_val);

    case NODE_IDENT: {
        Value *v = env_get(interp->current_env, node->as.name);
        if (!v) {
            fprintf(stderr, "[line %d] Runtime error: Undefined variable '%s'.\n",
                    node->line, node->as.name);
            interp->had_error = true;
            return val_unit();
        }
        return val_clone(*v);
    }

    case NODE_UNARY: {
        Value operand = interp_exec(interp, node->as.unary.operand);
        if (interp->had_error) return val_unit();
        if (node->as.unary.op == TOK_MINUS) {
            if (operand.type == VAL_INT)   { if (operand.as.int_val == INT64_MIN) { fprintf(stderr, "[line %d] Runtime error: Integer overflow on negation.\n", node->line); interp->had_error = true; return val_unit(); } return val_int(-operand.as.int_val); }
            if (operand.type == VAL_FLOAT) return val_float(-operand.as.float_val);
            fprintf(stderr, "[line %d] Runtime error: Unary '-' requires Int or Float.\n", node->line);
            interp->had_error = true; return val_unit();
        }
        if (node->as.unary.op == TOK_NOT) {
            if (operand.type == VAL_BOOL) return val_bool(!operand.as.bool_val);
            fprintf(stderr, "[line %d] Runtime error: 'not' requires Bool.\n", node->line);
            interp->had_error = true; return val_unit();
        }
        return val_unit();
    }

    case NODE_BINARY: {
        /* Short-circuit for 'and'/'or' */
        if (node->as.binary.op == TOK_AND) {
            Value left = interp_exec(interp, node->as.binary.left);
            if (interp->had_error) return val_unit();
            if (left.type != VAL_BOOL) {
                fprintf(stderr, "[line %d] Runtime error: 'and' requires Bool operands.\n", node->line);
                interp->had_error = true; return val_unit();
            }
            if (!left.as.bool_val) return val_bool(false);
            Value right = interp_exec(interp, node->as.binary.right);
            if (right.type != VAL_BOOL) {
                fprintf(stderr, "[line %d] Runtime error: 'and' requires Bool operands.\n", node->line);
                interp->had_error = true; return val_unit();
            }
            return val_bool(right.as.bool_val);
        }
        if (node->as.binary.op == TOK_OR) {
            Value left = interp_exec(interp, node->as.binary.left);
            if (interp->had_error) return val_unit();
            if (left.type != VAL_BOOL) {
                fprintf(stderr, "[line %d] Runtime error: 'or' requires Bool operands.\n", node->line);
                interp->had_error = true; return val_unit();
            }
            if (left.as.bool_val) return val_bool(true);
            Value right = interp_exec(interp, node->as.binary.right);
            if (right.type != VAL_BOOL) {
                fprintf(stderr, "[line %d] Runtime error: 'or' requires Bool operands.\n", node->line);
                interp->had_error = true; return val_unit();
            }
            return val_bool(right.as.bool_val);
        }

        Value left  = interp_exec(interp, node->as.binary.left);
        Value right = interp_exec(interp, node->as.binary.right);
        if (interp->had_error) return val_unit();

        /* Equality — works on any type via structural comparison */
        if (node->as.binary.op == TOK_EQ)  { bool eq = val_equal(left, right); val_free(left); val_free(right); return val_bool(eq); }
        if (node->as.binary.op == TOK_NEQ) { bool eq = val_equal(left, right); val_free(left); val_free(right); return val_bool(!eq); }

        /* String concatenation: ++ */
        if (node->as.binary.op == TOK_CONCAT) {
            if (left.type == VAL_STRING && right.type == VAL_STRING) {
                size_t len_l = strlen(left.as.string_val);
                size_t len_r = strlen(right.as.string_val);
                size_t len = len_l + len_r;
                char *buf = qk_malloc(len + 1);
                memcpy(buf, left.as.string_val, len_l);
                memcpy(buf + len_l, right.as.string_val, len_r);
                buf[len] = '\0';
                val_free(left);
                val_free(right);
                Value v; v.type = VAL_STRING; v.as.string_val = buf;
                return v;
            }
            val_free(left);
            val_free(right);
            fprintf(stderr, "[line %d] Runtime error: '++' requires String operands (use to_string() to convert).\n", node->line);
            interp->had_error = true; return val_unit();
        }

        /* Numeric ops — same type required, no implicit coercion */
        if (left.type != right.type || (left.type != VAL_INT && left.type != VAL_FLOAT)) {
            fprintf(stderr, "[line %d] Runtime error: Numeric operations require same numeric type (got %s and %s).\n",
                    node->line, val_type_name(left), val_type_name(right));
            interp->had_error = true;
            val_free(left);
            val_free(right);
            return val_unit();
        }

        Value bin_res = val_unit();
        if (left.type == VAL_INT) {
            int64_t l = left.as.int_val, r = right.as.int_val;
            switch (node->as.binary.op) {
                case TOK_PLUS:  bin_res = val_int(l + r); break;
                case TOK_MINUS: bin_res = val_int(l - r); break;
                case TOK_STAR:  bin_res = val_int(l * r); break;
                case TOK_SLASH:
                    if (r == 0) { fprintf(stderr, "[line %d] Runtime error: Division by zero.\n", node->line); interp->had_error = true; }
                    else if (l == INT64_MIN && r == -1) { fprintf(stderr, "[line %d] Runtime error: Integer overflow on division.\n", node->line); interp->had_error = true; }
                    else { bin_res = val_int(l / r); }
                    break;
                case TOK_PERCENT:
                    if (r == 0) { fprintf(stderr, "[line %d] Runtime error: Division by zero.\n", node->line); interp->had_error = true; }
                    else if (l == INT64_MIN && r == -1) { bin_res = val_int(0); }
                    else { bin_res = val_int(l % r); }
                    break;
                case TOK_LT:  bin_res = val_bool(l < r); break;
                case TOK_GT:  bin_res = val_bool(l > r); break;
                case TOK_LEQ: bin_res = val_bool(l <= r); break;
                case TOK_GEQ: bin_res = val_bool(l >= r); break;
                default: break;
            }
        } else {
            double l = left.as.float_val, r = right.as.float_val;
            switch (node->as.binary.op) {
                case TOK_PLUS:  bin_res = val_float(l + r); break;
                case TOK_MINUS: bin_res = val_float(l - r); break;
                case TOK_STAR:  bin_res = val_float(l * r); break;
                case TOK_SLASH:
                    if (r == 0.0) { fprintf(stderr, "[line %d] Runtime error: Division by zero.\n", node->line); interp->had_error = true; }
                    else { bin_res = val_float(l / r); }
                    break;
                case TOK_PERCENT:
                    fprintf(stderr, "[line %d] Runtime error: '%%' not supported on Float.\n", node->line);
                    interp->had_error = true;
                    break;
                case TOK_LT:  bin_res = val_bool(l < r); break;
                case TOK_GT:  bin_res = val_bool(l > r); break;
                case TOK_LEQ: bin_res = val_bool(l <= r); break;
                case TOK_GEQ: bin_res = val_bool(l >= r); break;
                default: break;
            }
        }
        val_free(left);
        val_free(right);
        return bin_res;
    }

    case NODE_LET: {
        Value val = interp_exec(interp, node->as.let_bind.value);
        if (interp->had_error) return val_unit();
        env_define(interp->current_env, node->as.let_bind.name, val, node->as.let_bind.is_mut);
        val_free(val);
        return val_unit();
    }

    case NODE_ASSIGN: {
        Value val = interp_exec(interp, node->as.assign.value);
        if (interp->had_error) return val_unit();
        if (!env_set(interp->current_env, node->as.assign.name, val)) {
            fprintf(stderr, "[line %d] Runtime error: '%s' is undefined or not mutable.\n",
                    node->line, node->as.assign.name);
            interp->had_error = true;
        }
        val_free(val);
        return val_unit();
    }

    case NODE_SHADOW: {
        Value val = interp_exec(interp, node->as.shadow.value);
        if (interp->had_error) return val_unit();
        /* Shadow: redefine in current scope (env_define allows overwrite) */
        env_define(interp->current_env, node->as.shadow.name, val, false);
        val_free(val);
        return val_unit();
    }

    case NODE_IF: {
        Value cond = interp_exec(interp, node->as.if_expr.cond);
        if (interp->had_error) return val_unit();
        if (cond.type != VAL_BOOL) {
            fprintf(stderr, "[line %d] Runtime error: if condition must be Bool.\n", node->line);
            interp->had_error = true;
            val_free(cond);
            return val_unit();
        }
        bool branch = cond.as.bool_val;
        val_free(cond);
        if (branch)
            return interp_exec(interp, node->as.if_expr.then_b);
        else if (node->as.if_expr.else_b)
            return interp_exec(interp, node->as.if_expr.else_b);
        return val_unit();
    }

    case NODE_MATCH: {
        Value subject = interp_exec(interp, node->as.match_expr.subject);
        if (interp->had_error) return val_unit();
        for (int i = 0; i < node->as.match_expr.arm_count; i++) {
            MatchArm *arm = &node->as.match_expr.arms[i];
            bool matched = false;
            Value inner_val = val_unit();
            if (strcmp(arm->pattern, "Some") == 0 && subject.type == VAL_SOME) {
                matched = true; inner_val = val_clone(*subject.as.inner);
            } else if (strcmp(arm->pattern, "None") == 0 && subject.type == VAL_NONE) {
                matched = true;
            } else if (strcmp(arm->pattern, "Ok") == 0 && subject.type == VAL_OK) {
                matched = true; inner_val = val_clone(*subject.as.inner);
            } else if (strcmp(arm->pattern, "Err") == 0 && subject.type == VAL_ERR) {
                matched = true; inner_val = val_clone(*subject.as.inner);
            } else if (strcmp(arm->pattern, "_") == 0) {
                matched = true;
            }
            if (matched) {
                Env *arm_env = env_new(interp->current_env);
                if (arm->binding)
                    env_define(arm_env, arm->binding, inner_val, false);
                val_free(inner_val);
                Env *prev = interp->current_env;
                interp->current_env = arm_env;
                Value res = interp_exec(interp, arm->body);
                interp->current_env = prev;
                env_free(arm_env);
                val_free(subject);
                return res;
            }
        }
        val_free(subject);
        fprintf(stderr, "[line %d] Runtime error: Non-exhaustive match.\n", node->line);
        interp->had_error = true;
        return val_unit();
    }

    case NODE_BLOCK: {
        Env *block_env = env_new(interp->current_env);
        Env *prev = interp->current_env;
        interp->current_env = block_env;
        Value last = val_unit();
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            val_free(last);
            last = interp_exec(interp, node->as.block.stmts.items[i]);
            if (interp->had_error || interp->returning ||
                interp->breaking || interp->continuing) break;
        }
        interp->current_env = prev;
        env_free(block_env);
        return last;
    }

    case NODE_FN_DEF: {
        QkFunction fn;
        fn.name        = qk_strdup(node->as.fn_def.name);
        fn.params      = node->as.fn_def.params;
        fn.body        = node->as.fn_def.body;
        /* Use current env directly — named functions live in their defining scope,
           so they can always find themselves (recursion) and siblings. */
        fn.closure_env = interp->current_env;
        env_define(interp->current_env, fn.name, val_fn(fn), false);
        free(fn.name);
        return val_unit();
    }

    case NODE_LAMBDA: {
        QkFunction fn;
        fn.name        = NULL;
        fn.params      = node->as.lambda.params;
        fn.body        = node->as.lambda.body;
        fn.closure_env = env_snapshot(interp->current_env);
        return val_fn(fn);
    }

    case NODE_CALL: {
        Value *callee_ref = env_get(interp->current_env, node->as.call.fn_name);
        if (!callee_ref) {
            fprintf(stderr, "[line %d] Runtime error: Undefined function '%s'.\n", node->line, node->as.call.fn_name);
            interp->had_error = true; return val_unit();
        }
        Value callee = val_clone(*callee_ref);
        int argc = node->as.call.args.count;
        Value *args = malloc(sizeof(Value) * (argc > 0 ? argc : 1));
        for (int i = 0; i < argc; i++) {
            args[i] = interp_exec(interp, node->as.call.args.items[i]);
            if (interp->had_error) {
                for (int j = 0; j <= i; j++) val_free(args[j]);
                free(args);
                val_free(callee);
                return val_unit();
            }
        }
        Value res = call_value(callee, args, argc, node->line);
        for (int i = 0; i < argc; i++) val_free(args[i]);
        free(args);
        val_free(callee);
        return res;
    }

    case NODE_RETURN: {
        Value val = node->as.ret.value
                  ? interp_exec(interp, node->as.ret.value)
                  : val_unit();
        interp->returning  = true;
        interp->return_val = val;
        return val_unit();
    }

    case NODE_WHILE: {
        for (;;) {
            Value cond = interp_exec(interp, node->as.while_loop.cond);
            if (interp->had_error) break;
            if (cond.type != VAL_BOOL) {
                fprintf(stderr, "[line %d] Runtime error: while condition must be Bool.\n", node->line);
                interp->had_error = true; break;
            }
            if (!cond.as.bool_val) break;

            interp_exec(interp, node->as.while_loop.body);
            if (interp->had_error || interp->returning) break;
            if (interp->breaking)    { interp->breaking = false; break; }
            if (interp->continuing)  { interp->continuing = false; }
        }
        return val_unit();
    }

    case NODE_FOR: {
        Value iter = interp_exec(interp, node->as.for_loop.iter);
        if (interp->had_error) return val_unit();
        if (iter.type != VAL_LIST) {
            fprintf(stderr, "[line %d] Runtime error: 'for' requires a List (use range()).\n", node->line);
            interp->had_error = true;
            val_free(iter);
            return val_unit();
        }
        for (int i = 0; i < iter.as.list->count; i++) {
            Env *loop_env = env_new(interp->current_env);
            env_define(loop_env, node->as.for_loop.var_name,
                       iter.as.list->items[i], false);
            Env *prev = interp->current_env;
            interp->current_env = loop_env;

            interp_exec(interp, node->as.for_loop.body);

            interp->current_env = prev;
            env_free(loop_env);

            if (interp->had_error || interp->returning) break;
            if (interp->breaking)   { interp->breaking = false; break; }
            if (interp->continuing) { interp->continuing = false; }
        }
        val_free(iter);
        return val_unit();
    }

    case NODE_SOME: {
        Value v = interp_exec(interp, node->as.some_val.value);
        if (interp->had_error) return val_unit();
        Value res = val_some(v);
        val_free(v);
        return res;
    }

    case NODE_NONE: return val_none();

    case NODE_OK: {
        Value v = interp_exec(interp, node->as.ok_val.value);
        if (interp->had_error) return val_unit();
        Value res = val_ok(v);
        val_free(v);
        return res;
    }

    case NODE_ERR: {
        Value v = interp_exec(interp, node->as.err_val.value);
        if (interp->had_error) return val_unit();
        Value res = val_err(v);
        val_free(v);
        return res;
    }

    case NODE_UNWRAP: {
        Value v = interp_exec(interp, node->as.unwrap.operand);
        if (interp->had_error) return val_unit();
        if (v.type == VAL_SOME) {
            Value res = val_clone(*v.as.inner);
            val_free(v);
            return res;
        }
        if (v.type == VAL_NONE) {
            interp->returning  = true;
            interp->return_val = val_none();
            val_free(v);
            return val_unit();
        }
        if (v.type == VAL_OK) {
            Value res = val_clone(*v.as.inner);
            val_free(v);
            return res;
        }
        if (v.type == VAL_ERR) {
            interp->returning  = true;
            interp->return_val = val_clone(v);
            val_free(v);
            return val_unit();
        }
        fprintf(stderr, "[line %d] Runtime error: '?' requires Option or Result.\n", node->line);
        interp->had_error = true;
        val_free(v);
        return val_unit();
    }

    case NODE_BREAK:
        interp->breaking = true;
        return val_unit();

    case NODE_CONTINUE:
        interp->continuing = true;
        return val_unit();

    case NODE_PROGRAM: {
        Value last = val_unit();
        for (int i = 0; i < node->as.program.count; i++) {
            val_free(last);
            last = interp_exec(interp, node->as.program.items[i]);
            if (interp->had_error || interp->returning) break;
        }
        return last;
    }

    case NODE_LIST_LIT: {
        Value lst = val_list();
        for (int i = 0; i < node->as.list_lit.elements.count; i++) {
            Value el = interp_exec(interp, node->as.list_lit.elements.items[i]);
            if (interp->had_error) { val_free(lst); return val_unit(); }
            list_push(&lst, el);
        }
        return lst;
    }

    case NODE_INDEX: {
        Value arr = interp_exec(interp, node->as.index.array);
        if (interp->had_error) return val_unit();
        if (arr.type != VAL_LIST && arr.type != VAL_STRING) {
            fprintf(stderr, "[line %d] Runtime error: Can only index lists or strings.\n", node->line);
            interp->had_error = true;
            val_free(arr);
            return val_unit();
        }
        Value idx = interp_exec(interp, node->as.index.index);
        if (interp->had_error) {
            val_free(arr);
            return val_unit();
        }
        if (idx.type != VAL_INT) {
            fprintf(stderr, "[line %d] Runtime error: Index must be an integer.\n", node->line);
            interp->had_error = true;
            val_free(arr);
            val_free(idx);
            return val_unit();
        }
        int64_t i_val = idx.as.int_val;
        Value res = val_unit();
        if (arr.type == VAL_LIST) {
            if (i_val < 0 || i_val >= arr.as.list->count) {
                fprintf(stderr, "[line %d] Runtime error: Index out of bounds.\n", node->line);
                interp->had_error = true;
            } else {
                res = val_clone(arr.as.list->items[(size_t)i_val]);
            }
        } else {
            int64_t len = (int64_t)strlen(arr.as.string_val);
            if (i_val < 0 || i_val >= len) {
                fprintf(stderr, "[line %d] Runtime error: String index out of bounds.\n", node->line);
                interp->had_error = true;
            } else {
                char buf[2] = { arr.as.string_val[(size_t)i_val], '\0' };
                res = val_string(buf);
            }
        }
        val_free(arr);
        val_free(idx);
        return res;
    }

    case NODE_IMPORT: {
        const char *path = node->as.import_stmt.path;
        if (!path) return val_unit();
        
        for (int i = 0; i < interp->module_count; i++) {
            if (strcmp(interp->imported_modules[i], path) == 0) {
                return val_unit();
            }
        }
        
        char candidate[512];
        const char *found_path = NULL;
        FILE *tf = fopen(path, "rb");
        if (tf) { fclose(tf); found_path = path; }
        else {
            snprintf(candidate, sizeof(candidate), "%s.qk", path);
            tf = fopen(candidate, "rb");
            if (tf) { fclose(tf); found_path = candidate; }
            else {
                snprintf(candidate, sizeof(candidate), "src/std/%s.qk", path);
                tf = fopen(candidate, "rb");
                if (tf) { fclose(tf); found_path = candidate; }
                else {
                    snprintf(candidate, sizeof(candidate), "src/std/%s.qka", path);
                    tf = fopen(candidate, "rb");
                    if (tf) { fclose(tf); found_path = candidate; }
                }
            }
        }
        if (!found_path) {
            fprintf(stderr, "[line %d] Runtime error: Could not find module '%s'.\n", node->line, path);
            interp->had_error = true;
            return val_unit();
        }

        if (interp->module_count >= interp->module_cap) {
            interp->module_cap = interp->module_cap < 8 ? 8 : interp->module_cap * 2;
            interp->imported_modules = realloc(interp->imported_modules, sizeof(char*) * interp->module_cap);
            if (!interp->imported_modules) qk_fatal("Out of memory in module cache");
        }
        interp->imported_modules[interp->module_count++] = qk_strdup(path);

        FILE *mf = fopen(found_path, "rb");
        if (!mf) {
            fprintf(stderr, "[line %d] Runtime error: Could not open module '%s'.\n", node->line, found_path);
            interp->had_error = true;
            return val_unit();
        }
        fseek(mf, 0, SEEK_END);
        long sz = ftell(mf);
        fseek(mf, 0, SEEK_SET);
        if (sz < 0) { fclose(mf); return val_unit(); }
        char *src = qk_malloc((size_t)sz + 1);
        size_t rd = fread(src, 1, (size_t)sz, mf);
        src[rd] = '\0';
        fclose(mf);

        Parser mp;
        parser_init(&mp, src);
        Node *m_ast = parser_parse(&mp);
        free(src);
        if (!m_ast || mp.had_error) {
            fprintf(stderr, "[line %d] Runtime error: Failed to parse module '%s'.\n", node->line, found_path);
            interp->had_error = true;
            return val_unit();
        }
        interp_exec(interp, m_ast);
        return val_unit();
    }

    case NODE_STRUCT_DEF: {
        QkFunction fn;
        fn.name = qk_strdup(node->as.struct_def.name);
        fn.params = node->as.struct_def.fields;
        
        Node *ctor = node_new(NODE_STRUCT_CONSTRUCT, node->line);
        ctor->as.struct_construct.name = qk_strdup(node->as.struct_def.name);
        ctor->as.struct_construct.fields = node->as.struct_def.fields;
        qk_ast_track(ctor);
        
        fn.body = ctor;
        fn.closure_env = interp->current_env;
        env_define(interp->current_env, fn.name, val_fn(fn), false);
        free(fn.name);
        return val_unit();
    }

    case NODE_STRUCT_CONSTRUCT: {
        int count = node->as.struct_construct.fields.count;
        char **names = qk_malloc(sizeof(char*) * (count > 0 ? count : 1));
        Value *values = qk_malloc(sizeof(Value) * (count > 0 ? count : 1));
        for (int i = 0; i < count; i++) {
            names[i] = node->as.struct_construct.fields.items[i].name;
            Value *val_ptr = env_get(interp->current_env, names[i]);
            values[i] = val_ptr ? *val_ptr : val_unit();
        }
        Value inst = val_struct(node->as.struct_construct.name, count, names, values);
        free(names);
        free(values);
        return inst;
    }

    case NODE_GET_FIELD: {
        Value obj = interp_exec(interp, node->as.get_field.object);
        if (interp->had_error) return val_unit();
        if (obj.type != VAL_STRUCT) {
            fprintf(stderr, "[line %d] Runtime error: Cannot access property '%s' on %s.\n",
                    node->line, node->as.get_field.field, val_type_name(obj));
            interp->had_error = true;
            val_free(obj);
            return val_unit();
        }
        QkStructInstance *s = obj.as.struct_val;
        for (int i = 0; i < s->field_count; i++) {
            if (strcmp(s->field_names[i], node->as.get_field.field) == 0) {
                Value res = val_clone(s->field_values[i]);
                val_free(obj);
                return res;
            }
        }
        fprintf(stderr, "[line %d] Runtime error: Field '%s' not found on struct '%s'.\n",
                node->line, node->as.get_field.field, s->name);
        interp->had_error = true;
        val_free(obj);
        return val_unit();
    }

    case NODE_SET_FIELD: {
        Value obj = interp_exec(interp, node->as.set_field.object);
        if (interp->had_error) return val_unit();
        if (obj.type != VAL_STRUCT) {
            fprintf(stderr, "[line %d] Runtime error: Cannot set property '%s' on %s.\n",
                    node->line, node->as.set_field.field, val_type_name(obj));
            interp->had_error = true;
            val_free(obj);
            return val_unit();
        }
        Value val = interp_exec(interp, node->as.set_field.value);
        if (interp->had_error) {
            val_free(obj);
            return val_unit();
        }
        QkStructInstance *s = obj.as.struct_val;
        bool found = false;
        for (int i = 0; i < s->field_count; i++) {
            if (strcmp(s->field_names[i], node->as.set_field.field) == 0) {
                val_free(s->field_values[i]);
                s->field_values[i] = val_clone(val);
                found = true;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "[line %d] Runtime error: Field '%s' not found on struct '%s'.\n",
                    node->line, node->as.set_field.field, s->name);
            interp->had_error = true;
        }
        val_free(obj);
        val_free(val);
        return val_unit();
    }

    default:
        break;
    }
    return val_unit();
}
