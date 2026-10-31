#include "quokka.h"

static char *unescape_string(const char *src, int len) {
    char *dest = qk_malloc(len + 1);
    int di = 0;
    for (int si = 0; si < len; si++) {
        if (src[si] == '\\' && si + 1 < len) {
            si++;
            switch (src[si]) {
                case 'n': dest[di++] = '\n'; break;
                case 't': dest[di++] = '\t'; break;
                case 'r': dest[di++] = '\r'; break;
                case '\\': dest[di++] = '\\'; break;
                case '"': dest[di++] = '"'; break;
                default: dest[di++] = src[si]; break;
            }
        } else {
            dest[di++] = src[si];
        }
    }
    dest[di] = '\0';
    return dest;
}

void nodelist_init(NodeList *list) {
    list->count = 0;
    list->cap = 0;
    list->items = NULL;
}

void nodelist_push(NodeList *list, Node *node) {
    if (list->count + 1 > list->cap) {
        list->cap = list->cap < 8 ? 8 : list->cap * 2;
        list->items = realloc(list->items, sizeof(Node*) * list->cap);
        if (!list->items) qk_fatal("Out of memory in nodelist_push");
    }
    list->items[list->count++] = node;
}

void paramlist_init(ParamList *list) {
    list->count = 0;
    list->cap = 0;
    list->items = NULL;
}

void paramlist_push(ParamList *list, Param p) {
    if (list->count + 1 > list->cap) {
        list->cap = list->cap < 8 ? 8 : list->cap * 2;
        list->items = realloc(list->items, sizeof(Param) * list->cap);
        if (!list->items) qk_fatal("Out of memory in paramlist_push");
    }
    list->items[list->count++] = p;
}

Node *node_new(NodeType type, int line) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) qk_fatal("Out of memory in node_new");
    n->type = type;
    n->line = line;
    return n;
}

