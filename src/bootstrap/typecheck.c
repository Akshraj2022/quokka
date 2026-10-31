#include "typecheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

QkType *qk_type_new(QkTypeTag tag) {
    QkType *t = (QkType *)qk_malloc(sizeof(QkType));
    t->tag = tag;
    t->inner = NULL;
    t->err_type = NULL;
    t->name = NULL;
    t->field_count = 0;
    t->field_names = NULL;
    t->field_types = NULL;
    t->return_type = NULL;
    t->refcount = 1;
    return t;
}

QkType *qk_type_int(void)    { return qk_type_new(QK_TYPE_INT); }
QkType *qk_type_float(void)  { return qk_type_new(QK_TYPE_FLOAT); }
QkType *qk_type_bool(void)   { return qk_type_new(QK_TYPE_BOOL); }
QkType *qk_type_string(void) { return qk_type_new(QK_TYPE_STRING); }
QkType *qk_type_unit(void)   { return qk_type_new(QK_TYPE_UNIT); }
QkType *qk_type_any(void)    { return qk_type_new(QK_TYPE_ANY); }
QkType *qk_type_thread(void) { return qk_type_new(QK_TYPE_THREAD); }

QkType *qk_type_option(QkType *inner) {
    QkType *t = qk_type_new(QK_TYPE_OPTION);
    t->inner = inner ? qk_type_clone(inner) : qk_type_any();
    return t;
}

QkType *qk_type_result(QkType *ok_t, QkType *err_t) {
    QkType *t = qk_type_new(QK_TYPE_RESULT);
    t->inner = ok_t ? qk_type_clone(ok_t) : qk_type_any();
    t->err_type = err_t ? qk_type_clone(err_t) : qk_type_any();
    return t;
}

QkType *qk_type_list(QkType *elem_t) {
    QkType *t = qk_type_new(QK_TYPE_LIST);
    t->inner = elem_t ? qk_type_clone(elem_t) : qk_type_any();
    return t;
}

QkType *qk_type_channel(QkType *elem_t) {
    QkType *t = qk_type_new(QK_TYPE_CHANNEL);
    t->inner = elem_t ? qk_type_clone(elem_t) : qk_type_any();
    return t;
}

QkType *qk_type_struct(const char *name, int count, char **fields, QkType **types) {
    QkType *t = qk_type_new(QK_TYPE_STRUCT);
    t->name = qk_strdup(name ? name : "Struct");
    t->field_count = count;
    if (count > 0) {
        t->field_names = (char **)qk_malloc(sizeof(char *) * count);
        t->field_types = (QkType **)qk_malloc(sizeof(QkType *) * count);
        for (int i = 0; i < count; i++) {
            t->field_names[i] = qk_strdup(fields[i]);
            t->field_types[i] = types && types[i] ? qk_type_clone(types[i]) : qk_type_any();
        }
    }
    return t;
}

QkType *qk_type_fn(int param_count, QkType **params, QkType *ret) {
    QkType *t = qk_type_new(QK_TYPE_FN);
    t->field_count = param_count;
    if (param_count > 0 && params) {
        t->field_types = (QkType **)qk_malloc(sizeof(QkType *) * param_count);
        for (int i = 0; i < param_count; i++) {
            t->field_types[i] = qk_type_clone(params[i]);
        }
    }
    t->return_type = ret ? qk_type_clone(ret) : qk_type_any();
    return t;
}

QkType *qk_type_clone(QkType *t) {
    if (!t) return NULL;
    t->refcount++;
    return t;
}

void qk_type_free(QkType *t) {
    if (!t) return;
    t->refcount--;
    if (t->refcount > 0) return;

    if (t->inner) qk_type_free(t->inner);
    if (t->err_type) qk_type_free(t->err_type);
    if (t->return_type) qk_type_free(t->return_type);
    if (t->name) free(t->name);

    if (t->field_names) {
        for (int i = 0; i < t->field_count; i++) {
            if (t->field_names[i]) free(t->field_names[i]);
        }
        free(t->field_names);
    }
    if (t->field_types) {
        for (int i = 0; i < t->field_count; i++) {
            if (t->field_types[i]) qk_type_free(t->field_types[i]);
        }
        free(t->field_types);
    }
    free(t);
}

