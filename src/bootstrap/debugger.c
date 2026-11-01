#include "debugger.h"
#include "quokka.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_BREAKPOINTS 128

typedef struct {
    int breakpoints[MAX_BREAKPOINTS];
    int num_breakpoints;
    bool single_step;
    int step_over_line;
    int last_break_line;
    char **lines;
    int line_count;
    const char *filepath;
} DebugState;

static bool has_breakpoint(DebugState *state, int line) {
    for (int i = 0; i < state->num_breakpoints; i++) {
        if (state->breakpoints[i] == line) return true;
    }
    return false;
}

static void add_breakpoint(DebugState *state, int line) {
    if (has_breakpoint(state, line)) {
        printf("Breakpoint already exists at line %d\n", line);
        return;
    }
    if (state->num_breakpoints < MAX_BREAKPOINTS) {
        state->breakpoints[state->num_breakpoints++] = line;
        printf("Breakpoint %d set at line %d\n", state->num_breakpoints, line);
    } else {
        printf("Maximum breakpoints reached\n");
    }
}

static void remove_breakpoint(DebugState *state, int line) {
    for (int i = 0; i < state->num_breakpoints; i++) {
        if (state->breakpoints[i] == line) {
            for (int j = i; j < state->num_breakpoints - 1; j++) {
                state->breakpoints[j] = state->breakpoints[j + 1];
            }
            state->num_breakpoints--;
            printf("Removed breakpoint at line %d\n", line);
            return;
        }
    }
    printf("No breakpoint found at line %d\n", line);
}

static void print_source_around(DebugState *state, int current_line, int window) {
    int start = current_line - window;
    if (start < 1) start = 1;
    int end = current_line + window;
    if (end > state->line_count) end = state->line_count;

    for (int i = start; i <= end; i++) {
        const char *marker = (i == current_line) ? "=>" : "  ";
        const char *bp = has_breakpoint(state, i) ? "*" : " ";
        printf("%s%s %4d | %s\n", marker, bp, i, state->lines[i - 1]);
    }
}

static void print_help(void) {
    printf("\nQuokka Debugger (qkdb) Commands:\n");
    printf("  s, step             Step into next statement\n");
    printf("  n, next             Step over to next line\n");
    printf("  c, continue         Continue execution until next breakpoint\n");
    printf("  b <line>, break     Set breakpoint at line\n");
    printf("  d <line>, delete    Delete breakpoint at line\n");
    printf("  bl, breakpoints     List active breakpoints\n");
    printf("  p <var>, print      Print variable value in current scope\n");
    printf("  locals              List all local bindings in scope\n");
    printf("  l, list             Show source lines around current line\n");
    printf("  bt, backtrace       Show call stack depth\n");
    printf("  h, help             Show this help\n");
    printf("  q, quit             Quit debugger\n\n");
}

