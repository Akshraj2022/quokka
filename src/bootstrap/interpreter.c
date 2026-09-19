#include "quokka.h"

/* Forward declaration — used by builtins that need to call QkFunctions */
static Interp *current_interp = NULL;

/* Helper: call a function or builtin value */
static Value call_value(Value callee, Value *args, int argc, int line) {
    if (callee.type == VAL_BUILTIN) {
        return callee.as.builtin(args, argc, line);
    } else if (callee.type == VAL_FN) {
        QkFunction *fn = &callee.as.fn;
        if (argc != fn->params.count) {
            fprintf(stderr, "[line %d] Runtime error: Expected %d arguments but got %d.\n",
                    line, fn->params.count, argc);
            current_interp->had_error = true;
            return val_unit();
        }
        /* Lexical scoping: new env's parent is the closure env, NOT the call-site env */
        Env *call_env = env_new(fn->closure_env);
        for (int i = 0; i < argc; i++) {
            env_define(call_env, fn->params.items[i].name, val_clone(args[i]), false);
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
        return result;
    } else {
        fprintf(stderr, "[line %d] Runtime error: Not a function.\n", line);
        current_interp->had_error = true;
        return val_unit();
    }
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
    val.as.string_val = qk_strdup(s);
    return val;
}

Value val_none(void) { Value val; val.type = VAL_NONE; return val; }

Value val_some(Value inner) {
    Value val; val.type = VAL_SOME;
    val.as.inner = malloc(sizeof(Value));
    *val.as.inner = val_clone(inner);
    return val;
}

Value val_ok(Value inner) {
    Value val; val.type = VAL_OK;
    val.as.inner = malloc(sizeof(Value));
    *val.as.inner = val_clone(inner);
    return val;
}

Value val_err(Value inner) {
    Value val; val.type = VAL_ERR;
    val.as.inner = malloc(sizeof(Value));
    *val.as.inner = val_clone(inner);
    return val;
}

Value val_fn(QkFunction f)    { Value val; val.type = VAL_FN;      val.as.fn = f;      return val; }
Value val_builtin(BuiltinFn f){ Value val; val.type = VAL_BUILTIN; val.as.builtin = f; return val; }

Value val_list(void) {
    Value val; val.type = VAL_LIST;
    val.as.list = malloc(sizeof(QkList));
    val.as.list->items = NULL;
    val.as.list->count = 0;
    val.as.list->cap   = 0;
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
        default:          return "Unknown";
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
        case VAL_LIST: {
            printf("[");
            for (int i = 0; i < v.as.list->count; i++) {
                val_print(v.as.list->items[i]);
                if (i < v.as.list->count - 1) printf(", ");
            }
            printf("]");
            break;
        }
    }
}

bool val_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_INT:     return a.as.int_val == b.as.int_val;
        case VAL_FLOAT:   return a.as.float_val == b.as.float_val;
        case VAL_BOOL:    return a.as.bool_val == b.as.bool_val;
        case VAL_STRING:  return strcmp(a.as.string_val, b.as.string_val) == 0;
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
            /* Share closure env pointer — avoid infinite deep-copy */
            return nv;
        }
        case VAL_LIST:
            return v;
        default: return v;
    }
}

void list_push(Value *list, Value item) {
    if (list->type != VAL_LIST) return;
    QkList *l = list->as.list;
    if (l->count >= l->cap) {
        l->cap = l->cap == 0 ? 8 : l->cap * 2;
        l->items = realloc(l->items, sizeof(Value) * l->cap);
    }
    l->items[l->count++] = item;
}

/* ============================================================
 * Environment
 * ============================================================ */
Env *env_new(Env *parent) {
    Env *env = malloc(sizeof(Env));
    env->parent   = parent;
    env->bindings = NULL;
    return env;
}

void env_define(Env *env, const char *name, Value val, bool is_mut) {
    /* Check for existing binding in THIS scope (not parents) */
    Binding *curr = env->bindings;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            /* Shadow: allow re-definition in same scope (used by NODE_SHADOW) */
            curr->value  = val_clone(val);
            curr->is_mut = is_mut;
            return;
        }
        curr = curr->next;
    }
    Binding *b = malloc(sizeof(Binding));
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
    Env *new_env = malloc(sizeof(Env));
    new_env->parent   = env_snapshot(env->parent);
    new_env->bindings = NULL;

    Binding *curr = env->bindings;
    Binding *last = NULL;
    while (curr) {
        Binding *b = malloc(sizeof(Binding));
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
    Binding *curr = env->bindings;
    while (curr) {
        Binding *next = curr->next;
        free(curr->name);
        free(curr);
        curr = next;
    }
    free(env);
}

