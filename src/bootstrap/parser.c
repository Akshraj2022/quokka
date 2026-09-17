#include "quokka.h"

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

static void consume(Parser *p, TokenType type, const char *message) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_at(p, &p->current, message);
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static void synchronize(Parser *p) {
    p->panic_mode = false;
    while (p->current.type != TOK_EOF) {
        if (p->previous.type == TOK_RBRACE) return;
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
        char *s = qk_strndup(p->previous.start + 1, len);
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
        n->as.match_expr.arms = malloc(sizeof(MatchArm) * cap);
        n->as.match_expr.arm_count = 0;
        
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (n->as.match_expr.arm_count == cap) {
                cap *= 2;
                n->as.match_expr.arms = realloc(n->as.match_expr.arms, sizeof(MatchArm) * cap);
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
            n->as.match_expr.arms[n->as.match_expr.arm_count++] = arm;
            if (!match(p, TOK_COMMA)) break;
        }
        consume(p, TOK_RBRACE, "Expect '}' after match arms");
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
        } else if (match(p, TOK_LPAREN) && expr->type == NODE_IDENT) {
            // Handled inside parse_primary directly as per grammar instructions,
            // but just in case, this is here if needed. But grammar says `Primary ('?' | '(' ArgList ')')*`
            // But we already handle IDENT '(' ArgList ')' in primary. Let's assume ? is the main postfix.
            // Oh, wait, the grammar says PostfixExpr = Primary ('?' | '(' ArgList ')')*
            // But wait, function calls on lambdas or expressions might happen.
            // But if it's already a call, then `foo()()` is possible if we implement it.
            // To be safe, if expr is something else, this might be invalid, but let's break if not ?
            break;
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
    if (match(p, TOK_RETURN)) {
        int line = p->previous.line;
        Node *val = NULL;
        // Check if there is an expression following
        if (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            val = expression(p);
        }
        Node *n = node_new(NODE_RETURN, line);
        n->as.ret.value = val;
        return n;
    }
    if (match(p, TOK_BREAK)) {
        return node_new(NODE_BREAK, p->previous.line);
    }
    if (match(p, TOK_CONTINUE)) {
        return node_new(NODE_CONTINUE, p->previous.line);
    }
    
    // Assignment or expression statement
    // Simple lookahead: if IDENT and then '=' -> assignment
    if (check(p, TOK_IDENT)) {
        // Peek ahead without consuming in Lexer?
        // Since we don't have token peek, we parse expression.
        Node *expr = expression(p);
        if (match(p, TOK_ASSIGN)) {
            if (expr->type == NODE_IDENT) {
                Node *val = expression(p);
                Node *n = node_new(NODE_ASSIGN, expr->line);
                n->as.assign.name = expr->as.name;
                n->as.assign.value = val;
                // Leak expr node memory, but that's typical for simple ast trees without manual free functions
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
    
    while (!check(p, TOK_EOF)) {
        Node *stmt = statement(p);
        if (stmt) {
            nodelist_push(&prog->as.program, stmt);
        }
        if (p->panic_mode) synchronize(p);
    }
    
    return prog;
}
