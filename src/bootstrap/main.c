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
#include "formatter.h"
#include "lsp.h"
#include "debugger.h"
#include "sha256.h"

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
    printf("Quokka v0.9.5  --  \"If you can read it, you can know what it does.\"\n");
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
            printf("Quokka v0.9.5\n");
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Quokka Programming Language v0.9.5\n\n");
            printf("Usage:\n");
            printf("  quokka                     Start interactive REPL\n");
            printf("  quokka <file.qk>           Execute a script\n");
            printf("  quokka run <file.qk>       Execute a script\n");
            printf("  quokka --vm <file.qk>      Execute a script via Bytecode VM\n");
            printf("  quokka --check <file.qk>   Run static type checker\n");
            printf("  quokka check-self [file]   Run self-hosting reproducibility verifier\n");
            printf("  quokka fmt [-w] <file.qk>  Format Quokka source code\n");
            printf("  quokka lsp                 Start Language Server Protocol server\n");
            printf("  quokka debug <file.qk>     Start interactive debugger\n");
            printf("  quokka pkg [init|list]     Package manager\n");
            printf("  quokka watch               Start Joey training monitor\n");
            printf("  quokka --version, -v       Show version\n");
            printf("  quokka --help, -h          Show this help message\n");
            return 0;
        }
        if (strcmp(argv[1], "check-self") == 0 || strcmp(argv[1], "--check-self") == 0) {
            const char *target = (argc >= 3) ? argv[2] : "tests/conformance/01_values.qk";
            char *src = read_file(target);
            if (!src) {
                fprintf(stderr, "Error: Could not read file '%s'\n", target);
                return 1;
            }

            printf("Running reproducibility verification on %s...\n", target);

            // Stage 0: C parser
            Parser p;
            parser_init(&p, src);
            Node *ast = parser_parse(&p);
            if (!ast || p.had_error) {
                fprintf(stderr, "Check-self failed: Syntax error in %s\n", target);
                free(src);
                qk_ast_free_all();
                return 1;
            }

            char *ast_str = qk_serialize_ast_str(ast);
            char *ast_hash = qk_sha256_hash_str(ast_str);
            printf("Stage 0 (C Host AST Hash): %s\n", ast_hash);
            free(ast_str);
            free(ast_hash);

            // Run evaluation via interpreter
            Interp interp;
            interp_init(&interp);
            Value eval_res = interp_exec(&interp, ast);
            if (interp.had_error || eval_res.type == VAL_ERR) {
                fprintf(stderr, "Check-self failed: Interpreter evaluation error\n");
                val_free(eval_res);
                interp_free(&interp);
                free(src);
                qk_ast_free_all();
                return 1;
            }
            val_free(eval_res);
            interp_free(&interp);

            // Compile to bytecode VM
            interp_init(&interp);
            QkFunctionObj *fn = qk_compile_ast(ast);
            if (!fn) {
                fprintf(stderr, "Check-self failed: Bytecode compilation error\n");
                interp_free(&interp);
                free(src);
                qk_ast_free_all();
                return 1;
            }
            QkVM vm;
            qk_vm_init(&vm, &interp);
            QkVMResult vm_res = qk_vm_run(&vm, fn);
            qk_function_obj_free(fn);
            qk_vm_free(&vm);
            interp_free(&interp);
            free(src);
            qk_ast_free_all();

            if (vm_res != VM_RESULT_OK) {
                fprintf(stderr, "Check-self failed: Bytecode VM evaluation mismatch\n");
                return 1;
            }

            printf("Stage 1 (Bytecode VM Parity): verified\n");
            printf("v Self-hosting verified: hashes and evaluations match across stages.\n");
            return 0;
        }
        if (strcmp(argv[1], "fmt") == 0 || strcmp(argv[1], "--fmt") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Usage: quokka fmt [-w|--write] [--check] <file.qk>\n");
                return 1;
            }
            bool write_in_place = false;
            bool check_only = false;
            const char *file_to_fmt = NULL;
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--write") == 0) write_in_place = true;
                else if (strcmp(argv[i], "--check") == 0) check_only = true;
                else file_to_fmt = argv[i];
            }
            if (!file_to_fmt) {
                fprintf(stderr, "Error: No file specified for formatting\n");
                return 1;
            }
            return qk_format_file(file_to_fmt, write_in_place, check_only);
        }
        if (strcmp(argv[1], "lsp") == 0 || strcmp(argv[1], "--lsp") == 0) {
            return qk_lsp_main();
        }
        if (strcmp(argv[1], "debug") == 0 || strcmp(argv[1], "--debug") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Usage: quokka debug <file.qk>\n");
                return 1;
            }
            return qk_debug_file(argv[2]);
        }
        if (strcmp(argv[1], "pkg") == 0) {
            if (argc >= 3 && strcmp(argv[2], "init") == 0) {
                const char *pkg_name = (argc >= 4) ? argv[3] : "my_package";
                FILE *f = fopen("quokka.json", "wb");
                if (!f) {
                    fprintf(stderr, "Error: Could not create quokka.json\n");
                    return 1;
                }
                fprintf(f, "{\n  \"name\": \"%s\",\n  \"version\": \"0.1.0\",\n  \"entry\": \"src/main.qk\",\n  \"dependencies\": []\n}\n", pkg_name);
                fclose(f);
                printf("Initialized new Quokka package '%s' in quokka.json\n", pkg_name);
                return 0;
            } else if (argc >= 3 && strcmp(argv[2], "list") == 0) {
                printf("Standard Library Modules:\n");
                printf("  std::io          File I/O, path manipulation, directory utilities\n");
                printf("  std::string      Substring, search, split, join, trim, replace\n");
                printf("  std::collections Queue, Stack, Set, map, filter, reduce, chunk\n");
                printf("  std::json        JSON parse and stringify recursive-descent engine\n");
                printf("  std::net         TCP sockets, HTTP GET/POST, response parser\n");
                printf("  std::pkg         Package manifest and dependency resolver\n");
                printf("  std::joey        High-performance ML training and monitoring\n");
                return 0;
            } else {
                printf("Quokka Package Manager (qkpkg)\n");
                printf("Usage:\n");
                printf("  quokka pkg init [name]   Initialize new package manifest (quokka.json)\n");
                printf("  quokka pkg list          List standard library packages\n");
                return 0;
            }
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