void node_free(Node *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_STRING_LIT:
            if (node->as.string_val) free(node->as.string_val);
            break;
        case NODE_IDENT:
            if (node->as.name) free(node->as.name);
            break;
        case NODE_UNARY:
            node_free(node->as.unary.operand);
            break;
        case NODE_BINARY:
            node_free(node->as.binary.left);
            node_free(node->as.binary.right);
            break;
        case NODE_LET:
            if (node->as.let_bind.name) free(node->as.let_bind.name);
            if (node->as.let_bind.type_ann) free(node->as.let_bind.type_ann);
            node_free(node->as.let_bind.value);
            break;
        case NODE_ASSIGN:
            if (node->as.assign.name) free(node->as.assign.name);
            node_free(node->as.assign.value);
            break;
        case NODE_SHADOW:
            if (node->as.shadow.name) free(node->as.shadow.name);
            node_free(node->as.shadow.value);
            break;
        case NODE_IF:
            node_free(node->as.if_expr.cond);
            node_free(node->as.if_expr.then_b);
            node_free(node->as.if_expr.else_b);
            break;
        case NODE_MATCH:
            node_free(node->as.match_expr.subject);
            for (int i = 0; i < node->as.match_expr.arm_count; i++) {
                if (node->as.match_expr.arms[i].pattern) free(node->as.match_expr.arms[i].pattern);
                if (node->as.match_expr.arms[i].binding) free(node->as.match_expr.arms[i].binding);
                node_free(node->as.match_expr.arms[i].body);
            }
            if (node->as.match_expr.arms) free(node->as.match_expr.arms);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.stmts.count; i++) {
                node_free(node->as.block.stmts.items[i]);
            }
            if (node->as.block.stmts.items) free(node->as.block.stmts.items);
            break;
        case NODE_FN_DEF:
            if (node->as.fn_def.name) free(node->as.fn_def.name);
            for (int i = 0; i < node->as.fn_def.params.count; i++) {
                if (node->as.fn_def.params.items[i].name) free(node->as.fn_def.params.items[i].name);
                if (node->as.fn_def.params.items[i].type_name) free(node->as.fn_def.params.items[i].type_name);
            }
            if (node->as.fn_def.params.items) free(node->as.fn_def.params.items);
            if (node->as.fn_def.ret_type) free(node->as.fn_def.ret_type);
            node_free(node->as.fn_def.body);
            break;
        case NODE_LAMBDA:
            for (int i = 0; i < node->as.lambda.params.count; i++) {
                if (node->as.lambda.params.items[i].name) free(node->as.lambda.params.items[i].name);
                if (node->as.lambda.params.items[i].type_name) free(node->as.lambda.params.items[i].type_name);
            }
            if (node->as.lambda.params.items) free(node->as.lambda.params.items);
            if (node->as.lambda.ret_type) free(node->as.lambda.ret_type);
            node_free(node->as.lambda.body);
            break;
        case NODE_CALL:
            if (node->as.call.fn_name) free(node->as.call.fn_name);
            for (int i = 0; i < node->as.call.args.count; i++) {
                node_free(node->as.call.args.items[i]);
            }
            if (node->as.call.args.items) free(node->as.call.args.items);
            break;
        case NODE_RETURN:
            node_free(node->as.ret.value);
            break;
        case NODE_WHILE:
            node_free(node->as.while_loop.cond);
            node_free(node->as.while_loop.body);
            break;
        case NODE_FOR:
            if (node->as.for_loop.var_name) free(node->as.for_loop.var_name);
            node_free(node->as.for_loop.iter);
            node_free(node->as.for_loop.body);
            break;
        case NODE_SOME:
            node_free(node->as.some_val.value);
            break;
        case NODE_OK:
            node_free(node->as.ok_val.value);
            break;
        case NODE_ERR:
            node_free(node->as.err_val.value);
            break;
        case NODE_UNWRAP:
            node_free(node->as.unwrap.operand);
            break;
        case NODE_LIST_LIT:
            for (int i = 0; i < node->as.list_lit.elements.count; i++) {
                node_free(node->as.list_lit.elements.items[i]);
            }
            if (node->as.list_lit.elements.items) free(node->as.list_lit.elements.items);
            break;
        case NODE_INDEX:
            node_free(node->as.index.array);
            node_free(node->as.index.index);
            break;
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++) {
                node_free(node->as.program.items[i]);
            }
            if (node->as.program.items) free(node->as.program.items);
            break;
        case NODE_IMPORT:
            if (node->as.import_stmt.path) free(node->as.import_stmt.path);
            if (node->as.import_stmt.alias) free(node->as.import_stmt.alias);
            break;
        case NODE_STRUCT_DEF:
            if (node->as.struct_def.name) free(node->as.struct_def.name);
            for (int i = 0; i < node->as.struct_def.fields.count; i++) {
                if (node->as.struct_def.fields.items[i].name) free(node->as.struct_def.fields.items[i].name);
                if (node->as.struct_def.fields.items[i].type_name) free(node->as.struct_def.fields.items[i].type_name);
            }
            if (node->as.struct_def.fields.items) free(node->as.struct_def.fields.items);
            break;
        case NODE_STRUCT_CONSTRUCT:
            if (node->as.struct_construct.name) free(node->as.struct_construct.name);
            break;
        case NODE_GET_FIELD:
            node_free(node->as.get_field.object);
            if (node->as.get_field.field) free(node->as.get_field.field);
            break;
        case NODE_SET_FIELD:
            node_free(node->as.set_field.object);
            if (node->as.set_field.field) free(node->as.set_field.field);
            node_free(node->as.set_field.value);
            break;
        default:
            break;
    }
    free(node);
}

static Node **tracked_roots = NULL;
static int tracked_roots_count = 0;
static int tracked_roots_cap = 0;

void qk_ast_track(Node *node) {
    if (!node) return;
    if (tracked_roots_count >= tracked_roots_cap) {
        tracked_roots_cap = tracked_roots_cap < 8 ? 8 : tracked_roots_cap * 2;
        tracked_roots = realloc(tracked_roots, sizeof(Node*) * tracked_roots_cap);
        if (!tracked_roots) qk_fatal("Out of memory tracking AST roots");
    }
    tracked_roots[tracked_roots_count++] = node;
}

void qk_ast_free_all(void) {
    for (int i = 0; i < tracked_roots_count; i++) {
        node_free(tracked_roots[i]);
    }
    if (tracked_roots) {
        free(tracked_roots);
        tracked_roots = NULL;
    }
    tracked_roots_count = 0;
    tracked_roots_cap = 0;
}

void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->had_error = false;
    p->panic_mode = false;
}