bool qk_type_compatible(QkType *expected, QkType *actual) {
    if (!expected || !actual) return true;
    if (expected->tag == QK_TYPE_ANY || actual->tag == QK_TYPE_ANY) return true;
    if (expected->tag != actual->tag) {
        if (expected->tag == QK_TYPE_FLOAT && actual->tag == QK_TYPE_INT) return true;
        return false;
    }
    if (expected->tag == QK_TYPE_OPTION || expected->tag == QK_TYPE_LIST || expected->tag == QK_TYPE_CHANNEL) {
        return qk_type_compatible(expected->inner, actual->inner);
    }
    if (expected->tag == QK_TYPE_RESULT) {
        return qk_type_compatible(expected->inner, actual->inner) &&
               qk_type_compatible(expected->err_type, actual->err_type);
    }
    if (expected->tag == QK_TYPE_STRUCT) {
        if (expected->name && actual->name) {
            return strcmp(expected->name, actual->name) == 0;
        }
        return true;
    }
    return true;
}

const char *qk_type_to_string(QkType *t) {
    if (!t) return "Unknown";
    switch (t->tag) {
        case QK_TYPE_ANY:     return "Any";
        case QK_TYPE_INT:     return "Int";
        case QK_TYPE_FLOAT:   return "Float";
        case QK_TYPE_BOOL:    return "Bool";
        case QK_TYPE_STRING:  return "String";
        case QK_TYPE_UNIT:    return "Unit";
        case QK_TYPE_OPTION:  return "Option";
        case QK_TYPE_RESULT:  return "Result";
        case QK_TYPE_LIST:    return "List";
        case QK_TYPE_STRUCT:  return t->name ? t->name : "Struct";
        case QK_TYPE_FN:      return "Function";
        case QK_TYPE_CHANNEL: return "Channel";
        case QK_TYPE_THREAD:  return "Thread";
        case QK_TYPE_ERROR:   return "Error";
        default:              return "Unknown";
    }
}

QkTypeEnv *qk_type_env_new(QkTypeEnv *parent) {
    QkTypeEnv *env = (QkTypeEnv *)qk_malloc(sizeof(QkTypeEnv));
    env->bindings = NULL;
    env->structs = NULL;
    env->parent = parent;
    return env;
}

void qk_type_env_free(QkTypeEnv *env) {
    if (!env) return;
    QkTypeBinding *b = env->bindings;
    while (b) {
        QkTypeBinding *next = b->next;
        if (b->name) free(b->name);
        qk_type_free(b->type);
        free(b);
        b = next;
    }
    QkStructTypeEntry *s = env->structs;
    while (s) {
        QkStructTypeEntry *next = s->next;
        if (s->name) free(s->name);
        for (int i = 0; i < s->field_count; i++) {
            if (s->field_names && s->field_names[i]) free(s->field_names[i]);
            if (s->field_types && s->field_types[i]) qk_type_free(s->field_types[i]);
        }
        if (s->field_names) free(s->field_names);
        if (s->field_types) free(s->field_types);
        free(s);
        s = next;
    }
    free(env);
}

bool qk_type_env_define(QkTypeEnv *env, const char *name, QkType *type, bool is_mut) {
    QkTypeBinding *curr = env->bindings;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            qk_type_free(curr->type);
            curr->type = type;
            curr->is_mut = is_mut;
            return true;
        }
        curr = curr->next;
    }
    QkTypeBinding *b = (QkTypeBinding *)qk_malloc(sizeof(QkTypeBinding));
    b->name = qk_strdup(name);
    b->type = type;
    b->is_mut = is_mut;
    b->next = env->bindings;
    env->bindings = b;
    return true;
}

QkType *qk_type_env_lookup(QkTypeEnv *env, const char *name) {
    while (env) {
        QkTypeBinding *curr = env->bindings;
        while (curr) {
            if (strcmp(curr->name, name) == 0) return curr->type;
            curr = curr->next;
        }
        env = env->parent;
    }
    return NULL;
}

