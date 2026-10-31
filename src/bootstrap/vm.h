#ifndef QUOKKA_VM_H
#define QUOKKA_VM_H

#include <stdint.h>
#include <stdbool.h>
#include "quokka.h"

#define QK_STACK_MAX  2048
#define QK_FRAMES_MAX 256

typedef enum {
    OP_CONSTANT,      /* [uint16_t const_idx] */
    OP_UNIT,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,
    OP_GET_LOCAL,     /* [uint16_t slot] */
    OP_SET_LOCAL,     /* [uint16_t slot] */
    OP_GET_GLOBAL,    /* [uint16_t name_idx] */
    OP_SET_GLOBAL,    /* [uint16_t name_idx] */
    OP_DEFINE_GLOBAL, /* [uint16_t name_idx] */
    OP_GET_FIELD,     /* [uint16_t name_idx] */
    OP_SET_FIELD,     /* [uint16_t name_idx] */
    OP_BUILD_STRUCT,  /* [uint16_t name_idx] [uint8_t field_count] */
    OP_BUILD_LIST,    /* [uint16_t item_count] */
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_SOME,
    OP_OK,
    OP_ERR,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_CONCAT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_NOT,
    OP_NEGATE,
    OP_JUMP,          /* [uint16_t offset] */
    OP_JUMP_IF_FALSE, /* [uint16_t offset] */
    OP_LOOP,          /* [uint16_t offset] */
    OP_CALL,          /* [uint8_t arg_count] */
    OP_RETURN,
    OP_HALT,
} QkOpcode;

typedef struct {
    uint8_t *code;
    int     *lines;
    int      count;
    int      capacity;
    Value   *constants;
    int      const_count;
    int      const_capacity;
} QkChunk;

typedef struct QkFunctionObj {
    int arity;
    QkChunk chunk;
    char *name;
    int refcount;
} QkFunctionObj;

typedef struct {
    QkFunctionObj *function;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct QkVM {
    CallFrame frames[QK_FRAMES_MAX];
    int frame_count;
    Value stack[QK_STACK_MAX];
    Value *stack_top;
    Env *globals;
    Interp *interp;
} QkVM;

typedef enum {
    VM_RESULT_OK,
    VM_RESULT_COMPILE_ERROR,
    VM_RESULT_RUNTIME_ERROR,
} QkVMResult;

/* Chunk Primitives */
void qk_chunk_init(QkChunk *chunk);
void qk_chunk_free(QkChunk *chunk);
void qk_chunk_write(QkChunk *chunk, uint8_t byte, int line);
int  qk_chunk_add_constant(QkChunk *chunk, Value val);

/* Function Object Primitives */
QkFunctionObj *qk_function_obj_new(const char *name, int arity);
void           qk_function_obj_free(QkFunctionObj *fn);

/* VM Primitives */
void       qk_vm_init(QkVM *vm, Interp *interp);
void       qk_vm_free(QkVM *vm);
void       qk_vm_push(QkVM *vm, Value val);
Value      qk_vm_pop(QkVM *vm);
QkVMResult qk_vm_run(QkVM *vm, QkFunctionObj *main_fn);

/* Compiler */
QkFunctionObj *qk_compile_ast(Node *root);

#endif /* QUOKKA_VM_H */
