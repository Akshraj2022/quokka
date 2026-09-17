/*============================================================
 * Quokka Lexer — tokenizes .qk source into a stream of tokens
 *============================================================*/

#include "quokka.h"

/* ---- helpers ---- */

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

/* ---- Lexer core ---- */

void lexer_init(Lexer *lex, const char *source) {
    lex->start   = source;
    lex->current = source;
    lex->line    = 1;
}

static bool at_end(Lexer *lex) {
    return *lex->current == '\0';
}

static char advance(Lexer *lex) {
    char c = *lex->current;
    lex->current++;
    return c;
}

static char peek(Lexer *lex) {
    return *lex->current;
}

static char peek_next(Lexer *lex) {
    if (at_end(lex)) return '\0';
    return lex->current[1];
}

static bool match_char(Lexer *lex, char expected) {
    if (at_end(lex)) return false;
    if (*lex->current != expected) return false;
    lex->current++;
    return true;
}

static Token make_token(Lexer *lex, TokenType type) {
    Token t;
    t.type   = type;
    t.start  = lex->start;
    t.length = (int)(lex->current - lex->start);
    t.line   = lex->line;
    return t;
}

static Token error_token(Lexer *lex, const char *msg) {
    Token t;
    t.type   = TOK_ERROR;
    t.start  = msg;
    t.length = (int)strlen(msg);
    t.line   = lex->line;
    return t;
}

