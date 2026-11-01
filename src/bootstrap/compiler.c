#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    int depth;
    bool is_mut;
} Local;

typedef struct Compiler {
    struct Compiler *enclosing;
    QkFunctionObj *function;
    Local locals[256];
    int local_count;
    int scope_depth;
} Compiler;

static void init_compiler(Compiler *c, Compiler *enclosing, const char *name, int arity) {
    c->enclosing = enclosing;
    c->function = qk_function_obj_new(name, arity);
    c->local_count = 0;
    c->scope_depth = 0;

    /* Slot 0 is reserved for callee / function */
    c->locals[0].name = qk_strdup("");
    c->locals[0].depth = 0;
    c->locals[0].is_mut = false;
    c->local_count = 1;
}

static void emit_byte(Compiler *c, uint8_t byte, int line) {
    qk_chunk_write(&c->function->chunk, byte, line);
}

static void emit_bytes(Compiler *c, uint8_t b1, uint8_t b2, int line) {
    emit_byte(c, b1, line);
    emit_byte(c, b2, line);
}

static void emit_constant(Compiler *c, Value val, int line) {
    int idx = qk_chunk_add_constant(&c->function->chunk, val);
    emit_byte(c, OP_CONSTANT, line);
    emit_byte(c, (uint8_t)(idx & 0xFF), line);
    emit_byte(c, (uint8_t)((idx >> 8) & 0xFF), line);
}

static int emit_jump(Compiler *c, uint8_t instruction, int line) {
    emit_byte(c, instruction, line);
    emit_byte(c, 0xFF, line);
    emit_byte(c, 0xFF, line);
    return c->function->chunk.count - 2;
}

static void patch_jump(Compiler *c, int offset) {
    int jump = c->function->chunk.count - offset - 2;
    if (jump > 65535) {
        fprintf(stderr, "Jump offset exceeds 16-bit limit\n");
    }
    c->function->chunk.code[offset] = (uint8_t)(jump & 0xFF);
    c->function->chunk.code[offset + 1] = (uint8_t)((jump >> 8) & 0xFF);
}

static void emit_loop(Compiler *c, int loop_start, int line) {
    emit_byte(c, OP_LOOP, line);
    int offset = c->function->chunk.count - loop_start + 2;
    emit_byte(c, (uint8_t)(offset & 0xFF), line);
    emit_byte(c, (uint8_t)((offset >> 8) & 0xFF), line);
}

static void begin_scope(Compiler *c) {
    c->scope_depth++;
}

static void end_scope_with_result(Compiler *c, bool has_result, int line) {
    c->scope_depth--;
    int locals_to_pop = 0;
    while (c->local_count > 0 && c->locals[c->local_count - 1].depth > c->scope_depth) {
        if (c->locals[c->local_count - 1].name) {
            free(c->locals[c->local_count - 1].name);
            c->locals[c->local_count - 1].name = NULL;
        }
        c->local_count--;
        locals_to_pop++;
    }
    if (locals_to_pop > 0) {
        if (has_result) {
            /* Top of stack is result. First local is at c->local_count.
             * Store result in first local's slot, then pop remaining. */
            emit_byte(c, OP_SET_LOCAL, line);
            emit_byte(c, (uint8_t)c->local_count, line);
            for (int i = 0; i < locals_to_pop; i++) {
                emit_byte(c, OP_POP, line);
            }
        } else {
            for (int i = 0; i < locals_to_pop; i++) {
                emit_byte(c, OP_POP, line);
            }
        }
    }
}

