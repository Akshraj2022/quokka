#include "formatter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void append_str(char **buf, size_t *cap, size_t *len, const char *str) {
    size_t slen = strlen(str);
    while (*len + slen + 1 >= *cap) {
        *cap = (*cap == 0) ? 1024 : (*cap * 2);
        *buf = (char *)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, str, slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

static void append_char(char **buf, size_t *cap, size_t *len, char c) {
    char s[2] = { c, '\0' };
    append_str(buf, cap, len, s);
}

/* Format an individual line's operators and commas outside string literals */
static char *format_line_tokens(const char *line) {
    size_t cap = 256, len = 0;
    char *out = (char *)malloc(cap);
    out[0] = '\0';

    bool in_str = false;
    char quote_char = 0;
    int n = (int)strlen(line);

    for (int i = 0; i < n; i++) {
        char c = line[i];

        // String literal handling
        if (!in_str && (c == '"' || c == '\'')) {
            in_str = true;
            quote_char = c;
            append_char(&out, &cap, &len, c);
            continue;
        } else if (in_str && c == quote_char && (i == 0 || line[i - 1] != '\\')) {
            in_str = false;
            append_char(&out, &cap, &len, c);
            continue;
        } else if (in_str) {
            append_char(&out, &cap, &len, c);
            continue;
        }

        // Comment handling: if '//', copy rest of line as-is
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            if (len > 0 && out[len - 1] != ' ') {
                append_char(&out, &cap, &len, ' ');
            }
            append_str(&out, &cap, &len, line + i);
            break;
        }

        // Comma formatting: ensure ", "
        if (c == ',') {
            append_char(&out, &cap, &len, ',');
            if (i + 1 < n && line[i + 1] != ' ' && line[i + 1] != '\0' && line[i + 1] != ')' && line[i + 1] != ']') {
                append_char(&out, &cap, &len, ' ');
            }
            continue;
        }

        // Two-character operators
        if (i + 1 < n) {
            char next = line[i + 1];
            if ((c == '=' && next == '=') ||
                (c == '!' && next == '=') ||
                (c == '<' && next == '=') ||
                (c == '>' && next == '=') ||
                (c == '-' && next == '>') ||
                (c == '=' && next == '>') ||
                (c == '+' && next == '+')) {
                if (len > 0 && out[len - 1] != ' ') append_char(&out, &cap, &len, ' ');
                append_char(&out, &cap, &len, c);
                append_char(&out, &cap, &len, next);
                if (i + 2 < n && line[i + 2] != ' ') append_char(&out, &cap, &len, ' ');
                i++;
                continue;
            }
        }

        // Single-character binary operators: =, +, -, *, /, %, <, >
        if (c == '=' || c == '+' || c == '*' || c == '/' || c == '%' || c == '<' || c == '>') {
            if (len > 0 && out[len - 1] != ' ') append_char(&out, &cap, &len, ' ');
            append_char(&out, &cap, &len, c);
            if (i + 1 < n && line[i + 1] != ' ' && line[i + 1] != '=') append_char(&out, &cap, &len, ' ');
            continue;
        }

        if (c == '-') {
            // Check if unary minus
            bool is_unary = false;
            if (len == 0) is_unary = true;
            else {
                char prev = out[len - 1];
                if (prev == ' ' && len >= 2) {
                    char prev2 = out[len - 2];
                    if (prev2 == '(' || prev2 == '[' || prev2 == ',' || prev2 == '=' || prev2 == '+' || prev2 == '-' || prev2 == '*' || prev2 == '/') {
                        is_unary = true;
                    }
                } else if (prev == '(' || prev == '[' || prev == ',') {
                    is_unary = true;
                }
            }
            if (is_unary) {
                append_char(&out, &cap, &len, '-');
            } else {
                if (len > 0 && out[len - 1] != ' ') append_char(&out, &cap, &len, ' ');
                append_char(&out, &cap, &len, '-');
                if (i + 1 < n && line[i + 1] != ' ') append_char(&out, &cap, &len, ' ');
            }
            continue;
        }

        append_char(&out, &cap, &len, c);
    }

    return out;
}

char *qk_format_code(const char *source) {
    if (!source) return NULL;

    size_t out_cap = strlen(source) * 2 + 512, out_len = 0;
    char *out = (char *)malloc(out_cap);
    out[0] = '\0';

    char *copy = strdup(source);
    char *line = strtok(copy, "\r\n");
    int indent_level = 0;
    bool last_was_blank = false;

    while (line) {
        char *trimmed = trim_whitespace(line);

        if (strlen(trimmed) == 0) {
            if (!last_was_blank && out_len > 0) {
                append_str(&out, &out_cap, &out_len, "\n");
                last_was_blank = true;
            }
            line = strtok(NULL, "\r\n");
            continue;
        }
        last_was_blank = false;

        // Count leading closing braces on this line to dedent before printing
        int leading_dedents = 0;
        int idx = 0;
        while (trimmed[idx] == '}' || trimmed[idx] == ')' || trimmed[idx] == ']') {
            leading_dedents++;
            idx++;
            while (trimmed[idx] == ' ') idx++;
        }

        int current_indent = indent_level - leading_dedents;
        if (current_indent < 0) current_indent = 0;

        // Apply indentation (4 spaces per level)
        for (int s = 0; s < current_indent * 4; s++) {
            append_char(&out, &out_cap, &out_len, ' ');
        }

        // Format operators and commas
        char *formatted_line = format_line_tokens(trimmed);
        append_str(&out, &out_cap, &out_len, formatted_line);
        append_str(&out, &out_cap, &out_len, "\n");

        // Calculate delta indent for subsequent lines
        bool in_s = false;
        char q = 0;
        int delta = 0;
        for (int i = 0; formatted_line[i]; i++) {
            char c = formatted_line[i];
            if (!in_s && (c == '"' || c == '\'')) { in_s = true; q = c; }
            else if (in_s && c == q && (i == 0 || formatted_line[i-1] != '\\')) { in_s = false; }
            else if (!in_s) {
                if (c == '/' && formatted_line[i+1] == '/') break;
                if (c == '{') delta++;
                else if (c == '}') delta--;
            }
        }
        free(formatted_line);

        indent_level += delta;
        if (indent_level < 0) indent_level = 0;

        line = strtok(NULL, "\r\n");
    }

    free(copy);
    return out;
}

int qk_format_file(const char *filepath, bool write_in_place, bool check_only) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open file '%s' for formatting\n", filepath);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    size_t rd = fread(buf, 1, size, f);
    buf[rd] = '\0';
    fclose(f);

    char *formatted = qk_format_code(buf);
    if (!formatted) {
        free(buf);
        return 1;
    }

    if (check_only) {
        bool diff = (strcmp(buf, formatted) != 0);
        free(buf);
        free(formatted);
        if (diff) {
            printf("Needs formatting: %s\n", filepath);
            return 1;
        } else {
            printf("Already formatted: %s\n", filepath);
            return 0;
        }
    }

    if (write_in_place) {
        FILE *wf = fopen(filepath, "wb");
        if (!wf) {
            fprintf(stderr, "Error: Could not write file '%s'\n", filepath);
            free(buf);
            free(formatted);
            return 1;
        }
        fwrite(formatted, 1, strlen(formatted), wf);
        fclose(wf);
        printf("Formatted: %s\n", filepath);
    } else {
        printf("%s", formatted);
    }

    free(buf);
    free(formatted);
    return 0;
}
