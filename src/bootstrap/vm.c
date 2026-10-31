#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Chunk Primitives
 * ============================================================ */

void qk_chunk_init(QkChunk *chunk) {
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->constants = NULL;
    chunk->const_count = 0;
    chunk->const_capacity = 0;
}

void qk_chunk_free(QkChunk *chunk) {
    if (chunk->code) free(chunk->code);
    if (chunk->lines) free(chunk->lines);
    if (chunk->constants) {
        for (int i = 0; i < chunk->const_count; i++) {
            val_free(chunk->constants[i]);
        }
        free(chunk->constants);
    }
    qk_chunk_init(chunk);
}

void qk_chunk_write(QkChunk *chunk, uint8_t byte, int line) {
    if (chunk->count >= chunk->capacity) {
        int new_cap = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        chunk->code = (uint8_t *)qk_realloc(chunk->code, new_cap);
        chunk->lines = (int *)qk_realloc(chunk->lines, new_cap * sizeof(int));
        chunk->capacity = new_cap;
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int qk_chunk_add_constant(QkChunk *chunk, Value val) {
    if (chunk->const_count >= chunk->const_capacity) {
        int new_cap = chunk->const_capacity < 8 ? 8 : chunk->const_capacity * 2;
        chunk->constants = (Value *)qk_realloc(chunk->constants, new_cap * sizeof(Value));
        chunk->const_capacity = new_cap;
    }
    chunk->constants[chunk->const_count] = val;
    return chunk->const_count++;
}

/* ============================================================
 * Function Object
 * ============================================================ */

QkFunctionObj *qk_function_obj_new(const char *name, int arity) {
    QkFunctionObj *fn = (QkFunctionObj *)qk_malloc(sizeof(QkFunctionObj));
    fn->arity = arity;
    fn->name = qk_strdup(name ? name : "");
    fn->refcount = 1;
    qk_chunk_init(&fn->chunk);
    return fn;
}

void qk_function_obj_free(QkFunctionObj *fn) {
    if (!fn) return;
    fn->refcount--;
    if (fn->refcount > 0) return;
    if (fn->name) free(fn->name);
    qk_chunk_free(&fn->chunk);
    free(fn);
}

/* ============================================================
 * Virtual Machine
 * ============================================================ */

void qk_vm_init(QkVM *vm, Interp *interp) {
    vm->frame_count = 0;
    vm->stack_top = vm->stack;
    vm->interp = interp;
    vm->globals = interp ? interp->globals : env_new(NULL);
}

void qk_vm_free(QkVM *vm) {
    while (vm->stack_top > vm->stack) {
        vm->stack_top--;
        val_free(*vm->stack_top);
    }
}

void qk_vm_push(QkVM *vm, Value val) {
    if (vm->stack_top >= vm->stack + QK_STACK_MAX) {
        fprintf(stderr, "VM stack overflow\n");
        return;
    }
    *vm->stack_top = val;
    vm->stack_top++;
}

Value qk_vm_pop(QkVM *vm) {
    if (vm->stack_top <= vm->stack) {
        return val_unit();
    }
    vm->stack_top--;
    return *vm->stack_top;
}

static Value qk_vm_peek(QkVM *vm, int distance) {
    return vm->stack_top[-1 - distance];
}

QkVMResult qk_vm_run(QkVM *vm, QkFunctionObj *main_fn) {
    if (!main_fn) return VM_RESULT_COMPILE_ERROR;

    vm->frame_count = 1;
    CallFrame *frame = &vm->frames[0];
    frame->function = main_fn;
    frame->ip = main_fn->chunk.code;
    frame->slots = vm->stack;

    /* Push main function onto slot 0 */
    qk_vm_push(vm, val_unit());

    for (;;) {
        uint8_t instruction = *frame->ip++;

        switch (instruction) {
            case OP_CONSTANT: {
                uint16_t idx = (uint16_t)(*frame->ip++);
                idx |= (uint16_t)((*frame->ip++) << 8);
                Value constant = frame->function->chunk.constants[idx];
                qk_vm_push(vm, val_clone(constant));
                break;
            }

            case OP_UNIT:  qk_vm_push(vm, val_unit()); break;
            case OP_TRUE:  qk_vm_push(vm, val_bool(true)); break;
            case OP_FALSE: qk_vm_push(vm, val_bool(false)); break;

            case OP_POP: {
                Value v = qk_vm_pop(vm);
                val_free(v);
                break;
            }

            case OP_DUP: {
                Value v = qk_vm_peek(vm, 0);
                qk_vm_push(vm, val_clone(v));
                break;
            }

            case OP_GET_LOCAL: {
                uint8_t slot = *frame->ip++;
                qk_vm_push(vm, val_clone(frame->slots[slot]));
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = *frame->ip++;
                val_free(frame->slots[slot]);
                frame->slots[slot] = val_clone(qk_vm_peek(vm, 0));
                break;
            }

            case OP_GET_GLOBAL: {
                uint16_t name_idx = (uint16_t)(*frame->ip++);
                name_idx |= (uint16_t)((*frame->ip++) << 8);
                const char *name = frame->function->chunk.constants[name_idx].as.string_val;
                Value *v = env_get(vm->globals, name);
                if (!v) {
                    fprintf(stderr, "Undefined global variable '%s'\n", name);
                    return VM_RESULT_RUNTIME_ERROR;
                }
                qk_vm_push(vm, val_clone(*v));
                break;
            }

            case OP_DEFINE_GLOBAL:
            case OP_SET_GLOBAL: {
                uint16_t name_idx = (uint16_t)(*frame->ip++);
                name_idx |= (uint16_t)((*frame->ip++) << 8);
                const char *name = frame->function->chunk.constants[name_idx].as.string_val;
                Value val = qk_vm_peek(vm, 0);
                env_define(vm->globals, name, val, true);
                break;
            }

            case OP_ADD: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    qk_vm_push(vm, val_int(a.as.int_val + b.as.int_val));
                } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                    double da = a.type == VAL_FLOAT ? a.as.float_val : (double)a.as.int_val;
                    double db = b.type == VAL_FLOAT ? b.as.float_val : (double)b.as.int_val;
                    qk_vm_push(vm, val_float(da + db));
                } else if (a.type == VAL_STRING && b.type == VAL_STRING) {
                    size_t l1 = strlen(a.as.string_val);
                    size_t l2 = strlen(b.as.string_val);
                    char *cat = (char *)qk_malloc(l1 + l2 + 1);
                    memcpy(cat, a.as.string_val, l1);
                    memcpy(cat + l1, b.as.string_val, l2 + 1);
                    qk_vm_push(vm, val_string(cat));
                    free(cat);
                } else {
                    qk_vm_push(vm, val_unit());
                }
                val_free(a); val_free(b);
                break;
            }

            case OP_SUBTRACT: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    qk_vm_push(vm, val_int(a.as.int_val - b.as.int_val));
                } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                    double da = a.type == VAL_FLOAT ? a.as.float_val : (double)a.as.int_val;
                    double db = b.type == VAL_FLOAT ? b.as.float_val : (double)b.as.int_val;
                    qk_vm_push(vm, val_float(da - db));
                } else {
                    qk_vm_push(vm, val_unit());
                }
                val_free(a); val_free(b);
                break;
            }

            case OP_MULTIPLY: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    qk_vm_push(vm, val_int(a.as.int_val * b.as.int_val));
                } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                    double da = a.type == VAL_FLOAT ? a.as.float_val : (double)a.as.int_val;
                    double db = b.type == VAL_FLOAT ? b.as.float_val : (double)b.as.int_val;
                    qk_vm_push(vm, val_float(da * db));
                } else {
                    qk_vm_push(vm, val_unit());
                }
                val_free(a); val_free(b);
                break;
            }

            case OP_DIVIDE: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (b.type == VAL_INT && b.as.int_val == 0) {
                    fprintf(stderr, "Division by zero\n");
                    val_free(a); val_free(b);
                    return VM_RESULT_RUNTIME_ERROR;
                }
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    qk_vm_push(vm, val_int(a.as.int_val / b.as.int_val));
                } else {
                    double da = a.type == VAL_FLOAT ? a.as.float_val : (double)a.as.int_val;
                    double db = b.type == VAL_FLOAT ? b.as.float_val : (double)b.as.int_val;
                    qk_vm_push(vm, val_float(da / db));
                }
                val_free(a); val_free(b);
                break;
            }

            case OP_MODULO: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (b.as.int_val == 0) {
                    fprintf(stderr, "Modulo by zero\n");
                    val_free(a); val_free(b);
                    return VM_RESULT_RUNTIME_ERROR;
                }
                qk_vm_push(vm, val_int(a.as.int_val % b.as.int_val));
                val_free(a); val_free(b);
                break;
            }

            case OP_CONCAT: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                const char *sa = a.type == VAL_STRING ? a.as.string_val : "";
                const char *sb = b.type == VAL_STRING ? b.as.string_val : "";
                size_t la = strlen(sa);
                size_t lb = strlen(sb);
                char *cat = (char *)qk_malloc(la + lb + 1);
                memcpy(cat, sa, la);
                memcpy(cat + la, sb, lb + 1);
                qk_vm_push(vm, val_string(cat));
                free(cat);
                val_free(a); val_free(b);
                break;
            }

            case OP_EQUAL: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                qk_vm_push(vm, val_bool(val_equal(a, b)));
                val_free(a); val_free(b);
                break;
            }

            case OP_GREATER: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    qk_vm_push(vm, val_bool(a.as.int_val > b.as.int_val));
                } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                    double da = a.type == VAL_FLOAT ? a.as.float_val : (double)a.as.int_val;
                    double db = b.type == VAL_FLOAT ? b.as.float_val : (double)b.as.int_val;
                    qk_vm_push(vm, val_bool(da > db));
                } else {
                    qk_vm_push(vm, val_bool(false));
                }
                val_free(a); val_free(b);
                break;
            }

            case OP_LESS: {
                Value b = qk_vm_pop(vm);
                Value a = qk_vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    qk_vm_push(vm, val_bool(a.as.int_val < b.as.int_val));
                } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                    double da = a.type == VAL_FLOAT ? a.as.float_val : (double)a.as.int_val;
                    double db = b.type == VAL_FLOAT ? b.as.float_val : (double)b.as.int_val;
                    qk_vm_push(vm, val_bool(da < db));
                } else {
                    qk_vm_push(vm, val_bool(false));
                }
                val_free(a); val_free(b);
                break;
            }

            case OP_NOT: {
                Value a = qk_vm_pop(vm);
                bool b = (a.type == VAL_BOOL) ? !a.as.bool_val : false;
                qk_vm_push(vm, val_bool(b));
                val_free(a);
                break;
            }

            case OP_NEGATE: {
                Value a = qk_vm_pop(vm);
                if (a.type == VAL_INT) qk_vm_push(vm, val_int(-a.as.int_val));
                else if (a.type == VAL_FLOAT) qk_vm_push(vm, val_float(-a.as.float_val));
                else qk_vm_push(vm, val_unit());
                val_free(a);
                break;
            }

            case OP_JUMP: {
                uint16_t offset = (uint16_t)(*frame->ip++);
                offset |= (uint16_t)((*frame->ip++) << 8);
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)(*frame->ip++);
                offset |= (uint16_t)((*frame->ip++) << 8);
                Value top = qk_vm_peek(vm, 0);
                if (top.type == VAL_BOOL && !top.as.bool_val) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_LOOP: {
                uint16_t offset = (uint16_t)(*frame->ip++);
                offset |= (uint16_t)((*frame->ip++) << 8);
                frame->ip -= offset;
                break;
            }

            case OP_BUILD_STRUCT: {
                uint16_t name_idx = (uint16_t)(*frame->ip++);
                name_idx |= (uint16_t)((*frame->ip++) << 8);
                uint8_t count = *frame->ip++;
                const char *struct_name = frame->function->chunk.constants[name_idx].as.string_val;

                Value *args = (Value *)qk_malloc(sizeof(Value) * (count > 0 ? count : 1));
                char **fields = (char **)qk_malloc(sizeof(char *) * (count > 0 ? count : 1));
                for (int i = count - 1; i >= 0; i--) {
                    args[i] = qk_vm_pop(vm);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "field_%d", i);
                    fields[i] = qk_strdup(buf);
                }
                Value s = val_struct(struct_name, count, fields, args);
                for (int i = 0; i < count; i++) {
                    free(fields[i]);
                    val_free(args[i]);
                }
                free(fields);
                free(args);
                qk_vm_push(vm, s);
                break;
            }

            case OP_GET_FIELD: {
                uint16_t name_idx = (uint16_t)(*frame->ip++);
                name_idx |= (uint16_t)((*frame->ip++) << 8);
                const char *fname = frame->function->chunk.constants[name_idx].as.string_val;
                Value target = qk_vm_pop(vm);
                if (target.type == VAL_STRUCT && target.as.struct_val) {
                    Value res = val_unit();
                    for (int i = 0; i < target.as.struct_val->field_count; i++) {
                        if (strcmp(target.as.struct_val->field_names[i], fname) == 0) {
                            res = val_clone(target.as.struct_val->field_values[i]);
                            break;
                        }
                    }
                    qk_vm_push(vm, res);
                } else {
                    qk_vm_push(vm, val_unit());
                }
                val_free(target);
                break;
            }

            case OP_SET_FIELD: {
                uint16_t name_idx = (uint16_t)(*frame->ip++);
                name_idx |= (uint16_t)((*frame->ip++) << 8);
                const char *fname = frame->function->chunk.constants[name_idx].as.string_val;
                Value val = qk_vm_pop(vm);
                Value target = qk_vm_pop(vm);
                if (target.type == VAL_STRUCT && target.as.struct_val) {
                    for (int i = 0; i < target.as.struct_val->field_count; i++) {
                        if (strcmp(target.as.struct_val->field_names[i], fname) == 0) {
                            val_free(target.as.struct_val->field_values[i]);
                            target.as.struct_val->field_values[i] = val_clone(val);
                            break;
                        }
                    }
                }
                val_free(val);
                val_free(target);
                qk_vm_push(vm, val_unit());
                break;
            }

            case OP_BUILD_LIST: {
                uint16_t count = (uint16_t)(*frame->ip++);
                count |= (uint16_t)((*frame->ip++) << 8);
                Value list = val_list();
                Value *temp = (Value *)qk_malloc(sizeof(Value) * (count > 0 ? count : 1));
                for (int i = count - 1; i >= 0; i--) {
                    temp[i] = qk_vm_pop(vm);
                }
                for (int i = 0; i < count; i++) {
                    list_push(&list, temp[i]);
                }
                free(temp);
                qk_vm_push(vm, list);
                break;
            }

            case OP_GET_INDEX: {
                Value idx = qk_vm_pop(vm);
                Value target = qk_vm_pop(vm);
                if (target.type == VAL_LIST && idx.type == VAL_INT) {
                    int64_t i = idx.as.int_val;
                    if (i >= 0 && i < target.as.list->count) {
                        qk_vm_push(vm, val_clone(target.as.list->items[i]));
                    } else {
                        qk_vm_push(vm, val_unit());
                    }
                } else if (target.type == VAL_STRING && idx.type == VAL_INT) {
                    int64_t i = idx.as.int_val;
                    size_t len = strlen(target.as.string_val);
                    if (i >= 0 && (size_t)i < len) {
                        char sub[2] = { target.as.string_val[i], '\0' };
                        qk_vm_push(vm, val_string(sub));
                    } else {
                        qk_vm_push(vm, val_unit());
                    }
                } else {
                    qk_vm_push(vm, val_unit());
                }
                val_free(idx);
                val_free(target);
                break;
            }

            case OP_SOME: {
                Value inner = qk_vm_pop(vm);
                qk_vm_push(vm, val_some(inner));
                val_free(inner);
                break;
            }

            case OP_OK: {
                Value inner = qk_vm_pop(vm);
                qk_vm_push(vm, val_ok(inner));
                val_free(inner);
                break;
            }

            case OP_ERR: {
                Value inner = qk_vm_pop(vm);
                qk_vm_push(vm, val_err(inner));
                val_free(inner);
                break;
            }

            case OP_CALL: {
                uint8_t arg_count = *frame->ip++;
                Value callee = qk_vm_peek(vm, arg_count);

                if (callee.type == VAL_BUILTIN) {
                    Value *args = vm->stack_top - arg_count;
                    Value res = callee.as.builtin(args, arg_count, 0);
                    /* Pop arguments and callee */
                    for (int i = 0; i <= arg_count; i++) {
                        Value v = qk_vm_pop(vm);
                        val_free(v);
                    }
                    qk_vm_push(vm, res);
                } else if (callee.type == VAL_FN) {
                    Value *args = vm->stack_top - arg_count;
                    Value res = qk_call_value(vm->interp, callee, args, arg_count, 0);
                    for (int i = 0; i <= arg_count; i++) {
                        Value v = qk_vm_pop(vm);
                        val_free(v);
                    }
                    qk_vm_push(vm, res);
                } else {
                    fprintf(stderr, "Attempted to call non-function value\n");
                    return VM_RESULT_RUNTIME_ERROR;
                }
                break;
            }

            case OP_RETURN: {
                Value res = qk_vm_pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    val_free(res);
                    return VM_RESULT_OK;
                }
                frame = &vm->frames[vm->frame_count - 1];
                qk_vm_push(vm, res);
                break;
            }

            case OP_HALT:
                return VM_RESULT_OK;

            default:
                fprintf(stderr, "Unknown VM opcode: %02x\n", instruction);
                return VM_RESULT_RUNTIME_ERROR;
        }
    }
}