static void debugger_hook(Interp *interp, Node *node) {
    DebugState *state = (DebugState *)interp->debug_data;
    if (!state || node->line <= 0) return;

    // Only pause when line changes or step condition met
    bool at_breakpoint = has_breakpoint(state, node->line);
    bool should_pause = state->single_step || at_breakpoint || (state->step_over_line > 0 && node->line >= state->step_over_line);

    if (!should_pause || node->line == state->last_break_line) {
        return;
    }

    state->last_break_line = node->line;
    state->step_over_line = -1;

    printf("\n[qkdb] Break at %s:%d\n", state->filepath, node->line);
    print_source_around(state, node->line, 2);

    char cmd[256];
    while (1) {
        printf("(qkdb) ");
        fflush(stdout);
        if (!fgets(cmd, sizeof(cmd), stdin)) {
            exit(0);
        }

        // Trim newline
        size_t len = strlen(cmd);
        while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r')) {
            cmd[--len] = '\0';
        }

        if (len == 0) continue;

        if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            state->single_step = true;
            return;
        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            state->single_step = false;
            state->step_over_line = node->line + 1;
            return;
        } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            state->single_step = false;
            return;
        } else if (strncmp(cmd, "b ", 2) == 0 || strncmp(cmd, "break ", 6) == 0) {
            const char *arg = (cmd[1] == ' ') ? cmd + 2 : cmd + 6;
            int bp_line = atoi(arg);
            if (bp_line > 0) add_breakpoint(state, bp_line);
            else printf("Invalid line number: %s\n", arg);
        } else if (strncmp(cmd, "d ", 2) == 0 || strncmp(cmd, "delete ", 7) == 0) {
            const char *arg = (cmd[1] == ' ') ? cmd + 2 : cmd + 7;
            int bp_line = atoi(arg);
            if (bp_line > 0) remove_breakpoint(state, bp_line);
            else printf("Invalid line number: %s\n", arg);
        } else if (strcmp(cmd, "bl") == 0 || strcmp(cmd, "breakpoints") == 0) {
            printf("Breakpoints (%d total):\n", state->num_breakpoints);
            for (int i = 0; i < state->num_breakpoints; i++) {
                printf("  #%d at line %d\n", i + 1, state->breakpoints[i]);
            }
        } else if (strncmp(cmd, "p ", 2) == 0 || strncmp(cmd, "print ", 6) == 0) {
            const char *varname = (cmd[1] == ' ') ? cmd + 2 : cmd + 6;
            while (*varname == ' ') varname++;
            Env *env = interp->current_env ? interp->current_env : interp->globals;
            Value *val = env_get(env, varname);
            if (val) {
                printf("%s = ", varname);
                val_print(*val);
                printf(" (%s)\n", val_type_name(*val));
            } else {
                printf("Undefined variable '%s'\n", varname);
            }
        } else if (strcmp(cmd, "locals") == 0) {
            Env *env = interp->current_env ? interp->current_env : interp->globals;
            printf("Variables in scope:\n");
            for (Binding *b = env->bindings; b != NULL; b = b->next) {
                printf("  %s: %s = ", b->name, b->is_mut ? "mut" : "let");
                val_print(b->value);
                printf("\n");
            }
        } else if (strcmp(cmd, "l") == 0 || strcmp(cmd, "list") == 0) {
            print_source_around(state, node->line, 5);
        } else if (strcmp(cmd, "bt") == 0 || strcmp(cmd, "backtrace") == 0) {
            printf("Call stack depth: %d\n", interp->call_depth);
            printf("Current frame: %s:%d\n", state->filepath, node->line);
        } else if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            printf("Aborting debugger.\n");
            exit(0);
        } else {
            printf("Unknown command '%s'. Type 'help' for commands.\n", cmd);
        }
    }
}

int qk_debug_file(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filepath);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    size_t rd = fread(buf, 1, size, f);
    buf[rd] = '\0';
    fclose(f);

    // Count lines and split
    int max_lines = 16;
    for (size_t i = 0; i < rd; i++) if (buf[i] == '\n') max_lines++;

    char **lines = (char **)malloc(sizeof(char *) * max_lines);
    int line_count = 0;

    char *copy = strdup(buf);
    char *cur = copy;
    while (*cur) {
        char *line_start = cur;
        while (*cur && *cur != '\n' && *cur != '\r') cur++;
        if (*cur == '\r' && *(cur + 1) == '\n') { *cur = '\0'; cur += 2; }
        else if (*cur == '\n') { *cur = '\0'; cur++; }
        lines[line_count++] = line_start;
    }

    DebugState state;
    state.num_breakpoints = 0;
    state.single_step = true; // start paused at entry
    state.step_over_line = -1;
    state.last_break_line = -1;
    state.lines = lines;
    state.line_count = line_count;
    state.filepath = filepath;

    printf("============================================================\n");
    printf("Quokka Interactive Debugger v0.9.5\n");
    printf("Target: %s (%d lines)\n", filepath, line_count);
    printf("Type 'c' to run, 's' to step, 'b <line>' to break, 'h' for help.\n");
    printf("============================================================\n");

    Parser parser;
    parser_init(&parser, buf);
    Node *program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Cannot debug program due to parsing errors.\n");
        free(buf);
        free(copy);
        free(lines);
        qk_ast_free_all();
        return 1;
    }

    Interp interp;
    interp_init(&interp);
    interp.debug_hook = debugger_hook;
    interp.debug_data = &state;

    Value res = interp_exec(&interp, program);

    printf("\nProgram finished execution.\n");
    if (res.type != VAL_UNIT) {
        printf("Result: ");
        val_print(res);
        printf("\n");
    }

    val_free(res);
    interp_free(&interp);
    qk_ast_free_all();
    free(buf);
    free(copy);
    free(lines);
    return 0;
}