void qk_type_env_register_struct(QkTypeEnv *env, const char *name, int count, char **fields) {
    QkStructTypeEntry *s = (QkStructTypeEntry *)qk_malloc(sizeof(QkStructTypeEntry));
    s->name = qk_strdup(name);
    s->field_count = count;
    s->field_names = (char **)qk_malloc(sizeof(char *) * (count > 0 ? count : 1));
    s->field_types = (QkType **)qk_malloc(sizeof(QkType *) * (count > 0 ? count : 1));
    for (int i = 0; i < count; i++) {
        s->field_names[i] = qk_strdup(fields[i]);
        s->field_types[i] = qk_type_any();
    }
    s->next = env->structs;
    env->structs = s;
}

QkStructTypeEntry *qk_type_env_find_struct(QkTypeEnv *env, const char *name) {
    while (env) {
        QkStructTypeEntry *curr = env->structs;
        while (curr) {
            if (strcmp(curr->name, name) == 0) return curr;
            curr = curr->next;
        }
        env = env->parent;
    }
    return NULL;
}

static void type_error(int line, int *error_count, const char *msg) {
    fprintf(stderr, "[line %d] Type error: %s\n", line, msg);
    if (error_count) (*error_count)++;
}

QkType *qk_typecheck_node(Node *node, QkTypeEnv *env, int *error_count) {
    if (!node) return qk_type_unit();

    switch (node->type) {
        case NODE_INT_LIT:    return qk_type_int();
        case NODE_FLOAT_LIT:  return qk_type_float();
        case NODE_BOOL_LIT:   return qk_type_bool();
        case NODE_STRING_LIT: return qk_type_string();

        case NODE_IDENT: {
            QkType *t = qk_type_env_lookup(env, node->as.name);
            if (!t) return qk_type_any();
            return qk_type_clone(t);
        }

        case NODE_UNARY: {
            QkType *op_t = qk_typecheck_node(node->as.unary.operand, env, error_count);
            if (node->as.unary.op == TOK_MINUS) {
                if (op_t->tag != QK_TYPE_INT && op_t->tag != QK_TYPE_FLOAT && op_t->tag != QK_TYPE_ANY) {
                    type_error(node->line, error_count, "Unary minus expects numeric type");
                }
                return op_t;
            } else if (node->as.unary.op == TOK_NOT) {
                if (op_t->tag != QK_TYPE_BOOL && op_t->tag != QK_TYPE_ANY) {
                    type_error(node->line, error_count, "Unary 'not' expects Bool");
                }
                qk_type_free(op_t);
                return qk_type_bool();
            }
            return op_t;
        }

        case NODE_BINARY: {
            QkType *left = qk_typecheck_node(node->as.binary.left, env, error_count);
            QkType *right = qk_typecheck_node(node->as.binary.right, env, error_count);
            QkTokenType op = node->as.binary.op;

            if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH || op == TOK_PERCENT) {
                if (left->tag == QK_TYPE_STRING && right->tag == QK_TYPE_STRING && op == TOK_PLUS) {
                    qk_type_free(left); qk_type_free(right);
                    return qk_type_string();
                }
                if ((left->tag != QK_TYPE_INT && left->tag != QK_TYPE_FLOAT && left->tag != QK_TYPE_ANY) ||
                    (right->tag != QK_TYPE_INT && right->tag != QK_TYPE_FLOAT && right->tag != QK_TYPE_ANY)) {
                    type_error(node->line, error_count, "Arithmetic operator requires numeric operands");
                }
                QkTypeTag res_tag = (left->tag == QK_TYPE_FLOAT || right->tag == QK_TYPE_FLOAT) ? QK_TYPE_FLOAT : QK_TYPE_INT;
                qk_type_free(left); qk_type_free(right);
                return qk_type_new(res_tag);
            }

            if (op == TOK_CONCAT) {
                if ((left->tag != QK_TYPE_STRING && left->tag != QK_TYPE_ANY) ||
                    (right->tag != QK_TYPE_STRING && right->tag != QK_TYPE_ANY)) {
                    type_error(node->line, error_count, "Concatenation '++' requires String operands");
                }
                qk_type_free(left); qk_type_free(right);
                return qk_type_string();
            }

            if (op == TOK_EQ || op == TOK_NEQ) {
                qk_type_free(left); qk_type_free(right);
                return qk_type_bool();
            }

            if (op == TOK_LT || op == TOK_GT || op == TOK_LEQ || op == TOK_GEQ) {
                qk_type_free(left); qk_type_free(right);
                return qk_type_bool();
            }

            qk_type_free(left); qk_type_free(right);
            return qk_type_any();
        }

        case NODE_LET: {
            QkType *val_t = qk_typecheck_node(node->as.let_bind.value, env, error_count);
            qk_type_env_define(env, node->as.let_bind.name, val_t, node->as.let_bind.is_mut);
            return qk_type_unit();
        }

        case NODE_SHADOW: {
            QkType *val_t = qk_typecheck_node(node->as.shadow.value, env, error_count);
            qk_type_env_define(env, node->as.shadow.name, val_t, false);
            return qk_type_unit();
        }

        case NODE_ASSIGN: {
            QkType *existing = qk_type_env_lookup(env, node->as.assign.name);
            QkType *val_t = qk_typecheck_node(node->as.assign.value, env, error_count);
            if (existing && !qk_type_compatible(existing, val_t)) {
                type_error(node->line, error_count, "Assignment type mismatch");
            }
            qk_type_free(val_t);
            return qk_type_unit();
        }

        case NODE_IF: {
            QkType *cond_t = qk_typecheck_node(node->as.if_expr.cond, env, error_count);
            if (cond_t->tag != QK_TYPE_BOOL && cond_t->tag != QK_TYPE_ANY) {
                type_error(node->line, error_count, "If condition must evaluate to Bool");
            }
            qk_type_free(cond_t);

            QkTypeEnv *then_env = qk_type_env_new(env);
            QkType *then_t = qk_typecheck_node(node->as.if_expr.then_b, then_env, error_count);
            qk_type_env_free(then_env);

            if (node->as.if_expr.else_b) {
                QkTypeEnv *else_env = qk_type_env_new(env);
                QkType *else_t = qk_typecheck_node(node->as.if_expr.else_b, else_env, error_count);
                qk_type_env_free(else_env);
                qk_type_free(else_t);
            }
            return then_t;
        }

        case NODE_WHILE: {
            QkType *cond_t = qk_typecheck_node(node->as.while_loop.cond, env, error_count);
            if (cond_t->tag != QK_TYPE_BOOL && cond_t->tag != QK_TYPE_ANY) {
                type_error(node->line, error_count, "While condition must evaluate to Bool");
            }
            qk_type_free(cond_t);
            QkTypeEnv *body_env = qk_type_env_new(env);
            QkType *body_t = qk_typecheck_node(node->as.while_loop.body, body_env, error_count);
            qk_type_env_free(body_env);
            qk_type_free(body_t);
            return qk_type_unit();
        }

        case NODE_FOR: {
            QkType *iter_t = qk_typecheck_node(node->as.for_loop.iter, env, error_count);
            QkTypeEnv *body_env = qk_type_env_new(env);
            QkType *elem_t = (iter_t->tag == QK_TYPE_LIST && iter_t->inner) ? qk_type_clone(iter_t->inner) : qk_type_any();
            qk_type_env_define(body_env, node->as.for_loop.var_name, elem_t, false);
            QkType *body_t = qk_typecheck_node(node->as.for_loop.body, body_env, error_count);
            qk_type_env_free(body_env);
            qk_type_free(body_t);
            qk_type_free(iter_t);
            return qk_type_unit();
        }

        case NODE_STRUCT_DEF: {
            int cnt = node->as.struct_def.fields.count;
            char **names = (char **)qk_malloc(sizeof(char *) * (cnt > 0 ? cnt : 1));
            for (int i = 0; i < cnt; i++) {
                names[i] = node->as.struct_def.fields.items[i].name;
            }
            qk_type_env_register_struct(env, node->as.struct_def.name, cnt, names);
            free(names);
            return qk_type_unit();
        }

        case NODE_STRUCT_CONSTRUCT: {
            QkStructTypeEntry *entry = qk_type_env_find_struct(env, node->as.struct_construct.name);
            int cnt = node->as.struct_construct.fields.count;
            if (entry && entry->field_count != cnt) {
                type_error(node->line, error_count, "Struct constructor argument count mismatch");
            }
            for (int i = 0; i < cnt; i++) {
                /* Struct construct stores arguments in fields ParamList or Call args */
                (void)i;
            }
            return qk_type_struct(node->as.struct_construct.name, entry ? entry->field_count : 0, entry ? entry->field_names : NULL, NULL);
        }

        case NODE_GET_FIELD: {
            QkType *target_t = qk_typecheck_node(node->as.get_field.object, env, error_count);
            if (target_t->tag == QK_TYPE_STRUCT && target_t->name) {
                QkStructTypeEntry *entry = qk_type_env_find_struct(env, target_t->name);
                if (entry) {
                    bool found = false;
                    for (int i = 0; i < entry->field_count; i++) {
                        if (strcmp(entry->field_names[i], node->as.get_field.field) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        type_error(node->line, error_count, "Access of non-existent field on struct");
                    }
                }
            }
            qk_type_free(target_t);
            return qk_type_any();
        }

        case NODE_SET_FIELD: {
            QkType *target_t = qk_typecheck_node(node->as.set_field.object, env, error_count);
            QkType *val_t = qk_typecheck_node(node->as.set_field.value, env, error_count);
            qk_type_free(target_t);
            qk_type_free(val_t);
            return qk_type_unit();
        }

        case NODE_BLOCK: {
            QkTypeEnv *block_env = qk_type_env_new(env);
            QkType *last = qk_type_unit();
            int cnt = node->as.block.stmts.count;
            for (int i = 0; i < cnt; i++) {
                qk_type_free(last);
                last = qk_typecheck_node(node->as.block.stmts.items[i], block_env, error_count);
            }
            qk_type_env_free(block_env);
            return last;
        }

        case NODE_CALL: {
            int cnt = node->as.call.args.count;
            for (int i = 0; i < cnt; i++) {
                QkType *arg_t = qk_typecheck_node(node->as.call.args.items[i], env, error_count);
                qk_type_free(arg_t);
            }
            QkStructTypeEntry *s_entry = qk_type_env_find_struct(env, node->as.call.fn_name);
            if (s_entry) {
                if (s_entry->field_count != cnt) {
                    type_error(node->line, error_count, "Struct constructor argument count mismatch");
                }
                return qk_type_struct(s_entry->name, s_entry->field_count, s_entry->field_names, s_entry->field_types);
            }
            return qk_type_any();
        }

        case NODE_FN_DEF: {
            QkTypeEnv *fn_env = qk_type_env_new(env);
            int pcount = node->as.fn_def.params.count;
            for (int i = 0; i < pcount; i++) {
                qk_type_env_define(fn_env, node->as.fn_def.params.items[i].name, qk_type_any(), false);
            }
            QkType *body_t = qk_typecheck_node(node->as.fn_def.body, fn_env, error_count);
            qk_type_env_free(fn_env);
            QkType *fn_t = qk_type_fn(pcount, NULL, body_t);
            qk_type_free(body_t);
            qk_type_env_define(env, node->as.fn_def.name, fn_t, false);
            return qk_type_unit();
        }

        case NODE_SOME: {
            QkType *inner = qk_typecheck_node(node->as.some_val.value, env, error_count);
            QkType *opt = qk_type_option(inner);
            qk_type_free(inner);
            return opt;
        }
        case NODE_NONE: return qk_type_option(qk_type_any());
        case NODE_OK: {
            QkType *inner = qk_typecheck_node(node->as.ok_val.value, env, error_count);
            QkType *res = qk_type_result(inner, qk_type_any());
            qk_type_free(inner);
            return res;
        }
        case NODE_ERR: {
            QkType *inner = qk_typecheck_node(node->as.err_val.value, env, error_count);
            QkType *res = qk_type_result(qk_type_any(), inner);
            qk_type_free(inner);
            return res;
        }

        case NODE_LIST_LIT: {
            QkType *first_t = NULL;
            int cnt = node->as.list_lit.elements.count;
            for (int i = 0; i < cnt; i++) {
                QkType *it_t = qk_typecheck_node(node->as.list_lit.elements.items[i], env, error_count);
                if (!first_t) first_t = it_t;
                else qk_type_free(it_t);
            }
            QkType *list_t = qk_type_list(first_t ? first_t : qk_type_any());
            if (first_t) qk_type_free(first_t);
            return list_t;
        }

        case NODE_INDEX: {
            QkType *arr_t = qk_typecheck_node(node->as.index.array, env, error_count);
            QkType *idx_t = qk_typecheck_node(node->as.index.index, env, error_count);
            if (idx_t->tag != QK_TYPE_INT && idx_t->tag != QK_TYPE_ANY) {
                type_error(node->line, error_count, "Index must be an Integer");
            }
            qk_type_free(idx_t);
            QkType *elem_t = (arr_t->tag == QK_TYPE_LIST && arr_t->inner) ? qk_type_clone(arr_t->inner) : qk_type_any();
            qk_type_free(arr_t);
            return elem_t;
        }

        case NODE_PROGRAM: {
            QkType *last = qk_type_unit();
            int cnt = node->as.program.count;
            for (int i = 0; i < cnt; i++) {
                qk_type_free(last);
                last = qk_typecheck_node(node->as.program.items[i], env, error_count);
            }
            return last;
        }

        case NODE_MATCH: {
            QkType *subj_t = qk_typecheck_node(node->as.match_expr.subject, env, error_count);
            qk_type_free(subj_t);
            QkType *arm_res = qk_type_unit();
            for (int i = 0; i < node->as.match_expr.arm_count; i++) {
                qk_type_free(arm_res);
                arm_res = qk_typecheck_node(node->as.match_expr.arms[i].body, env, error_count);
            }
            return arm_res;
        }

        case NODE_UNWRAP: {
            QkType *op_t = qk_typecheck_node(node->as.unwrap.operand, env, error_count);
            if (op_t->tag != QK_TYPE_OPTION && op_t->tag != QK_TYPE_RESULT && op_t->tag != QK_TYPE_ANY) {
                type_error(node->line, error_count, "Unwrap '?' operator expects Option or Result");
            }
            QkType *res = (op_t->inner) ? qk_type_clone(op_t->inner) : qk_type_any();
            qk_type_free(op_t);
            return res;
        }

        case NODE_IMPORT: {
            return qk_type_unit();
        }

        case NODE_RETURN: {
            if (node->as.ret.value) {
                return qk_typecheck_node(node->as.ret.value, env, error_count);
            }
            return qk_type_unit();
        }

        default:
            return qk_type_any();
    }
}

