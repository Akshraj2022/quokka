#ifndef QUOKKA_TYPECHECK_H
#define QUOKKA_TYPECHECK_H

#include <stdbool.h>
#include "quokka.h"

typedef enum {
    QK_TYPE_ANY,
    QK_TYPE_INT,
    QK_TYPE_FLOAT,
    QK_TYPE_BOOL,
    QK_TYPE_STRING,
    QK_TYPE_UNIT,
    QK_TYPE_OPTION,
    QK_TYPE_RESULT,
    QK_TYPE_LIST,
    QK_TYPE_STRUCT,
    QK_TYPE_FN,
    QK_TYPE_CHANNEL,
    QK_TYPE_THREAD,
    QK_TYPE_ERROR,
} QkTypeTag;

typedef struct QkType {
    QkTypeTag tag;
    struct QkType *inner;        /* For Option, List, Channel */
    struct QkType *err_type;     /* For Result error type */
    char *name;                  /* For Struct name */
    int field_count;             /* For Struct / Function params */
    char **field_names;
    struct QkType **field_types;
    struct QkType *return_type;  /* For Function */
    int refcount;
} QkType;

typedef struct QkTypeBinding {
    char *name;
    QkType *type;
    bool is_mut;
    struct QkTypeBinding *next;
} QkTypeBinding;

typedef struct QkStructTypeEntry {
    char *name;
    int field_count;
    char **field_names;
    QkType **field_types;
    struct QkStructTypeEntry *next;
} QkStructTypeEntry;

typedef struct QkTypeEnv {
    QkTypeBinding *bindings;
    QkStructTypeEntry *structs;
    struct QkTypeEnv *parent;
} QkTypeEnv;

/* Type constructors */
QkType *qk_type_new(QkTypeTag tag);
QkType *qk_type_int(void);
QkType *qk_type_float(void);
QkType *qk_type_bool(void);
QkType *qk_type_string(void);
QkType *qk_type_unit(void);
QkType *qk_type_any(void);
QkType *qk_type_option(QkType *inner);
QkType *qk_type_result(QkType *ok_t, QkType *err_t);
QkType *qk_type_list(QkType *elem_t);
QkType *qk_type_channel(QkType *elem_t);
QkType *qk_type_thread(void);
QkType *qk_type_struct(const char *name, int count, char **fields, QkType **types);
QkType *qk_type_fn(int param_count, QkType **params, QkType *ret);
QkType *qk_type_clone(QkType *t);
void    qk_type_free(QkType *t);

bool qk_type_compatible(QkType *expected, QkType *actual);
const char *qk_type_to_string(QkType *t);

/* Type Environment */
QkTypeEnv *qk_type_env_new(QkTypeEnv *parent);
void       qk_type_env_free(QkTypeEnv *env);
bool       qk_type_env_define(QkTypeEnv *env, const char *name, QkType *type, bool is_mut);
QkType    *qk_type_env_lookup(QkTypeEnv *env, const char *name);
void       qk_type_env_register_struct(QkTypeEnv *env, const char *name, int count, char **fields);
QkStructTypeEntry *qk_type_env_find_struct(QkTypeEnv *env, const char *name);

/* Main Checker Interface */
QkType *qk_typecheck_node(Node *node, QkTypeEnv *env, int *error_count);
bool    qk_typecheck_program(Node *root);

#endif /* QUOKKA_TYPECHECK_H */
