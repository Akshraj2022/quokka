# Quokka Language Grammar (v0.9.4)

```ebnf
program        ::= statement* EOF

statement      ::= let_stmt
                 | shadow_stmt
                 | assign_stmt
                 | import_stmt
                 | struct_def
                 | function_def
                 | while_stmt
                 | for_stmt
                 | break_stmt
                 | continue_stmt
                 | return_stmt
                 | expr_stmt

import_stmt    ::= "import" STRING ("as" IDENTIFIER)?
struct_def     ::= "struct" IDENTIFIER "{" struct_fields? "}"
struct_fields  ::= IDENTIFIER ("," IDENTIFIER)*

let_stmt       ::= "let" "mut"? IDENTIFIER (":" IDENTIFIER)? "=" expression
shadow_stmt    ::= "shadow" IDENTIFIER "=" expression
assign_stmt    ::= (IDENTIFIER | postfix "." IDENTIFIER) "=" expression
return_stmt    ::= "return" expression?
while_stmt     ::= "while" expression block
for_stmt       ::= "for" IDENTIFIER "in" expression block
break_stmt     ::= "break"
continue_stmt  ::= "continue"
expr_stmt      ::= expression

function_def   ::= "fn" IDENTIFIER "(" parameters? ")" ("->" IDENTIFIER)? block
parameters     ::= IDENTIFIER (":" IDENTIFIER)? ("," IDENTIFIER (":" IDENTIFIER)?)*

block          ::= "{" statement* "}"

expression     ::= assignment

assignment     ::= (IDENTIFIER | postfix "." IDENTIFIER) "=" assignment
                 | logic_or

logic_or       ::= logic_and ( "or" logic_and )*
logic_and      ::= equality ( "and" equality )*

equality       ::= comparison ( ( "!=" | "==" ) comparison )*
comparison     ::= term ( ( ">" | ">=" | "<" | "<=" ) term )*
term           ::= factor ( ( "-" | "+" | "++" ) factor )*
factor         ::= unary ( ( "/" | "*" | "%" ) unary )*

unary          ::= ( "not" | "-" ) unary
                 | postfix

postfix        ::= primary ( "(" arguments? ")" 
                           | "?" 
                           | "[" expression "]" 
                           | "." IDENTIFIER )*

arguments      ::= expression ( "," expression )*

primary        ::= NUMBER 
                 | STRING 
                 | "true" 
                 | "false" 
                 | IDENTIFIER 
                 | "None" 
                 | "Some" "(" expression ")"
                 | "Ok" "(" expression ")" 
                 | "Err" "(" expression ")"
                 | "[" (expression ("," expression)*)? "]"
                 | "(" expression ")"
                 | block
                 | "match" expression "{" (match_arm ("," match_arm)*)? ","? "}"
                 | "if" expression block ( "else" block )?

match_arm      ::= match_pattern "=>" (expression | block)
match_pattern  ::= "Some" "(" IDENTIFIER ")" 
                 | "None"
                 | "Ok" "(" IDENTIFIER ")"
                 | "Err" "(" IDENTIFIER ")"
                 | "_"
```