static int resolve_local(Compiler *c, const char *name) {
    for (int i = c->local_count - 1; i >= 0; i--) {
        if (c->locals[i].name && strcmp(c->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_local(Compiler *c, const char *name, bool is_mut) {
    if (c->local_count >= 256) {
        fprintf(stderr, "Too many local variables in scope\n");
        return;
    }
    c->locals[c->local_count].name = qk_strdup(name);
    c->locals[c->local_count].depth = c->scope_depth;
    c->locals[c->local_count].is_mut = is_mut;
    c->local_count++;
}

static void compile_node(Compiler *c, Node *node);

static void compile_node(Compiler *c, Node *node) {
    if (!node) {
        emit_byte(c, OP_UNIT, 0);
        return;
    }

    int line = node->line;

    switch (node->type) {
        case NODE_INT_LIT:
            emit_constant(c, val_int(node->as.int_val), line);
            break;

        case NODE_FLOAT_LIT:
            emit_constant(c, val_float(node->as.float_val), line);
            break;

        case NODE_BOOL_LIT:
            emit_byte(c, node->as.bool_val ? OP_TRUE : OP_FALSE, line);
            break;

        case NODE_STRING_LIT:
            emit_constant(c, val_string(node->as.string_val), line);
            break;

        case NODE_IDENT: {
            int local_slot = resolve_local(c, node->as.name);
            if (local_slot != -1) {
                emit_byte(c, OP_GET_LOCAL, line);
                emit_byte(c, (uint8_t)local_slot, line);
            } else {
                int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.name));
                emit_byte(c, OP_GET_GLOBAL, line);
                emit_byte(c, (uint8_t)(name_const & 0xFF), line);
                emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
            }
            break;
        }

        case NODE_LET: {
            compile_node(c, node->as.let_bind.value);
            if (c->scope_depth > 0) {
                add_local(c, node->as.let_bind.name, node->as.let_bind.is_mut);
            } else {
                int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.let_bind.name));
                emit_byte(c, OP_DEFINE_GLOBAL, line);
                emit_byte(c, (uint8_t)(name_const & 0xFF), line);
                emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
                emit_byte(c, OP_POP, line);
                emit_byte(c, OP_UNIT, line);
            }
            break;
        }

        case NODE_SHADOW: {
            compile_node(c, node->as.shadow.value);
            if (c->scope_depth > 0) {
                add_local(c, node->as.shadow.name, false);
            } else {
                int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.shadow.name));
                emit_byte(c, OP_DEFINE_GLOBAL, line);
                emit_byte(c, (uint8_t)(name_const & 0xFF), line);
                emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
                emit_byte(c, OP_POP, line);
                emit_byte(c, OP_UNIT, line);
            }
            break;
        }

        case NODE_ASSIGN: {
            compile_node(c, node->as.assign.value);
            int local_slot = resolve_local(c, node->as.assign.name);
            if (local_slot != -1) {
                emit_byte(c, OP_SET_LOCAL, line);
                emit_byte(c, (uint8_t)local_slot, line);
            } else {
                int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.assign.name));
                emit_byte(c, OP_SET_GLOBAL, line);
                emit_byte(c, (uint8_t)(name_const & 0xFF), line);
                emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
            }
            break;
        }

        case NODE_UNARY: {
            compile_node(c, node->as.unary.operand);
            if (node->as.unary.op == TOK_MINUS) {
                emit_byte(c, OP_NEGATE, line);
            } else if (node->as.unary.op == TOK_NOT) {
                emit_byte(c, OP_NOT, line);
            }
            break;
        }

        case NODE_BINARY: {
            compile_node(c, node->as.binary.left);
            compile_node(c, node->as.binary.right);
            switch (node->as.binary.op) {
                case TOK_PLUS:     emit_byte(c, OP_ADD, line); break;
                case TOK_MINUS:    emit_byte(c, OP_SUBTRACT, line); break;
                case TOK_STAR:     emit_byte(c, OP_MULTIPLY, line); break;
                case TOK_SLASH:    emit_byte(c, OP_DIVIDE, line); break;
                case TOK_PERCENT:  emit_byte(c, OP_MODULO, line); break;
                case TOK_CONCAT:   emit_byte(c, OP_CONCAT, line); break;
                case TOK_EQ:       emit_byte(c, OP_EQUAL, line); break;
                case TOK_NEQ:      emit_bytes(c, OP_EQUAL, OP_NOT, line); break;
                case TOK_GT:       emit_byte(c, OP_GREATER, line); break;
                case TOK_LT:       emit_byte(c, OP_LESS, line); break;
                case TOK_GEQ:      emit_bytes(c, OP_LESS, OP_NOT, line); break;
                case TOK_LEQ:      emit_bytes(c, OP_GREATER, OP_NOT, line); break;
                default: break;
            }
            break;
        }

        case NODE_IF: {
            compile_node(c, node->as.if_expr.cond);
            int then_jump = emit_jump(c, OP_JUMP_IF_FALSE, line);
            emit_byte(c, OP_POP, line);

            compile_node(c, node->as.if_expr.then_b);
            int else_jump = emit_jump(c, OP_JUMP, line);

            patch_jump(c, then_jump);
            emit_byte(c, OP_POP, line);

            if (node->as.if_expr.else_b) {
                compile_node(c, node->as.if_expr.else_b);
            } else {
                emit_byte(c, OP_UNIT, line);
            }
            patch_jump(c, else_jump);
            break;
        }

        case NODE_WHILE: {
            int loop_start = c->function->chunk.count;
            compile_node(c, node->as.while_loop.cond);
            int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, line);
            emit_byte(c, OP_POP, line);

            compile_node(c, node->as.while_loop.body);
            emit_byte(c, OP_POP, line);

            emit_loop(c, loop_start, line);
            patch_jump(c, exit_jump);
            emit_byte(c, OP_POP, line);
            emit_byte(c, OP_UNIT, line);
            break;
        }

        case NODE_FOR: {
            /* Compile for var in iter { body } */
            begin_scope(c);
            compile_node(c, node->as.for_loop.iter);
            add_local(c, "__iter", false);
            int iter_slot = resolve_local(c, "__iter");

            emit_constant(c, val_int(0), line);
            add_local(c, "__idx", true);
            int idx_slot = resolve_local(c, "__idx");

            int loop_start = c->function->chunk.count;

            /* Call len(__iter) */
            int len_const = qk_chunk_add_constant(&c->function->chunk, val_string("len"));
            emit_byte(c, OP_GET_GLOBAL, line);
            emit_byte(c, (uint8_t)(len_const & 0xFF), line);
            emit_byte(c, (uint8_t)((len_const >> 8) & 0xFF), line);
            emit_byte(c, OP_GET_LOCAL, line);
            emit_byte(c, (uint8_t)iter_slot, line);
            emit_byte(c, OP_CALL, line);
            emit_byte(c, 1, line);

            /* Check __idx < len */
            emit_byte(c, OP_GET_LOCAL, line);
            emit_byte(c, (uint8_t)idx_slot, line);
            /* len is on stack, idx is on stack: check idx < len */
            /* stack: [len, idx]. We want idx < len: swap by pop/push or OP_GREATER */
            /* len > idx <=> idx < len */
            emit_byte(c, OP_GREATER, line);

            int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, line);
            emit_byte(c, OP_POP, line);

            /* Get element: __iter[__idx] */
            emit_byte(c, OP_GET_LOCAL, line);
            emit_byte(c, (uint8_t)iter_slot, line);
            emit_byte(c, OP_GET_LOCAL, line);
            emit_byte(c, (uint8_t)idx_slot, line);
            emit_byte(c, OP_GET_INDEX, line);

            /* Bind loop var */
            add_local(c, node->as.for_loop.var_name, false);
            compile_node(c, node->as.for_loop.body);
            emit_byte(c, OP_POP, line);

            /* Pop loop var */
            emit_byte(c, OP_POP, line);
            if (c->locals[c->local_count - 1].name) free(c->locals[c->local_count - 1].name);
            c->local_count--;

            /* Increment __idx */
            emit_byte(c, OP_GET_LOCAL, line);
            emit_byte(c, (uint8_t)idx_slot, line);
            emit_constant(c, val_int(1), line);
            emit_byte(c, OP_ADD, line);
            emit_byte(c, OP_SET_LOCAL, line);
            emit_byte(c, (uint8_t)idx_slot, line);
            emit_byte(c, OP_POP, line);

            emit_loop(c, loop_start, line);

            patch_jump(c, exit_jump);
            emit_byte(c, OP_POP, line);

            end_scope_with_result(c, false, line);
            emit_byte(c, OP_UNIT, line);
            break;
        }

        case NODE_BLOCK: {
            begin_scope(c);
            int count = node->as.block.stmts.count;
            if (count == 0) {
                emit_byte(c, OP_UNIT, line);
                end_scope_with_result(c, false, line);
            } else {
                for (int i = 0; i < count; i++) {
                    Node *stmt = node->as.block.stmts.items[i];
                    compile_node(c, stmt);
                    if (i < count - 1) {
                        if (stmt && stmt->type != NODE_LET && stmt->type != NODE_SHADOW) {
                            emit_byte(c, OP_POP, line);
                        }
                    }
                }
                Node *last = node->as.block.stmts.items[count - 1];
                bool has_res = (last && last->type != NODE_LET && last->type != NODE_SHADOW &&
                                last->type != NODE_WHILE && last->type != NODE_FOR &&
                                last->type != NODE_STRUCT_DEF && last->type != NODE_FN_DEF);
                end_scope_with_result(c, has_res, line);
                if (!has_res) {
                    emit_byte(c, OP_UNIT, line);
                }
            }
            break;
        }

        case NODE_PROGRAM: {
            int count = node->as.program.count;
            if (count == 0) {
                emit_byte(c, OP_UNIT, line);
            } else {
                for (int i = 0; i < count; i++) {
                    compile_node(c, node->as.program.items[i]);
                    if (i < count - 1) {
                        emit_byte(c, OP_POP, line);
                    }
                }
            }
            break;
        }

        case NODE_CALL: {
            int local_slot = resolve_local(c, node->as.call.fn_name);
            if (local_slot != -1) {
                emit_byte(c, OP_GET_LOCAL, line);
                emit_byte(c, (uint8_t)local_slot, line);
            } else {
                int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.call.fn_name));
                emit_byte(c, OP_GET_GLOBAL, line);
                emit_byte(c, (uint8_t)(name_const & 0xFF), line);
                emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
            }

            int argc = node->as.call.args.count;
            for (int i = 0; i < argc; i++) {
                compile_node(c, node->as.call.args.items[i]);
            }
            emit_byte(c, OP_CALL, line);
            emit_byte(c, (uint8_t)argc, line);
            break;
        }

        case NODE_FN_DEF: {
            QkFunction fn;
            fn.name = qk_strdup(node->as.fn_def.name);
            fn.params = node->as.fn_def.params;
            fn.body = node->as.fn_def.body;
            fn.closure_env = NULL;
            Value fn_val = val_fn(fn);

            int val_const = qk_chunk_add_constant(&c->function->chunk, fn_val);
            int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.fn_def.name));

            emit_byte(c, OP_CONSTANT, line);
            emit_byte(c, (uint8_t)(val_const & 0xFF), line);
            emit_byte(c, (uint8_t)((val_const >> 8) & 0xFF), line);

            emit_byte(c, OP_DEFINE_GLOBAL, line);
            emit_byte(c, (uint8_t)(name_const & 0xFF), line);
            emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);

            emit_byte(c, OP_POP, line);
            emit_byte(c, OP_UNIT, line);
            break;
        }

        case NODE_LAMBDA: {
            QkFunction fn;
            fn.name = qk_strdup("<lambda>");
            fn.params = node->as.lambda.params;
            fn.body = node->as.lambda.body;
            fn.closure_env = NULL;
            Value fn_val = val_fn(fn);

            int val_const = qk_chunk_add_constant(&c->function->chunk, fn_val);
            emit_byte(c, OP_CONSTANT, line);
            emit_byte(c, (uint8_t)(val_const & 0xFF), line);
            emit_byte(c, (uint8_t)((val_const >> 8) & 0xFF), line);
            break;
        }

        case NODE_IMPORT: {
            const char *path = node->as.import_stmt.path;
            if (!path) break;

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
            if (!found_path) break;

            FILE *mf = fopen(found_path, "rb");
            if (!mf) break;
            fseek(mf, 0, SEEK_END);
            long sz = ftell(mf);
            fseek(mf, 0, SEEK_SET);
            if (sz <= 0) { fclose(mf); break; }
            char *src = qk_malloc((size_t)sz + 1);
            size_t rd = fread(src, 1, (size_t)sz, mf);
            src[rd] = '\0';
            fclose(mf);

            Parser mp;
            parser_init(&mp, src);
            Node *m_ast = parser_parse(&mp);
            free(src);
            if (m_ast && !mp.had_error) {
                compile_node(c, m_ast);
            }
            break;
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
            fn.closure_env = NULL;
            Value fn_val = val_fn(fn);

            int val_const = qk_chunk_add_constant(&c->function->chunk, fn_val);
            int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.struct_def.name));

            emit_byte(c, OP_CONSTANT, line);
            emit_byte(c, (uint8_t)(val_const & 0xFF), line);
            emit_byte(c, (uint8_t)((val_const >> 8) & 0xFF), line);

            emit_byte(c, OP_DEFINE_GLOBAL, line);
            emit_byte(c, (uint8_t)(name_const & 0xFF), line);
            emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);

            emit_byte(c, OP_POP, line);
            emit_byte(c, OP_UNIT, line);
            break;
        }

        case NODE_GET_FIELD: {
            compile_node(c, node->as.get_field.object);
            int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.get_field.field));
            emit_byte(c, OP_GET_FIELD, line);
            emit_byte(c, (uint8_t)(name_const & 0xFF), line);
            emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
            break;
        }

        case NODE_SET_FIELD: {
            compile_node(c, node->as.set_field.object);
            compile_node(c, node->as.set_field.value);
            int name_const = qk_chunk_add_constant(&c->function->chunk, val_string(node->as.set_field.field));
            emit_byte(c, OP_SET_FIELD, line);
            emit_byte(c, (uint8_t)(name_const & 0xFF), line);
            emit_byte(c, (uint8_t)((name_const >> 8) & 0xFF), line);
            break;
        }

        case NODE_LIST_LIT: {
            int count = node->as.list_lit.elements.count;
            for (int i = 0; i < count; i++) {
                compile_node(c, node->as.list_lit.elements.items[i]);
            }
            emit_byte(c, OP_BUILD_LIST, line);
            emit_byte(c, (uint8_t)(count & 0xFF), line);
            emit_byte(c, (uint8_t)((count >> 8) & 0xFF), line);
            break;
        }

        case NODE_INDEX: {
            compile_node(c, node->as.index.array);
            compile_node(c, node->as.index.index);
            emit_byte(c, OP_GET_INDEX, line);
            break;
        }

        case NODE_SOME: {
            compile_node(c, node->as.some_val.value);
            emit_byte(c, OP_SOME, line);
            break;
        }

        case NODE_NONE:
            emit_constant(c, val_none(), line);
            break;

        case NODE_OK: {
            compile_node(c, node->as.ok_val.value);
            emit_byte(c, OP_OK, line);
            break;
        }

        case NODE_ERR: {
            compile_node(c, node->as.err_val.value);
            emit_byte(c, OP_ERR, line);
            break;
        }

        case NODE_RETURN: {
            if (node->as.ret.value) {
                compile_node(c, node->as.ret.value);
            } else {
                emit_byte(c, OP_UNIT, line);
            }
            emit_byte(c, OP_RETURN, line);
            break;
        }

        default:
            emit_byte(c, OP_UNIT, line);
            break;
    }
}

QkFunctionObj *qk_compile_ast(Node *root) {
    if (!root) return NULL;
    Compiler compiler;
    init_compiler(&compiler, NULL, "main", 0);

    compile_node(&compiler, root);
    emit_byte(&compiler, OP_HALT, 0);

    /* Clean up locals */
    for (int i = 0; i < compiler.local_count; i++) {
        if (compiler.locals[i].name) free(compiler.locals[i].name);
    }

    return compiler.function;
}
