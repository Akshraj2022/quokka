# Quokka Language Behavior Specification

## 1. Overview
Quokka is a dynamically-typed interpreted language designed to be self-hosted. 
The canonical Quokka interpreter must be written in Quokka itself. C is only used as a stage-0 bootstrap host.

## 2. Variables and Mutability
- By default, variables are immutable, defined with `let`.
- Mutable variables are defined with `let mut`.
- The `shadow` keyword allows redefining a variable with the same name in the same scope.

## 3. Data Types
- **Integer**: 64-bit signed integer.
- **Float**: 64-bit IEEE 754 floating-point number.
- **Boolean**: `true` or `false`.
- **String**: Immutable sequence of characters.

## 4. Control Flow
- `if`/`else`: Expression-based branching.
- `while`: Loop execution while a condition is true.
- `break` / `continue`: Standard loop controls.
- `match`: Pattern matching supporting structural patterns (`Some`, `None`, `Ok`, `Err`, `_`).

## 5. Functions
- Defined with the `fn` keyword.
- Lexical scoping and closures are supported.
- The `return` keyword is used for early returns; otherwise, functions evaluate to `Unit`.

## 6. Built-ins
- `println(val)`: Prints the value with a trailing newline.
- `print(val)`: Prints the value without a trailing newline.

## 7. Error Handling
- The language strictly avoids `null` or exceptions, favoring Monadic optionals (`Some`/`None`) and Results (`Ok`/`Err`).
- The `?` operator is a syntax sugar for early returns on `None` or `Err`.