static void error_at(Parser *p, Token *token, const char *message) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOK_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOK_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    p->had_error = true;
}

static void advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next(&p->lexer);
        if (p->current.type != TOK_ERROR) break;
        error_at(p, &p->current, p->current.start);
    }
}

static void consume(Parser *p, QkTokenType type, const char *message) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_at(p, &p->current, message);
}

static bool check(Parser *p, QkTokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, QkTokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static void synchronize(Parser *p) {
    p->panic_mode = false;
    while (p->current.type != TOK_EOF) {
        if (p->previous.type == TOK_RBRACE) {
            // Only return if we actually advanced since the error, to prevent infinite loops
            // For a simple fix, just advance at least once if we're stuck.
            // Actually, just relying on switch below is safer.
        }
        switch (p->current.type) {
            case TOK_LET:
            case TOK_SHADOW:
            case TOK_FN:
            case TOK_WHILE:
            case TOK_FOR:
            case TOK_RETURN:
            case TOK_IF:
            case TOK_MATCH:
                return;
            default:
                ; /* Do nothing. */
        }
        advance(p);
    }
}

static Node *expression(Parser *p);
static Node *statement(Parser *p);
static Node *block_expr(Parser *p);

static Node *parse_primary(Parser *p) {
    if (match(p, TOK_INT)) {
        Node *n = node_new(NODE_INT_LIT, p->previous.line);
        n->as.int_val = strtoll(p->previous.start, NULL, 10);
        return n;
    }
    if (match(p, TOK_FLOAT)) {
        Node *n = node_new(NODE_FLOAT_LIT, p->previous.line);
        n->as.float_val = strtod(p->previous.start, NULL);
        return n;
    }
    if (match(p, TOK_STRING)) {
        Node *n = node_new(NODE_STRING_LIT, p->previous.line);
        // Strip quotes and handle very basic escapes if needed, for now just extract content
        int len = p->previous.length - 2;
        if (len < 0) len = 0;
        char *s = unescape_string(p->previous.start + 1, len);
        n->as.string_val = s;
        return n;
    }
    if (match(p, TOK_TRUE)) {
        Node *n = node_new(NODE_BOOL_LIT, p->previous.line);
        n->as.bool_val = true;
        return n;
    }
    if (match(p, TOK_FALSE)) {
        Node *n = node_new(NODE_BOOL_LIT, p->previous.line);
        n->as.bool_val = false;
        return n;
    }
    if (match(p, TOK_NONE)) {
        return node_new(NODE_NONE, p->previous.line);
    }
    if (match(p, TOK_SOME)) {
        int line = p->previous.line;
        consume(p, TOK_LPAREN, "Expect '(' after Some");
        Node *v = expression(p);
        consume(p, TOK_RPAREN, "Expect ')' after Some value");
        Node *n = node_new(NODE_SOME, line);
        n->as.some_val.value = v;
        return n;
    }
    if (match(p, TOK_OK)) {
        int line = p->previous.line;
        consume(p, TOK_LPAREN, "Expect '(' after Ok");
        Node *v = expression(p);
        consume(p, TOK_RPAREN, "Expect ')' after Ok value");
        Node *n = node_new(NODE_OK, line);
        n->as.ok_val.value = v;
        return n;
    }
    if (match(p, TOK_ERR)) {
        int line = p->previous.line;
        consume(p, TOK_LPAREN, "Expect '(' after Err");
        Node *v = expression(p);
        consume(p, TOK_RPAREN, "Expect ')' after Err value");
        Node *n = node_new(NODE_ERR, line);
        n->as.err_val.value = v;
        return n;
    }
    if (match(p, TOK_RETURN)) {
        int line = p->previous.line;
        Node *expr = NULL;
        if (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !check(p, TOK_COMMA)) {
            expr = expression(p);
        }
        Node *n = node_new(NODE_RETURN, line);
        n->as.ret.value = expr;
        return n;
    }
    if (match(p, TOK_IDENT)) {
        int line = p->previous.line;
        char *name = qk_strndup(p->previous.start, p->previous.length);
        if (match(p, TOK_LPAREN)) {
            Node *call_node = node_new(NODE_CALL, line);
            call_node->as.call.fn_name = name;
            nodelist_init(&call_node->as.call.args);
            if (!check(p, TOK_RPAREN)) {
                do {
                    nodelist_push(&call_node->as.call.args, expression(p));
                } while (match(p, TOK_COMMA));
            }
            consume(p, TOK_RPAREN, "Expect ')' after arguments");
            return call_node;
        } else {
            Node *n = node_new(NODE_IDENT, line);
            n->as.name = name;
            return n;
        }
    }
    if (match(p, TOK_FN)) {
        int line = p->previous.line;
        consume(p, TOK_LPAREN, "Expect '(' after fn");
        Node *n = node_new(NODE_LAMBDA, line);
        paramlist_init(&n->as.lambda.params);
        if (!check(p, TOK_RPAREN)) {
            do {
                consume(p, TOK_IDENT, "Expect parameter name");
                Param param;
                param.name = qk_strndup(p->previous.start, p->previous.length);
                param.type_name = NULL;
                if (match(p, TOK_COLON)) {
                    consume(p, TOK_IDENT, "Expect parameter type");
                    param.type_name = qk_strndup(p->previous.start, p->previous.length);
                }
                paramlist_push(&n->as.lambda.params, param);
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RPAREN, "Expect ')' after parameters");
        n->as.lambda.ret_type = NULL;
        if (match(p, TOK_ARROW)) {
            consume(p, TOK_IDENT, "Expect return type after ->");
            n->as.lambda.ret_type = qk_strndup(p->previous.start, p->previous.length);
        }
        n->as.lambda.body = block_expr(p);
        return n;
    }
    if (match(p, TOK_IF)) {
        int line = p->previous.line;
        Node *n = node_new(NODE_IF, line);
        n->as.if_expr.cond = expression(p);
        n->as.if_expr.then_b = block_expr(p);
        n->as.if_expr.else_b = NULL;
        if (match(p, TOK_ELSE)) {
            if (check(p, TOK_IF)) {
                // Not standard based on the grammar requested, but let's parse expression
                n->as.if_expr.else_b = expression(p);
            } else {
                n->as.if_expr.else_b = block_expr(p);
            }
        }
        return n;
    }
    if (match(p, TOK_MATCH)) {
        int line = p->previous.line;
        Node *n = node_new(NODE_MATCH, line);
        n->as.match_expr.subject = expression(p);
        consume(p, TOK_LBRACE, "Expect '{' after match subject");
        int cap = 4;
        n->as.match_expr.arms = qk_malloc(sizeof(MatchArm) * cap);
        n->as.match_expr.arm_count = 0;
        
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (n->as.match_expr.arm_count == cap) {
                cap *= 2;
                n->as.match_expr.arms = qk_realloc(n->as.match_expr.arms, sizeof(MatchArm) * cap);
            }
            MatchArm arm;
            arm.binding = NULL;
            if (match(p, TOK_SOME)) {
                arm.pattern = qk_strdup("Some");
                consume(p, TOK_LPAREN, "Expect '('");
                consume(p, TOK_IDENT, "Expect identifier in Some");
                arm.binding = qk_strndup(p->previous.start, p->previous.length);
                consume(p, TOK_RPAREN, "Expect ')'");
            } else if (match(p, TOK_NONE)) {
                arm.pattern = qk_strdup("None");
            } else if (match(p, TOK_OK)) {
                arm.pattern = qk_strdup("Ok");
                consume(p, TOK_LPAREN, "Expect '('");
                consume(p, TOK_IDENT, "Expect identifier in Ok");
                arm.binding = qk_strndup(p->previous.start, p->previous.length);
                consume(p, TOK_RPAREN, "Expect ')'");
            } else if (match(p, TOK_ERR)) {
                arm.pattern = qk_strdup("Err");
                consume(p, TOK_LPAREN, "Expect '('");
                consume(p, TOK_IDENT, "Expect identifier in Err");
                arm.binding = qk_strndup(p->previous.start, p->previous.length);
                consume(p, TOK_RPAREN, "Expect ')'");
            } else if (match(p, TOK_IDENT) && p->previous.length == 1 && p->previous.start[0] == '_') {
                arm.pattern = qk_strdup("_");
            } else {
                error_at(p, &p->current, "Expect pattern (Some, None, Ok, Err, _)");
                break;
            }
            consume(p, TOK_FAT_ARROW, "Expect '=>' after pattern");
            arm.body = expression(p);
            if (arm.body) { n->as.match_expr.arms[n->as.match_expr.arm_count++] = arm; }
            if (!match(p, TOK_COMMA)) break;
        }
        consume(p, TOK_RBRACE, "Expect '}' after match arms");
        return n;
    }
    if (match(p, TOK_RETURN)) {
        int line = p->previous.line;
        Node *val = NULL;
        if (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !check(p, TOK_COMMA)) {
            val = expression(p);
        }
        Node *n = node_new(NODE_RETURN, line);
        n->as.ret.value = val;
        return n;
    }
    if (match(p, TOK_LPAREN)) {
        Node *expr = expression(p);
        consume(p, TOK_RPAREN, "Expect ')' after expression");
        return expr;
    }
    if (match(p, TOK_LBRACKET)) {
        int line = p->previous.line;
        Node *n = node_new(NODE_LIST_LIT, line);
        nodelist_init(&n->as.list_lit.elements);
        if (!check(p, TOK_RBRACKET)) {
            do {
                nodelist_push(&n->as.list_lit.elements, expression(p));
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RBRACKET, "Expect ']' after list elements");
        return n;
    }
    if (check(p, TOK_LBRACE)) {
        return block_expr(p);
    }
    
    error_at(p, &p->current, "Expect expression");
    return NULL;
}

static Node *parse_postfix(Parser *p) {
    Node *expr = parse_primary(p);
    if (!expr) return NULL;
    
    while (true) {
        if (match(p, TOK_QUESTION)) {
            Node *unwrap = node_new(NODE_UNWRAP, p->previous.line);
            unwrap->as.unwrap.operand = expr;
            expr = unwrap;
        } else if (match(p, TOK_LBRACKET)) {
            Node *idx_node = node_new(NODE_INDEX, p->previous.line);
            idx_node->as.index.array = expr;
            idx_node->as.index.index = expression(p);
            consume(p, TOK_RBRACKET, "Expect ']' after index");
            expr = idx_node;
        } else if (match(p, TOK_DOT)) {
            consume(p, TOK_IDENT, "Expect property name after '.'");
            Node *dot = node_new(NODE_GET_FIELD, p->previous.line);
            dot->as.get_field.object = expr;
            dot->as.get_field.field = qk_strndup(p->previous.start, p->previous.length);
            expr = dot;
        } else {
            break;
        }
    }
    return expr;
}

static Node *parse_unary(Parser *p) {
    if (match(p, TOK_NOT) || match(p, TOK_MINUS)) {
        Token op = p->previous;
        Node *right = parse_unary(p);
        Node *n = node_new(NODE_UNARY, op.line);
        n->as.unary.op = op.type;
        n->as.unary.operand = right;
        return n;
    }
    return parse_postfix(p);
}

static Node *parse_mul(Parser *p) {
    Node *expr = parse_unary(p);
    while (match(p, TOK_STAR) || match(p, TOK_SLASH) || match(p, TOK_PERCENT)) {
        Token op = p->previous;
        Node *right = parse_unary(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *parse_add(Parser *p) {
    Node *expr = parse_mul(p);
    while (match(p, TOK_PLUS) || match(p, TOK_MINUS)) {
        Token op = p->previous;
        Node *right = parse_mul(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *parse_concat(Parser *p) {
    Node *expr = parse_add(p);
    while (match(p, TOK_CONCAT)) {
        Token op = p->previous;
        Node *right = parse_add(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *parse_cmp(Parser *p) {
    Node *expr = parse_concat(p);
    while (match(p, TOK_LT) || match(p, TOK_GT) || match(p, TOK_LEQ) || match(p, TOK_GEQ)) {
        Token op = p->previous;
        Node *right = parse_concat(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *parse_eq(Parser *p) {
    Node *expr = parse_cmp(p);
    while (match(p, TOK_EQ) || match(p, TOK_NEQ)) {
        Token op = p->previous;
        Node *right = parse_cmp(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *parse_and(Parser *p) {
    Node *expr = parse_eq(p);
    while (match(p, TOK_AND)) {
        Token op = p->previous;
        Node *right = parse_eq(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *parse_or(Parser *p) {
    Node *expr = parse_and(p);
    while (match(p, TOK_OR)) {
        Token op = p->previous;
        Node *right = parse_and(p);
        Node *n = node_new(NODE_BINARY, op.line);
        n->as.binary.op = op.type;
        n->as.binary.left = expr;
        n->as.binary.right = right;
        expr = n;
    }
    return expr;
}

static Node *expression(Parser *p) {
    Node *expr = parse_or(p);
    // Unwraps handled in postfix
    return expr;
}

static Node *block_expr(Parser *p) {
    int line = p->current.line;
    consume(p, TOK_LBRACE, "Expect '{' to start block");
    Node *n = node_new(NODE_BLOCK, line);
    nodelist_init(&n->as.block.stmts);
    
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        Node *stmt = statement(p);
        if (stmt) {
            nodelist_push(&n->as.block.stmts, stmt);
        }
        if (p->panic_mode) synchronize(p);
    }
    
    consume(p, TOK_RBRACE, "Expect '}' to end block");
    return n;
}

static Node *statement(Parser *p) {
    if (match(p, TOK_LET)) {
        int line = p->previous.line;
        bool is_mut = match(p, TOK_MUT);
        consume(p, TOK_IDENT, "Expect variable name after let");
        char *name = qk_strndup(p->previous.start, p->previous.length);
        char *type_ann = NULL;
        if (match(p, TOK_COLON)) {
            consume(p, TOK_IDENT, "Expect type after ':'");
            type_ann = qk_strndup(p->previous.start, p->previous.length);
        }
        consume(p, TOK_ASSIGN, "Expect '=' after variable declaration");
        Node *value = expression(p);
        Node *n = node_new(NODE_LET, line);
        n->as.let_bind.name = name;
        n->as.let_bind.is_mut = is_mut;
        n->as.let_bind.type_ann = type_ann;
        n->as.let_bind.value = value;
        return n;
    }
    if (match(p, TOK_SHADOW)) {
        int line = p->previous.line;
        consume(p, TOK_IDENT, "Expect variable name after shadow");
        char *name = qk_strndup(p->previous.start, p->previous.length);
        consume(p, TOK_ASSIGN, "Expect '=' after shadow variable name");
        Node *value = expression(p);
        Node *n = node_new(NODE_SHADOW, line);
        n->as.shadow.name = name;
        n->as.shadow.value = value;
        return n;
    }
    if (check(p, TOK_FN) && /* peek ahead: fn IDENT = definition, fn( = lambda */ true) {
        /* Save state to check if this is a definition or lambda */
        if (p->current.type == TOK_FN) {
            /* Peek: is the token AFTER 'fn' an identifier (definition) or '(' (lambda)? */
            Lexer save_lex = p->lexer;
            Token save_cur = p->current;
            advance(p); /* consume 'fn' */
            if (check(p, TOK_IDENT)) {
                /* It's a function definition: fn name(...) */
                int line = p->previous.line;
                advance(p); /* consume the name */
                char *name = qk_strndup(p->previous.start, p->previous.length);
                consume(p, TOK_LPAREN, "Expect '(' after function name");

                Node *n = node_new(NODE_FN_DEF, line);
                n->as.fn_def.name = name;
                paramlist_init(&n->as.fn_def.params);
                if (!check(p, TOK_RPAREN)) {
                    do {
                        consume(p, TOK_IDENT, "Expect parameter name");
                        Param param;
                        param.name = qk_strndup(p->previous.start, p->previous.length);
                        param.type_name = NULL;
                        if (match(p, TOK_COLON)) {
                            consume(p, TOK_IDENT, "Expect parameter type");
                            param.type_name = qk_strndup(p->previous.start, p->previous.length);
                        }
                        paramlist_push(&n->as.fn_def.params, param);
                    } while (match(p, TOK_COMMA));
                }
                consume(p, TOK_RPAREN, "Expect ')' after parameters");

                n->as.fn_def.ret_type = NULL;
                if (match(p, TOK_ARROW)) {
                    consume(p, TOK_IDENT, "Expect return type");
                    n->as.fn_def.ret_type = qk_strndup(p->previous.start, p->previous.length);
                }
                n->as.fn_def.body = block_expr(p);
                return n;
            } else {
                /* It's a lambda expression: fn(...) — restore and fall through to expression */
                p->lexer = save_lex;
                p->current = save_cur;
            }
        }
    }
    if (match(p, TOK_WHILE)) {
        int line = p->previous.line;
        Node *n = node_new(NODE_WHILE, line);
        n->as.while_loop.cond = expression(p);
        n->as.while_loop.body = block_expr(p);
        return n;
    }
    if (match(p, TOK_FOR)) {
        int line = p->previous.line;
        consume(p, TOK_IDENT, "Expect variable name after for");
        char *var_name = qk_strndup(p->previous.start, p->previous.length);
        consume(p, TOK_IN, "Expect 'in' after for variable");
        Node *iter = expression(p);
        Node *body = block_expr(p);
        Node *n = node_new(NODE_FOR, line);
        n->as.for_loop.var_name = var_name;
        n->as.for_loop.iter = iter;
        n->as.for_loop.body = body;
        return n;
    }
    if (match(p, TOK_BREAK)) {
        return node_new(NODE_BREAK, p->previous.line);
    }
    if (match(p, TOK_IMPORT)) {
        int line = p->previous.line;
        char *path = NULL;
        if (match(p, TOK_STRING)) {
            int len = p->previous.length - 2;
            path = unescape_string(p->previous.start + 1, len);
        } else if (match(p, TOK_IDENT)) {
            path = qk_strndup(p->previous.start, p->previous.length);
        } else {
            error_at(p, &p->current, "Expect string path or identifier after 'import'");
            return NULL;
        }
        char *alias = NULL;
        if (match(p, TOK_AS)) {
            consume(p, TOK_IDENT, "Expect alias identifier after 'as'");
            alias = qk_strndup(p->previous.start, p->previous.length);
        }
        Node *n = node_new(NODE_IMPORT, line);
        n->as.import_stmt.path = path;
        n->as.import_stmt.alias = alias;
        return n;
    }
    if (match(p, TOK_STRUCT)) {
        int line = p->previous.line;
        consume(p, TOK_IDENT, "Expect struct name");
        char *name = qk_strndup(p->previous.start, p->previous.length);
        consume(p, TOK_LBRACE, "Expect '{' after struct name");
        ParamList fields;
        paramlist_init(&fields);
        if (!check(p, TOK_RBRACE)) {
            do {
                consume(p, TOK_IDENT, "Expect field name");
                Param param;
                param.name = qk_strndup(p->previous.start, p->previous.length);
                param.type_name = NULL;
                if (match(p, TOK_COLON)) {
                    consume(p, TOK_IDENT, "Expect field type");
                    param.type_name = qk_strndup(p->previous.start, p->previous.length);
                }
                paramlist_push(&fields, param);
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RBRACE, "Expect '}' after struct fields");
        Node *n = node_new(NODE_STRUCT_DEF, line);
        n->as.struct_def.name = name;
        n->as.struct_def.fields = fields;
        return n;
    }
    if (match(p, TOK_CONTINUE)) {
        return node_new(NODE_CONTINUE, p->previous.line);
    }
    
    // Assignment or expression statement
    if (check(p, TOK_IDENT)) {
        Node *expr = expression(p);
        if (match(p, TOK_ASSIGN)) {
            if (expr && expr->type == NODE_IDENT) {
                Node *val = expression(p);
                Node *n = node_new(NODE_ASSIGN, expr->line);
                n->as.assign.name = expr->as.name;
                n->as.assign.value = val;
                free(expr);
                return n;
            } else if (expr && expr->type == NODE_GET_FIELD) {
                Node *val = expression(p);
                Node *n = node_new(NODE_SET_FIELD, expr->line);
                n->as.set_field.object = expr->as.get_field.object;
                n->as.set_field.field = expr->as.get_field.field;
                n->as.set_field.value = val;
                free(expr);
                return n;
            } else {
                error_at(p, &p->previous, "Invalid assignment target");
                return expr;
            }
        }
        return expr;
    }
    
    return expression(p);
}

Node *parser_parse(Parser *p) {
    advance(p);
    Node *prog = node_new(NODE_PROGRAM, 1);
    nodelist_init(&prog->as.program);
    qk_ast_track(prog);
    
    while (!check(p, TOK_EOF)) {
        Node *stmt = statement(p);
        if (stmt) nodelist_push(&prog->as.program, stmt);
        if (p->panic_mode) synchronize(p);
    }
    
    return prog;
}
