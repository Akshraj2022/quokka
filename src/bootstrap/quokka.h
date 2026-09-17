/*============================================================
 * Quokka Programming Language v0.0
 * "If you can read it, you can know what it does."
 *
 * Master header — all shared types and function prototypes.
 * No implementations live here.
 *============================================================*/

#ifndef QUOKKA_H
#define QUOKKA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

/* ============================================================
 * Forward declarations
 * ============================================================ */
typedef struct Node Node;
typedef struct Value Value;
typedef struct Env Env;

/* ============================================================
 * Utility
 * ============================================================ */
char *qk_strdup(const char *s);
char *qk_strndup(const char *s, int n);

/* Fatal error — prints message and exits */
void qk_fatal(const char *fmt, ...);

/* ============================================================
 * Tokens
 * ============================================================ */
typedef enum {
    /* Literals */
    TOK_INT,            /* 42          */
    TOK_FLOAT,          /* 3.14        */
    TOK_STRING,         /* "hello"     */
    TOK_IDENT,          /* foo         */

    /* Keywords */
    TOK_LET,            /* let         */
    TOK_MUT,            /* mut         */
    TOK_SHADOW,         /* shadow      */
    TOK_FN,             /* fn          */
    TOK_RETURN,         /* return      */
    TOK_IF,             /* if          */
    TOK_ELSE,           /* else        */
    TOK_MATCH,          /* match       */
    TOK_WHILE,          /* while       */
    TOK_FOR,            /* for         */
    TOK_IN,             /* in          */
    TOK_TRUE,           /* true        */
    TOK_FALSE,          /* false       */
    TOK_AND,            /* and         */
    TOK_OR,             /* or          */
    TOK_NOT,            /* not         */
    TOK_SOME,           /* Some        */
    TOK_NONE,           /* None        */
    TOK_OK,             /* Ok          */
    TOK_ERR,            /* Err         */
    TOK_STRUCT,         /* struct      */
    TOK_TRAIT,          /* trait       */
    TOK_IMPL,           /* impl        */
    TOK_BREAK,          /* break       */
    TOK_CONTINUE,       /* continue    */

    /* Operators */
    TOK_PLUS,           /* +           */
    TOK_MINUS,          /* -           */
    TOK_STAR,           /* *           */
    TOK_SLASH,          /* /           */
    TOK_PERCENT,        /* %           */
    TOK_CONCAT,         /* ++          */
    TOK_EQ,             /* ==          */
    TOK_NEQ,            /* !=          */
    TOK_LT,             /* <           */
    TOK_GT,             /* >           */
    TOK_LEQ,            /* <=          */
    TOK_GEQ,            /* >=          */
    TOK_ASSIGN,         /* =           */
    TOK_QUESTION,       /* ?           */
    TOK_ARROW,          /* ->          */
    TOK_FAT_ARROW,      /* =>          */

    /* Delimiters */
    TOK_LPAREN,         /* (           */
    TOK_RPAREN,         /* )           */
    TOK_LBRACE,         /* {           */
    TOK_RBRACE,         /* }           */
    TOK_LBRACKET,       /* [           */
    TOK_RBRACKET,       /* ]           */
    TOK_COMMA,          /* ,           */
    TOK_COLON,          /* :           */
    TOK_DOT,            /* .           */

    /* Special */
    TOK_EOF,
    TOK_ERROR,
} TokenType;

typedef struct {
    TokenType type;
    const char *start;      /* pointer into source string */
    int length;
    int line;
} Token;

const char *tok_name(TokenType type);

/* ============================================================
 * Lexer
 * ============================================================ */
typedef struct {
    const char *start;      /* start of current token */
    const char *current;    /* current scan position  */
    int line;
} Lexer;

void lexer_init(Lexer *lex, const char *source);
Token lexer_next(Lexer *lex);

/* ============================================================
 * AST
 * ============================================================ */
typedef struct {
    Node **items;
    int count;
    int cap;
} NodeList;

typedef struct {
    char *name;
    char *type_name;        /* NULL if no annotation */
} Param;

typedef struct {
    Param *items;
    int count;
    int cap;
} ParamList;

typedef struct {
    char *pattern;          /* "Some", "None", "Ok", "Err", "_"  */
    char *binding;          /* variable bound in pattern, or NULL */
    Node *body;
} MatchArm;

typedef enum {
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_BOOL_LIT,
    NODE_IDENT,
    NODE_UNARY,
    NODE_BINARY,
    NODE_LET,
    NODE_ASSIGN,
    NODE_SHADOW,
    NODE_IF,
    NODE_MATCH,
    NODE_BLOCK,
    NODE_FN_DEF,
    NODE_LAMBDA,
    NODE_CALL,
    NODE_RETURN,
    NODE_WHILE,
    NODE_FOR,
    NODE_SOME,
    NODE_NONE,
    NODE_OK,
    NODE_ERR,
    NODE_UNWRAP,            /* postfix ? operator */
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_LIST_LIT,
    NODE_INDEX,
    NODE_PROGRAM,
} NodeType;

struct Node {
    NodeType type;
    int line;
    union {
        /* NODE_INT_LIT */
        int64_t int_val;

        /* NODE_FLOAT_LIT */
        double float_val;

        /* NODE_STRING_LIT */
        char *string_val;

        /* NODE_BOOL_LIT */
        bool bool_val;

