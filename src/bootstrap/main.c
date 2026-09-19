/*============================================================
 * Quokka Language — Entry Point
 *
 * Usage:
 *   quokka                   — start REPL
 *   quokka <file.qka>        — run a .qka file
 *============================================================*/

#include "quokka.h"

/* ============================================================
 * Utility implementations
 * ============================================================ */

char *qk_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (!dup) { fprintf(stderr, "Out of memory\n"); exit(1); }
    memcpy(dup, s, len + 1);
    return dup;
}

char *qk_strndup(const char *s, int n) {
    if (!s) return NULL;
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

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

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

static int run(const char *source) {
    /* Parse */
    Parser parser;
    parser_init(&parser, source);
    Node *program = parser_parse(&parser);

    if (parser.had_error) {
        return 1;
    }

    /* Interpret */
    Interp interp;
    interp_init(&interp);
    interp_exec(&interp, program);

    int result = interp.had_error ? 1 : 0;
    interp_free(&interp);
    return result;
}

/* ============================================================
 * REPL
 * ============================================================ */

static void repl(void) {
    printf("Quokka v0.0  —  \"If you can read it, you can know what it does.\"\n");
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
    }

    interp_free(&interp);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char *argv[]) {
    if (argc == 1) {
        repl();
        return 0;
    }

    // Pass arguments to Quokka scripts
    Value sys_args = val_list();
    for (int i = 1; i < argc; i++) {
        list_push(&sys_args, val_string(argv[i]));
    }

    char *source = read_file(argv[1]);
    if (!source) {
        fprintf(stderr, "Could not open file '%s'\n", argv[1]);
        return 1;
    }

    Interp interp;
    interp_init(&interp);
    env_define(interp.globals, "sys_args", sys_args, true);

    /* Parse */
    Parser parser;
    parser_init(&parser, source);
    Node *program = parser_parse(&parser);

    if (parser.had_error) {
        free(source);
        interp_free(&interp);
        return 1;
    }

    /* Interpret */
    interp_exec(&interp, program);

    int result = interp.had_error ? 1 : 0;
    free(source);
    interp_free(&interp);
    return result;
}