/* ============================================================
 * Built-in functions
 * ============================================================ */
static Value builtin_print(Value *args, int argc, int line) {
    (void)line;
    for (int i = 0; i < argc; i++) val_print(args[i]);
    printf("\n");
    return val_unit();
}

static Value builtin_println(Value *args, int argc, int line) {
    return builtin_print(args, argc, line);
}

static Value builtin_to_string(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1) return val_string("");
    char buf[256];
    switch (args[0].type) {
        case VAL_INT:    snprintf(buf, sizeof(buf), "%lld", (long long)args[0].as.int_val); return val_string(buf);
        case VAL_FLOAT:  snprintf(buf, sizeof(buf), "%g", args[0].as.float_val);            return val_string(buf);
        case VAL_BOOL:   return val_string(args[0].as.bool_val ? "true" : "false");
        case VAL_STRING: return val_clone(args[0]);
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
        return val_some(res);
    } else if (args[0].type == VAL_NONE) {
        return val_none();
    } else if (args[0].type == VAL_OK) {
        Value inner = *args[0].as.inner;
        Value res = call_value(args[1], &inner, 1, line);
        return val_ok(res);
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
    }
    return val_unit();
}

static Value builtin_map_err(Value *args, int argc, int line) {
    if (argc != 2) return val_unit();
    if (args[0].type == VAL_ERR) {
        Value inner = *args[0].as.inner;
        Value res = call_value(args[1], &inner, 1, line);
        return val_err(res);
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
        buf[strcspn(buf, "\n")] = 0;
        return val_string(buf);
    }
    return val_string("");
}

static Value builtin_substring(Value *args, int argc, int line) {
    (void)line;
    if (argc != 3 || args[0].type != VAL_STRING || args[1].type != VAL_INT || args[2].type != VAL_INT)
        return val_string("");
    int start = (int)args[1].as.int_val;
    int end = (int)args[2].as.int_val;
    int len = strlen(args[0].as.string_val);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return val_string("");
    char *buf = malloc(end - start + 1);
    memcpy(buf, args[0].as.string_val + start, end - start);
    buf[end - start] = '\0';
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
    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);
    string[fsize] = 0;
    Value res = val_string(string);
    free(string);
    return val_ok(res);
}

static Value builtin_file_write(Value *args, int argc, int line) {
    (void)line;
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return val_err(val_string("Expected (path, content)"));
    FILE *f = fopen(args[0].as.string_val, "wb");
    if (!f) return val_err(val_string("Cannot open file"));
    fwrite(args[1].as.string_val, 1, strlen(args[1].as.string_val), f);
    fclose(f);
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
    return val_ok(list);
}

