# Quokka Language Grammar

```ebnf
program        ::= statement* EOF

statement      ::= let_stmt
                 | shadow_stmt
                 | expr_stmt
                 | function_def
                 | return_stmt
                 | while_stmt
                 | break_stmt
                 | continue_stmt

let_stmt       ::= "let" "mut"? IDENTIFIER "=" expression
shadow_stmt    ::= "shadow" IDENTIFIER "=" expression
expr_stmt      ::= expression
return_stmt    ::= "return" expression?
while_stmt     ::= "while" expression block
break_stmt     ::= "break"
continue_stmt  ::= "continue"

function_def   ::= "fn" IDENTIFIER "(" parameters? ")" ("->" IDENTIFIER)? block
parameters     ::= IDENTIFIER (":" IDENTIFIER)? ("," IDENTIFIER (":" IDENTIFIER)?)*

block          ::= "{" statement* "}"

expression     ::= assignment

assignment     ::= IDENTIFIER "=" assignment
                 | logic_or

logic_or       ::= logic_and ( "or" logic_and )*
logic_and      ::= equality ( "and" equality )*

equality       ::= comparison ( ( "!=" | "==" ) comparison )*
comparison     ::= term ( ( ">" | ">=" | "<" | "<=" ) term )*
term           ::= factor ( ( "-" | "+" | "++" ) factor )*
factor         ::= unary ( ( "/" | "*" | "%" ) unary )*

unary          ::= ( "not" | "-" ) unary
                 | call

call           ::= primary ( "(" arguments? ")" | "?" )*
arguments      ::= expression ( "," expression )*

primary        ::= NUMBER | STRING | "true" | "false" 
                 | IDENTIFIER | "None" | "Some" "(" expression ")"
                 | "Ok" "(" expression ")" | "Err" "(" expression ")"
                 | "(" expression ")"
                 | "return" expression?
                 | "match" expression "{" match_arm* "}"
                 | "if" expression block ( "else" block )?

match_arm      ::= match_pattern "=>" expression ","
match_pattern  ::= "Some" "(" IDENTIFIER ")" 
                 | "None"
                 | "Ok" "(" IDENTIFIER ")"
                 | "Err" "(" IDENTIFIER ")"
                 | "_"
```