bool qk_typecheck_program(Node *root) {
    if (!root) return true;
    int error_count = 0;
    QkTypeEnv *global_env = qk_type_env_new(NULL);

    qk_type_env_define(global_env, "print", qk_type_any(), false);
    qk_type_env_define(global_env, "println", qk_type_any(), false);
    qk_type_env_define(global_env, "len", qk_type_any(), false);
    qk_type_env_define(global_env, "push", qk_type_any(), false);
    qk_type_env_define(global_env, "range", qk_type_any(), false);
    qk_type_env_define(global_env, "to_string", qk_type_any(), false);
    qk_type_env_define(global_env, "exit", qk_type_any(), false);
    qk_type_env_define(global_env, "channel_new", qk_type_any(), false);
    qk_type_env_define(global_env, "channel_send", qk_type_any(), false);
    qk_type_env_define(global_env, "channel_recv", qk_type_any(), false);
    qk_type_env_define(global_env, "channel_close", qk_type_any(), false);
    qk_type_env_define(global_env, "thread_spawn", qk_type_any(), false);
    qk_type_env_define(global_env, "thread_join", qk_type_any(), false);

    QkType *res = qk_typecheck_node(root, global_env, &error_count);
    qk_type_free(res);
    qk_type_env_free(global_env);

    return error_count == 0;
}