        /* NODE_IDENT */
        char *name;

        /* NODE_UNARY:  op operand */
        struct { TokenType op; Node *operand; } unary;

        /* NODE_BINARY: left op right */
        struct { TokenType op; Node *left; Node *right; } binary;

        /* NODE_LET:    let [mut] name [: type] = value */
        struct { char *name; char *type_ann; Node *value; bool is_mut; } let_bind;

        /* NODE_ASSIGN: name = value */
        struct { char *name; Node *value; } assign;

        /* NODE_SHADOW: shadow name = value */
        struct { char *name; Node *value; } shadow;

        /* NODE_IF:     if cond { then } [else { alt }] */
        struct { Node *cond; Node *then_b; Node *else_b; } if_expr;

        /* NODE_MATCH:  match subject { arms... } */
        struct { Node *subject; MatchArm *arms; int arm_count; } match_expr;

        /* NODE_BLOCK:  { stmts... } */
        struct { NodeList stmts; } block;

        /* NODE_FN_DEF: fn name(params) [-> ret] { body } */
        struct { char *name; ParamList params; char *ret_type; Node *body; } fn_def;

        /* NODE_LAMBDA: fn(params) [-> ret] { body } */
        struct { ParamList params; char *ret_type; Node *body; } lambda;

        /* NODE_CALL:   callee(args...) */
        struct { char *fn_name; NodeList args; } call;

        /* NODE_RETURN: return value */
        struct { Node *value; } ret;

        /* NODE_WHILE:  while cond { body } */
        struct { Node *cond; Node *body; } while_loop;

        /* NODE_FOR:    for var in iter { body } */
        struct { char *var_name; Node *iter; Node *body; } for_loop;

        /* NODE_SOME:   Some(value) */
        struct { Node *value; } some_val;

        /* NODE_NONE:   (no payload) */

        /* NODE_OK:     Ok(value) */
        struct { Node *value; } ok_val;

        /* NODE_ERR:    Err(value) */
        struct { Node *value; } err_val;

        /* NODE_UNWRAP: expr? */
        struct { Node *operand; } unwrap;

        /* NODE_LIST_LIT: [expr...] */
        struct { NodeList elements; } list_lit;

        /* NODE_INDEX: array[index] */
        struct { Node *array; Node *index; } index;

        /* NODE_PROGRAM */
        NodeList program;
    } as;
};

Node *node_new(NodeType type, int line);
void nodelist_init(NodeList *list);
void nodelist_push(NodeList *list, Node *node);
void paramlist_init(ParamList *list);
void paramlist_push(ParamList *list, Param p);

/* ============================================================
 * Parser
 * ============================================================ */
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

void parser_init(Parser *p, const char *source);
Node *parser_parse(Parser *p);

/* ============================================================
 * Runtime Values
 * ============================================================ */
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_NONE,
    VAL_SOME,
    VAL_OK,
    VAL_ERR,
    VAL_FN,
    VAL_BUILTIN,
    VAL_UNIT,
    VAL_LIST,
} ValueType;

/* Built-in function pointer type */
typedef Value (*BuiltinFn)(Value *args, int argc, int line);

/* Quokka function / closure */
typedef struct {
    char *name;             /* NULL for lambdas */
    ParamList params;
    Node *body;
    Env *closure_env;       /* snapshot for capture-by-value */
} QkFunction;

/* Dynamic list */
typedef struct {
    Value *items;
    int count;
    int cap;
} QkList;

struct Value {
    ValueType type;
    union {
        int64_t     int_val;
        double      float_val;
        bool        bool_val;
        char       *string_val;
        Value      *inner;          /* Some(v), Ok(v), Err(v) */
        QkFunction  fn;
        BuiltinFn   builtin;
        QkList     *list;
    } as;
};

Value val_int(int64_t v);
Value val_float(double v);
Value val_bool(bool v);
Value val_string(const char *s);
Value val_none(void);
Value val_some(Value inner);
Value val_ok(Value inner);
Value val_err(Value inner);
Value val_unit(void);
Value val_fn(QkFunction f);
Value val_builtin(BuiltinFn f);
Value val_list(void);

const char *val_type_name(Value v);
void val_print(Value v);
bool val_equal(Value a, Value b);
Value val_clone(Value v);
void list_push(Value *list, Value item);

/* ============================================================
 * Environment (scope chain)
 * ============================================================ */
typedef struct Binding {
    char *name;
    Value value;
    bool is_mut;
    struct Binding *next;
} Binding;

struct Env {
    Binding *bindings;
    Env *parent;
};

Env  *env_new(Env *parent);
void  env_define(Env *env, const char *name, Value val, bool is_mut);
Value *env_get(Env *env, const char *name);
bool  env_set(Env *env, const char *name, Value val);
Env  *env_snapshot(Env *env);   /* deep-copy for closure capture */
void  env_free(Env *env);

/* ============================================================
 * Interpreter
 * ============================================================ */
typedef struct {
    Env *globals;
    Env *current_env;
    bool had_error;
    bool returning;
    Value return_val;
    bool breaking;
    bool continuing;
} Interp;

void  interp_init(Interp *interp);
Value interp_exec(Interp *interp, Node *node);
void  interp_free(Interp *interp);
void  interp_register_builtins(Interp *interp);

#endif /* QUOKKA_H */