static Value builtin_exec(Value *args, int argc, int line) {
    (void)line;
    if (argc != 1 || args[0].type != VAL_LIST) return val_err(val_string("Expected list of string args"));
    
    // Naive concatenation for demonstration, but user requested no string concat for execution...
    // Actually, in C we should use _spawnvp or similar for safe execution, but for simplicity in Windows stage-0:
    char cmd[1024] = {0};
    for (int i = 0; i < args[0].as.list->count; i++) {
        if (args[0].as.list->items[i].type != VAL_STRING) return val_err(val_string("Args must be strings"));
        strcat(cmd, "\"");
        strcat(cmd, args[0].as.list->items[i].as.string_val);
        strcat(cmd, "\" ");
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
        if (fsize > 0) fread(out_str, fsize, 1, f);
        out_str[fsize] = 0;
        fclose(f);
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
    
    return val_ok(map);
}

static Value builtin_list_set(Value *args, int argc, int line) {
    (void)line;
    if (argc != 3 || args[0].type != VAL_LIST || args[1].type != VAL_INT)
        return val_unit();
    int idx = (int)args[1].as.int_val;
    if (idx >= 0 && idx < args[0].as.list->count) {
        args[0].as.list->items[idx] = val_clone(args[2]);
    }
    return val_unit();
}


/* ============================================================
 * Interpreter
 * ============================================================ */
void interp_register_builtins(Interp *interp) {
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
    env_define(interp->globals, "joey_run_native", val_builtin(builtin_joey_run_native), false);
    env_define(interp->globals, "exec",       val_builtin(builtin_exec),      false);


}

void interp_init(Interp *interp) {
    interp->globals     = env_new(NULL);
    interp->current_env = interp->globals;
    interp->had_error   = false;
    interp->returning   = false;
    interp->return_val  = val_unit();
    interp->breaking    = false;
    interp->continuing  = false;
    current_interp = interp;
    interp_register_builtins(interp);
}

void interp_free(Interp *interp) {
    /* Don't free globals in REPL mode — only on full shutdown */
    (void)interp;
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
            if (operand.type == VAL_INT)   return val_int(-operand.as.int_val);
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
        if (node->as.binary.op == TOK_EQ)  return val_bool(val_equal(left, right));
        if (node->as.binary.op == TOK_NEQ) return val_bool(!val_equal(left, right));

        /* String concatenation: ++ */
        if (node->as.binary.op == TOK_CONCAT) {
            if (left.type == VAL_STRING && right.type == VAL_STRING) {
                size_t len = strlen(left.as.string_val) + strlen(right.as.string_val);
                char *buf = malloc(len + 1);
                strcpy(buf, left.as.string_val);
                strcat(buf, right.as.string_val);
                Value v; v.type = VAL_STRING; v.as.string_val = buf;
                return v;
            }
            fprintf(stderr, "[line %d] Runtime error: '++' requires String operands (use to_string() to convert).\n", node->line);
            interp->had_error = true; return val_unit();
        }

        /* Numeric ops — same type required, no implicit coercion */
        if (left.type != right.type || (left.type != VAL_INT && left.type != VAL_FLOAT)) {
            fprintf(stderr, "[line %d] Runtime error: Numeric operations require same numeric type (got %s and %s).\n",
                    node->line, val_type_name(left), val_type_name(right));
            interp->had_error = true; return val_unit();
        }

        if (left.type == VAL_INT) {
            int64_t l = left.as.int_val, r = right.as.int_val;
            switch (node->as.binary.op) {
                case TOK_PLUS:  return val_int(l + r);
                case TOK_MINUS: return val_int(l - r);
                case TOK_STAR:  return val_int(l * r);
                case TOK_SLASH:
                    if (r == 0) { fprintf(stderr, "[line %d] Runtime error: Division by zero.\n", node->line); interp->had_error = true; return val_unit(); }
                    return val_int(l / r);
                case TOK_PERCENT:
                    if (r == 0) { fprintf(stderr, "[line %d] Runtime error: Division by zero.\n", node->line); interp->had_error = true; return val_unit(); }
                    return val_int(l % r);
                case TOK_LT:  return val_bool(l < r);
                case TOK_GT:  return val_bool(l > r);
                case TOK_LEQ: return val_bool(l <= r);
                case TOK_GEQ: return val_bool(l >= r);
                default: break;
            }
        } else {
            double l = left.as.float_val, r = right.as.float_val;
            switch (node->as.binary.op) {
                case TOK_PLUS:  return val_float(l + r);
                case TOK_MINUS: return val_float(l - r);
                case TOK_STAR:  return val_float(l * r);
                case TOK_SLASH:
                    if (r == 0.0) { fprintf(stderr, "[line %d] Runtime error: Division by zero.\n", node->line); interp->had_error = true; return val_unit(); }
                    return val_float(l / r);
                case TOK_PERCENT:
                    fprintf(stderr, "[line %d] Runtime error: '%%' not supported on Float.\n", node->line);
                    interp->had_error = true; return val_unit();
                case TOK_LT:  return val_bool(l < r);
                case TOK_GT:  return val_bool(l > r);
                case TOK_LEQ: return val_bool(l <= r);
                case TOK_GEQ: return val_bool(l >= r);
                default: break;
            }
        }
        return val_unit();
    }

    case NODE_LET: {
        Value val = interp_exec(interp, node->as.let_bind.value);
        if (interp->had_error) return val_unit();
        env_define(interp->current_env, node->as.let_bind.name, val, node->as.let_bind.is_mut);
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
        return val_unit();
    }

    case NODE_SHADOW: {
        Value val = interp_exec(interp, node->as.shadow.value);
        if (interp->had_error) return val_unit();
        /* Shadow: redefine in current scope (env_define allows overwrite) */
        env_define(interp->current_env, node->as.shadow.name, val, false);
        return val_unit();
    }

    case NODE_IF: {
        Value cond = interp_exec(interp, node->as.if_expr.cond);
        if (interp->had_error) return val_unit();
        if (cond.type != VAL_BOOL) {
            fprintf(stderr, "[line %d] Runtime error: if condition must be Bool.\n", node->line);
            interp->had_error = true; return val_unit();
        }
        if (cond.as.bool_val)
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
                Env *prev = interp->current_env;
                interp->current_env = arm_env;
                Value res = interp_exec(interp, arm->body);
                interp->current_env = prev;
                env_free(arm_env);
                return res;
            }
        }
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
            fprintf(stderr, "[line %d] Runtime error: Undefined function '%s' (%d %d %d %d).\n", node->line, node->as.call.fn_name, node->as.call.fn_name[0], node->as.call.fn_name[1], node->as.call.fn_name[2], node->as.call.fn_name[3]);
            interp->had_error = true; return val_unit();
        }
        Value callee = val_clone(*callee_ref);
        int argc = node->as.call.args.count;
        Value *args = malloc(sizeof(Value) * (argc > 0 ? argc : 1));
        for (int i = 0; i < argc; i++) {
            args[i] = interp_exec(interp, node->as.call.args.items[i]);
            if (interp->had_error) { free(args); return val_unit(); }
        }
        Value res = call_value(callee, args, argc, node->line);
        free(args);
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
            interp->had_error = true; return val_unit();
        }
        for (int i = 0; i < iter.as.list->count; i++) {
            Env *loop_env = env_new(interp->current_env);
            env_define(loop_env, node->as.for_loop.var_name,
                       val_clone(iter.as.list->items[i]), false);
            Env *prev = interp->current_env;
            interp->current_env = loop_env;

            interp_exec(interp, node->as.for_loop.body);

            interp->current_env = prev;
            env_free(loop_env);

            if (interp->had_error || interp->returning) break;
            if (interp->breaking)   { interp->breaking = false; break; }
            if (interp->continuing) { interp->continuing = false; }
        }
        return val_unit();
    }

    case NODE_SOME: {
        Value v = interp_exec(interp, node->as.some_val.value);
        return interp->had_error ? val_unit() : val_some(v);
    }

    case NODE_NONE: return val_none();

    case NODE_OK: {
        Value v = interp_exec(interp, node->as.ok_val.value);
        return interp->had_error ? val_unit() : val_ok(v);
    }

    case NODE_ERR: {
        Value v = interp_exec(interp, node->as.err_val.value);
        return interp->had_error ? val_unit() : val_err(v);
    }

    case NODE_UNWRAP: {
        Value v = interp_exec(interp, node->as.unwrap.operand);
        if (interp->had_error) return val_unit();
        if (v.type == VAL_SOME) return val_clone(*v.as.inner);
        if (v.type == VAL_NONE) {
            interp->returning  = true;
            interp->return_val = val_none();
            return val_unit();
        }
        if (v.type == VAL_OK) return val_clone(*v.as.inner);
        if (v.type == VAL_ERR) {
            interp->returning  = true;
            interp->return_val = val_clone(v);
            return val_unit();
        }
        fprintf(stderr, "[line %d] Runtime error: '?' requires Option or Result.\n", node->line);
        interp->had_error = true;
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
            last = interp_exec(interp, node->as.program.items[i]);
            if (interp->had_error || interp->returning) break;
        }
        return last;
    }

    case NODE_LIST_LIT: {
        Value lst = val_list();
        for (int i = 0; i < node->as.list_lit.elements.count; i++) {
            Value el = interp_exec(interp, node->as.list_lit.elements.items[i]);
            if (interp->had_error) return val_unit();
            list_push(&lst, el);
        }
        return lst;
    }

    case NODE_INDEX: {
        Value arr = interp_exec(interp, node->as.index.array);
        if (interp->had_error) return val_unit();
        if (arr.type != VAL_LIST && arr.type != VAL_STRING) {
            fprintf(stderr, "[line %d] Runtime error: Can only index lists or strings.\n", node->line);
            interp->had_error = true; return val_unit();
        }
        Value idx = interp_exec(interp, node->as.index.index);
        if (interp->had_error) return val_unit();
        if (idx.type != VAL_INT) {
            fprintf(stderr, "[line %d] Runtime error: Index must be an integer.\n", node->line);
            interp->had_error = true; return val_unit();
        }
        int i = (int)idx.as.int_val;
        if (arr.type == VAL_LIST) {
            if (i < 0 || i >= arr.as.list->count) {
                fprintf(stderr, "[line %d] Runtime error: Index out of bounds.\n", node->line);
                interp->had_error = true; return val_unit();
            }
            return val_clone(arr.as.list->items[i]);
        } else {
            int len = strlen(arr.as.string_val);
            if (i < 0 || i >= len) {
                fprintf(stderr, "[line %d] Runtime error: String index out of bounds.\n", node->line);
                interp->had_error = true; return val_unit();
            }
            char buf[2] = { arr.as.string_val[i], '\0' };
            return val_string(buf);
        }
    }

    default:
        break;
    }
    return val_unit();
}
