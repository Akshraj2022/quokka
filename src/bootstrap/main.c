/*============================================================
 * Quokka Language -- Entry Point
 *
 * Usage:
 *   quokka                   -- start REPL
 *   quokka <file.qka>        -- run a .qka file
 *============================================================*/

#include "quokka.h"
#include "typecheck.h"
#include "vm.h"

/* ============================================================
 * Utility implementations
 * ============================================================ */

void *qk_malloc(size_t size) {
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (!ptr) qk_fatal("Out of memory in qk_malloc");
    return ptr;
}

void *qk_realloc(void *ptr, size_t size) {
    if (size == 0) { free(ptr); return NULL; }
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) qk_fatal("Out of memory in qk_realloc");
    return new_ptr;
}

void qk_free(void *ptr) {
    if (ptr) free(ptr);
}

void qk_arena_reset(void) {
    qk_ast_free_all();
}

char *qk_strdup(const char *s) {
    if (!s) return qk_strdup("");

    if (!s) s = "";
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (!dup) { fprintf(stderr, "Out of memory\n"); exit(1); }
    memcpy(dup, s, len + 1);
    return dup;
}

char *qk_strndup(const char *s, int n) {
    if (!s) s = "";
    char *dup = (char *)malloc(n + 1);
    if (!dup) { fprintf(stderr, "Out of memory\n"); exit(1); }
    memcpy(dup, s, n);
    dup[n] = '\0';
    return dup;
}

void qk_fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Fatal: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

/* ============================================================
 * File reading
 * ============================================================ */

char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fprintf(stderr, "Error: Failed to determine file size for '%s'\n", path);
        fclose(file);
        return NULL;
    }

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory reading '%s'\n", path);
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    return buffer;
}

/* ============================================================
 * Run source code
 * ============================================================ */

/* run() removed - unused static function */

/* ============================================================
 * REPL
 * ============================================================ */

static void repl(void) {
    printf("Quokka v0.9.4  --  \"If you can read it, you can know what it does.\"\n");
    printf("Type expressions or statements. Ctrl+C to exit.\n\n");

    /* We keep one persistent interpreter across REPL lines
       so that bindings survive between inputs. */
    Interp interp;
    interp_init(&interp);

    char line[4096];
    for (;;) {
        printf("qk> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* Skip empty lines */
        if (line[0] == '\n' || line[0] == '\r') continue;

        Parser parser;
        parser_init(&parser, line);
        Node *program = parser_parse(&parser);

        if (parser.had_error) continue;

        Value result = interp_exec(&interp, program);

        /* Print non-unit results in the REPL */
        if (result.type != VAL_UNIT) {
            printf("=> ");
            val_print(result);
            printf("\n");
        }
        val_free(result);
    }

    interp_free(&interp);
    qk_ast_free_all();
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("Quokka v0.9.4\n");
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Quokka Programming Language v0.9.4\n\n");
            printf("Usage:\n");
            printf("  quokka                     Start interactive REPL\n");
            printf("  quokka <file.qk>           Execute a script\n");
            printf("  quokka run <file.qk>       Execute a script\n");
            printf("  quokka --vm <file.qk>      Execute a script via Bytecode VM\n");
            printf("  quokka --check <file.qk>   Run static type checker\n");
            printf("  quokka watch               Start Joey training monitor\n");
            printf("  quokka --version, -v       Show version\n");
            printf("  quokka --help, -h          Show this help message\n");
            return 0;
        }
        if (strcmp(argv[1], "--check") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: '--check' expects a file path\n");
                return 1;
            }
            char *src = read_file(argv[2]);
            if (!src) {
                fprintf(stderr, "Error: Could not read file '%s'\n", argv[2]);
                return 1;
            }
            Parser p;
            parser_init(&p, src);
            Node *ast = parser_parse(&p);
            free(src);
            if (!ast || p.had_error) {
                qk_ast_free_all();
                return 1;
            }
            bool ok = qk_typecheck_program(ast);
            qk_ast_free_all();
            if (ok) {
                printf("Typecheck passed: No errors found in %s\n", argv[2]);
                return 0;
            } else {
                return 1;
            }
        }
        if (strcmp(argv[1], "--vm") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: '--vm' expects a file path\n");
                return 1;
            }
            char *src = read_file(argv[2]);
            if (!src) {
                fprintf(stderr, "Error: Could not read file '%s'\n", argv[2]);
                return 1;
            }
            Parser p;
            parser_init(&p, src);
            Node *ast = parser_parse(&p);
            free(src);
            if (!ast || p.had_error) {
                qk_ast_free_all();
                return 1;
            }
            Interp interp;
            interp_init(&interp);
            QkFunctionObj *fn = qk_compile_ast(ast);
            if (!fn) {
                interp_free(&interp);
                qk_ast_free_all();
                return 1;
            }
            QkVM vm;
            qk_vm_init(&vm, &interp);
            QkVMResult res = qk_vm_run(&vm, fn);
            qk_function_obj_free(fn);
            qk_vm_free(&vm);
            interp_free(&interp);
            qk_ast_free_all();
            return (res == VM_RESULT_OK) ? 0 : 1;
        }
        if (strcmp(argv[1], "watch") == 0 || (argc >= 3 && strcmp(argv[1], "joey") == 0 && strcmp(argv[2], "status") == 0)) {
            extern int joey_watch_main(void);
            return joey_watch_main();
        }
    }
    if (argc == 1) {
        repl();
        return 0;
    }

    int script_idx = 1;
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'quokka run' expects a file path\n");
            return 1;
        }
        script_idx = 2;
    }

    // Pass arguments to Quokka scripts
    Value sys_args = val_list();
    for (int i = script_idx; i < argc; i++) {
        list_push(&sys_args, val_string(argv[i]));
    }

    char *source = read_file(argv[script_idx]);
    if (!source) {
        val_free(sys_args);
        return 1;
    }

    Interp interp;
    interp_init(&interp);
    env_define(interp.globals, "sys_args", sys_args, true);
    val_free(sys_args);

    /* Parse */
    Parser parser;
    parser_init(&parser, source);
    Node *program = parser_parse(&parser);

    if (parser.had_error) {
        free(source);
        interp_free(&interp);
        qk_ast_free_all();
        return 1;
    }

    /* Interpret */
    Value eval_result = interp_exec(&interp, program);

    int result = (interp.had_error || eval_result.type == VAL_ERR) ? 1 : 0;
    val_free(eval_result);
    free(source);
    interp_free(&interp);
    qk_ast_free_all();
    return result;
}