static void skip_whitespace(Lexer *lex) {
    for (;;) {
        char c = peek(lex);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance(lex);
                break;
            case '\n':
                lex->line++;
                advance(lex);
                break;
            case '/':
                if (peek_next(lex) == '/') {
                    /* line comment — skip to end of line */
                    while (!at_end(lex) && peek(lex) != '\n')
                        advance(lex);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* ---- keyword table ---- */

typedef struct {
    const char *word;
    int length;
    TokenType type;
} Keyword;

static Keyword keywords[] = {
    {"let",      3, TOK_LET},
    {"mut",      3, TOK_MUT},
    {"shadow",   6, TOK_SHADOW},
    {"fn",       2, TOK_FN},
    {"return",   6, TOK_RETURN},
    {"if",       2, TOK_IF},
    {"else",     4, TOK_ELSE},
    {"match",    5, TOK_MATCH},
    {"while",    5, TOK_WHILE},
    {"for",      3, TOK_FOR},
    {"in",       2, TOK_IN},
    {"true",     4, TOK_TRUE},
    {"false",    5, TOK_FALSE},
    {"and",      3, TOK_AND},
    {"or",       2, TOK_OR},
    {"not",      3, TOK_NOT},
    {"Some",     4, TOK_SOME},
    {"None",     4, TOK_NONE},
    {"Ok",       2, TOK_OK},
    {"Err",      3, TOK_ERR},
    {"struct",   6, TOK_STRUCT},
    {"trait",    5, TOK_TRAIT},
    {"impl",     4, TOK_IMPL},
    {"break",    5, TOK_BREAK},
    {"continue", 8, TOK_CONTINUE},
    {NULL,       0, TOK_ERROR},
};

static TokenType ident_type(const char *start, int length) {
    for (int i = 0; keywords[i].word != NULL; i++) {
        if (keywords[i].length == length &&
            memcmp(keywords[i].word, start, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

/* ---- scan individual token types ---- */

static Token scan_string(Lexer *lex) {
    while (!at_end(lex) && peek(lex) != '"') {
        if (peek(lex) == '\n') lex->line++;
        if (peek(lex) == '\\' && !at_end(lex)) {
            advance(lex); /* skip escaped char */
        }
        advance(lex);
    }
    if (at_end(lex)) {
        return error_token(lex, "Unterminated string literal");
    }
    advance(lex); /* closing " */
    return make_token(lex, TOK_STRING);
}

static Token scan_number(Lexer *lex) {
    while (is_digit(peek(lex))) advance(lex);

    bool is_float = false;
    if (peek(lex) == '.' && is_digit(peek_next(lex))) {
        is_float = true;
        advance(lex); /* consume '.' */
        while (is_digit(peek(lex))) advance(lex);
    }

    return make_token(lex, is_float ? TOK_FLOAT : TOK_INT);
}

static Token scan_identifier(Lexer *lex) {
    while (is_alnum(peek(lex))) advance(lex);

    int length = (int)(lex->current - lex->start);
    TokenType type = ident_type(lex->start, length);
    return make_token(lex, type);
}

/* ---- main entry point ---- */

Token lexer_next(Lexer *lex) {
    skip_whitespace(lex);
    lex->start = lex->current;

    if (at_end(lex)) return make_token(lex, TOK_EOF);

    char c = advance(lex);

    /* identifiers and keywords */
    if (is_alpha(c)) return scan_identifier(lex);

    /* numbers */
    if (is_digit(c)) return scan_number(lex);

    switch (c) {
        /* strings */
        case '"': return scan_string(lex);

        /* single-char tokens */
        case '(': return make_token(lex, TOK_LPAREN);
        case ')': return make_token(lex, TOK_RPAREN);
        case '{': return make_token(lex, TOK_LBRACE);
        case '}': return make_token(lex, TOK_RBRACE);
        case '[': return make_token(lex, TOK_LBRACKET);
        case ']': return make_token(lex, TOK_RBRACKET);
        case ',': return make_token(lex, TOK_COMMA);
        case ':': return make_token(lex, TOK_COLON);
        case '.': return make_token(lex, TOK_DOT);
        case '?': return make_token(lex, TOK_QUESTION);
        case '*': return make_token(lex, TOK_STAR);
        case '%': return make_token(lex, TOK_PERCENT);

        /* two-char operators */
        case '+':
            if (match_char(lex, '+')) return make_token(lex, TOK_CONCAT);
            return make_token(lex, TOK_PLUS);

        case '-':
            if (match_char(lex, '>')) return make_token(lex, TOK_ARROW);
            return make_token(lex, TOK_MINUS);

        case '/':
            return make_token(lex, TOK_SLASH);

        case '=':
            if (match_char(lex, '=')) return make_token(lex, TOK_EQ);
            if (match_char(lex, '>')) return make_token(lex, TOK_FAT_ARROW);
            return make_token(lex, TOK_ASSIGN);

        case '!':
            if (match_char(lex, '=')) return make_token(lex, TOK_NEQ);
            return error_token(lex, "Unexpected '!'. Use 'not' for logical negation.");

        case '<':
            if (match_char(lex, '=')) return make_token(lex, TOK_LEQ);
            return make_token(lex, TOK_LT);

        case '>':
            if (match_char(lex, '=')) return make_token(lex, TOK_GEQ);
            return make_token(lex, TOK_GT);
    }

    return error_token(lex, "Unexpected character");
}

/* ---- token name for debug ---- */

const char *tok_name(TokenType type) {
    switch (type) {
        case TOK_INT:        return "INT";
        case TOK_FLOAT:      return "FLOAT";
        case TOK_STRING:     return "STRING";
        case TOK_IDENT:      return "IDENT";
        case TOK_LET:        return "let";
        case TOK_MUT:        return "mut";
        case TOK_SHADOW:     return "shadow";
        case TOK_FN:         return "fn";
        case TOK_RETURN:     return "return";
        case TOK_IF:         return "if";
        case TOK_ELSE:       return "else";
        case TOK_MATCH:      return "match";
        case TOK_WHILE:      return "while";
        case TOK_FOR:        return "for";
        case TOK_IN:         return "in";
        case TOK_TRUE:       return "true";
        case TOK_FALSE:      return "false";
        case TOK_AND:        return "and";
        case TOK_OR:         return "or";
        case TOK_NOT:        return "not";
        case TOK_SOME:       return "Some";
        case TOK_NONE:       return "None";
        case TOK_OK:         return "Ok";
        case TOK_ERR:        return "Err";
        case TOK_STRUCT:     return "struct";
        case TOK_TRAIT:      return "trait";
        case TOK_IMPL:       return "impl";
        case TOK_BREAK:      return "break";
        case TOK_CONTINUE:   return "continue";
        case TOK_PLUS:       return "+";
        case TOK_MINUS:      return "-";
        case TOK_STAR:       return "*";
        case TOK_SLASH:      return "/";
        case TOK_PERCENT:    return "%";
        case TOK_CONCAT:     return "++";
        case TOK_EQ:         return "==";
        case TOK_NEQ:        return "!=";
        case TOK_LT:         return "<";
        case TOK_GT:         return ">";
        case TOK_LEQ:        return "<=";
        case TOK_GEQ:        return ">=";
        case TOK_ASSIGN:     return "=";
        case TOK_QUESTION:   return "?";
        case TOK_ARROW:      return "->";
        case TOK_FAT_ARROW:  return "=>";
        case TOK_LPAREN:     return "(";
        case TOK_RPAREN:     return ")";
        case TOK_LBRACE:     return "{";
        case TOK_RBRACE:     return "}";
        case TOK_LBRACKET:   return "[";
        case TOK_RBRACKET:   return "]";
        case TOK_COMMA:      return ",";
        case TOK_COLON:      return ":";
        case TOK_DOT:        return ".";
        case TOK_EOF:        return "EOF";
        case TOK_ERROR:      return "ERROR";
    }
    return "UNKNOWN";
}
