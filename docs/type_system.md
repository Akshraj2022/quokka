# Quokka Static Type System Specification

## Overview
Quokka provides compile-time semantic analysis and static type checking via the CLI flag:
```bash
quokka --check <script.qk>
```
or via the built-in function `typecheck(source_code)`.

The type checker traverses the Abstract Syntax Tree (AST), infers expression types, tracks variable definitions and mutability across nested environments, and validates type constraints before code execution.

---

## Type Tags & Representations
The type engine models all fundamental and compound types via `QkType`:
```c
typedef enum {
    QK_TYPE_ANY,      /* Dynamic / wildcard type */
    QK_TYPE_INT,      /* 64-bit integer */
    QK_TYPE_FLOAT,    /* 64-bit double */
    QK_TYPE_BOOL,     /* Boolean */
    QK_TYPE_STRING,   /* String */
    QK_TYPE_UNIT,     /* Unit () */
    QK_TYPE_OPTION,   /* Option<T> */
    QK_TYPE_RESULT,   /* Result<T, E> */
    QK_TYPE_LIST,     /* List<T> */
    QK_TYPE_STRUCT,   /* User-defined struct */
    QK_TYPE_FN,       /* Function signature */
    QK_TYPE_CHANNEL,  /* Thread-safe Channel<T> */
    QK_TYPE_THREAD,   /* Native OS Thread<T> */
    QK_TYPE_ERROR,    /* Type-checking error */
} QkTypeTag;
```

---

## Validation & Semantic Rules

### 1. Arithmetic Operators (`+`, `-`, `*`, `/`, `%`)
- Operands must be numeric (`Int` or `Float`).
- If either operand is `Float`, the operation evaluates to `Float`.
- If both operands are `Int`, the operation evaluates to `Int`.
- If string addition is attempted with `+`, a type diagnostic is emitted recommending the string concatenation operator `++`.

### 2. String Concatenation (`++`)
- Both left and right operands must evaluate to `String` (or `Any`).
- The expression evaluates to `String`.

### 3. Comparison & Equality
- Equality operators (`==`, `!=`) accept compatible types and evaluate to `Bool`.
- Relational operators (`<`, `<=`, `>`, `>=`) evaluate to `Bool`.

### 4. Conditional & Loop Expressions
- Condition expressions in `if` and `while` must evaluate to `Bool`.
- `for var in iter` requires `iter` to evaluate to `List<T>`. Inside the loop body, `var` is bound to type `T`.

### 5. Structs & Properties
- Struct definitions (`struct Name { f1, f2, ... }`) register their schema in the type environment.
- Struct instantiation (`Name(arg1, arg2, ...)`) checks argument count against field count.
- Field access (`obj.field`) checks whether `field` is declared on `obj`'s struct schema. Accessing an undeclared property yields a compile-time error.

### 6. Lists & Indexing
- Dynamic list literals `[e1, e2, ...]` infer their element type `T` from the first element.
- Indexing `list[idx]` enforces that `idx` evaluates to `Int`.

### 7. Monads & The Postfix `?` Operator
- `Some(v)` constructs `Option<T>`.
- `Ok(v)` constructs `Result<T, Any>`, and `Err(e)` constructs `Result<Any, E>`.
- The postfix `?` unwrap operator verifies that its operand is either an `Option` or `Result`.

### 8. Mutability & Assignment
- Reassignment (`x = expr`) checks that variable `x` is declared as mutable (`let mut`).
- Reassignment validates that the type of `expr` is compatible with the existing binding type of `x`.

---

## Diagnostics & Output
When `quokka --check` detects a violation, it outputs a diagnostic message with line number:
```text
[line 3] Type error: Arithmetic operator requires numeric operands
[line 7] Type error: Access of non-existent field on struct
```
If errors are encountered, the command exits with exit code `1`. When code is clean, it prints:
```text
Typecheck passed: No errors found in <file.qk>
```
and terminates with exit code `0`.
