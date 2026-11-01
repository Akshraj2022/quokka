#include "lsp.h"
#include "quokka.h"
#include "formatter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static void send_lsp_response(const char *content) {
    size_t len = strlen(content);
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", len, content);
    fflush(stdout);
}

/* Find string value for key in simple JSON */
static char *json_find_string(const char *json, const char *key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *pos = strstr(json, needle);
    if (!pos) return NULL;
    pos += strlen(needle);
    while (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    if (*pos != '"') return NULL;
    pos++;
    const char *end = pos;
    while (*end && (*end != '"' || *(end - 1) == '\\')) end++;
    size_t len = end - pos;
    char *res = (char *)malloc(len + 1);
    memcpy(res, pos, len);
    res[len] = '\0';
    return res;
}

/* Extract id (integer or string) */
static char *json_extract_id(const char *json) {
    const char *pos = strstr(json, "\"id\"");
    if (!pos) return strdup("1");
    pos += 4;
    while (*pos == ' ' || *pos == ':' || *pos == '\t') pos++;
    const char *end = pos;
    while (*end && *end != ',' && *end != '}' && *end != '\r' && *end != '\n') end++;
    size_t len = end - pos;
    char *res = (char *)malloc(len + 1);
    memcpy(res, pos, len);
    res[len] = '\0';
    return res;
}

/* Escape a string for JSON output */
static char *json_escape_string(const char *str) {
    if (!str) return strdup("");
    size_t cap = strlen(str) * 2 + 16, len = 0;
    char *out = (char *)malloc(cap);
    for (size_t i = 0; str[i]; i++) {
        char c = str[i];
        if (c == '"') { out[len++] = '\\'; out[len++] = '"'; }
        else if (c == '\\') { out[len++] = '\\'; out[len++] = '\\'; }
        else if (c == '\n') { out[len++] = '\\'; out[len++] = 'n'; }
        else if (c == '\r') { out[len++] = '\\'; out[len++] = 'r'; }
        else if (c == '\t') { out[len++] = '\\'; out[len++] = 't'; }
        else { out[len++] = c; }
    }
    out[len] = '\0';
    return out;
}

int qk_lsp_main(void) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    fprintf(stderr, "[Quokka LSP] Server initialized (v0.9.5)\n");
    fflush(stderr);

    char *doc_uri = NULL;
    char *doc_text = NULL;

    while (1) {
        char header[256];
        int content_length = 0;

        // Read headers until \r\n\r\n
        while (fgets(header, sizeof(header), stdin)) {
            if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0) {
                break;
            }
            if (sscanf(header, "Content-Length: %d", &content_length) == 1) {
                // found length
            }
        }

        if (content_length <= 0) break;

        char *body = (char *)malloc(content_length + 1);
        size_t total_read = 0;
        while (total_read < (size_t)content_length) {
            size_t r = fread(body + total_read, 1, content_length - total_read, stdin);
            if (r == 0) break;
            total_read += r;
        }
        body[total_read] = '\0';

        char *method = json_find_string(body, "method");
        char *id = json_extract_id(body);

        if (method) {
            if (strcmp(method, "initialize") == 0) {
                char resp[1024];
                snprintf(resp, sizeof(resp),
                    "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{"
                    "\"capabilities\":{"
                    "\"textDocumentSync\":1,"
                    "\"documentFormattingProvider\":true,"
                    "\"hoverProvider\":true"
                    "},"
                    "\"serverInfo\":{\"name\":\"quokka-lsp\",\"version\":\"0.9.5\"}"
                    "}}", id);
                send_lsp_response(resp);
            } else if (strcmp(method, "initialized") == 0) {
                // Notification, no response
            } else if (strcmp(method, "textDocument/didOpen") == 0 || strcmp(method, "textDocument/didChange") == 0) {
                char *uri = json_find_string(body, "uri");
                char *text = json_find_string(body, "text");

                if (uri) { free(doc_uri); doc_uri = uri; }
                if (text) {
                    free(doc_text);
                    doc_text = text;

                    // Run parser for diagnostics
                    Parser parser;
                    parser_init(&parser, doc_text);
                    Node *ast = parser_parse(&parser);
                    (void)ast;

                    char diag_resp[2048];
                    if (parser.had_error) {
                        snprintf(diag_resp, sizeof(diag_resp),
                            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{"
                            "\"uri\":\"%s\","
                            "\"diagnostics\":[{"
                            "\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":10}},"
                            "\"severity\":1,"
                            "\"source\":\"quokka\","
                            "\"message\":\"Syntax error in Quokka program\""
                            "}]}}", doc_uri ? doc_uri : "");
                    } else {
                        snprintf(diag_resp, sizeof(diag_resp),
                            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{"
                            "\"uri\":\"%s\","
                            "\"diagnostics\":[]}}", doc_uri ? doc_uri : "");
                    }
                    send_lsp_response(diag_resp);
                    qk_ast_free_all();
                }
            } else if (strcmp(method, "textDocument/formatting") == 0) {
                if (doc_text) {
                    char *formatted = qk_format_code(doc_text);
                    char *escaped = json_escape_string(formatted);
                    
                    // Count lines in original
                    int lines = 1;
                    for (int i = 0; doc_text[i]; i++) if (doc_text[i] == '\n') lines++;

                    size_t resp_len = strlen(escaped) + 512;
                    char *resp = (char *)malloc(resp_len);
                    snprintf(resp, resp_len,
                        "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":[{"
                        "\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":%d,\"character\":0}},"
                        "\"newText\":\"%s\""
                        "}]}", id, lines + 1, escaped);
                    send_lsp_response(resp);
                    free(resp);
                    free(escaped);
                    free(formatted);
                } else {
                    char resp[256];
                    snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":[]}", id);
                    send_lsp_response(resp);
                }
            } else if (strcmp(method, "textDocument/hover") == 0) {
                char resp[512];
                snprintf(resp, sizeof(resp),
                    "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{"
                    "\"contents\":{\"kind\":\"markdown\",\"value\":\"**Quokka Language** v0.9.5\\n\\nType-safe, concurrently verified expression language.\"}"
                    "}}", id);
                send_lsp_response(resp);
            } else if (strcmp(method, "shutdown") == 0) {
                char resp[256];
                snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":null}", id);
                send_lsp_response(resp);
            } else if (strcmp(method, "exit") == 0) {
                free(body);
                free(method);
                free(id);
                break;
            } else {
                // Method not supported, return null
                char resp[256];
                snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":null}", id);
                send_lsp_response(resp);
            }
            free(method);
        }
        free(id);
        free(body);
    }

    free(doc_uri);
    free(doc_text);
    return 0;
}
