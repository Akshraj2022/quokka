# Quokka Bytecode Virtual Machine & Compiler Specification

## Overview
Quokka's bytecode execution engine comprises a high-performance linear chunk compiler and a stack-based virtual machine (`QkVM`). The VM can be invoked via the CLI flag:
```bash
quokka --vm <script.qk>
```
or via the built-in function `vm_eval(source_code)`.

---

## Architecture & Data Layout

### Bytecode Chunk (`QkChunk`)
A chunk represents an executable sequence of instructions alongside a constant pool and source line mappings:
```c
typedef struct {
    uint8_t *code;        /* Bytecode instruction stream */
    int     *lines;       /* Source line mapping per instruction */
    int      count;       /* Total instruction count */
    int      capacity;    /* Allocated capacity */
    Value   *constants;   /* Constant pool */
    int      const_count; /* Number of constants */
    int      const_capacity;
} QkChunk;
```

### Function Object (`QkFunctionObj`)
Each compiled function encapsulates a bytecode chunk, its arity, and its identifier:
```c
typedef struct QkFunctionObj {
    int arity;
    QkChunk chunk;
    char *name;
    int refcount;
} QkFunctionObj;
```

### Call Frame (`CallFrame`)
The VM maintains a fixed frame stack up to `QK_FRAMES_MAX` (256):
```c
typedef struct {
    QkFunctionObj *function;
    uint8_t *ip;          /* Instruction pointer */
    Value *slots;         /* Frame base pointer into the VM stack */
} CallFrame;
```

---

## Instruction Set Architecture (ISA)

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_CONSTANT` | `[uint16_t const_idx]` | `[] -> [value]` | Pushes constant from chunk pool |
| `OP_UNIT` | None | `[] -> [()]` | Pushes Unit value |
| `OP_TRUE` | None | `[] -> [true]` | Pushes Boolean true |
| `OP_FALSE` | None | `[] -> [false]` | Pushes Boolean false |
| `OP_POP` | None | `[value] -> []` | Discards the top of stack |
| `OP_DUP` | None | `[value] -> [value, value]` | Duplicates the top of stack |
| `OP_GET_LOCAL` | `[uint8_t slot]` | `[] -> [local_val]` | Pushes value at frame stack offset `slot` |
| `OP_SET_LOCAL` | `[uint8_t slot]` | `[value] -> [value]` | Updates local variable at `slot` (value remains on top) |
| `OP_GET_GLOBAL` | `[uint16_t name_idx]` | `[] -> [global_val]` | Looks up global identifier by name constant |
| `OP_DEFINE_GLOBAL` | `[uint16_t name_idx]` | `[value] -> [value]` | Defines global identifier in current environment |
| `OP_SET_GLOBAL` | `[uint16_t name_idx]` | `[value] -> [value]` | Rebinds existing global identifier |
| `OP_ADD` | None | `[a, b] -> [a + b]` | Arithmetic addition or string concatenation |
| `OP_SUBTRACT` | None | `[a, b] -> [a - b]` | Arithmetic subtraction |
| `OP_MULTIPLY` | None | `[a, b] -> [a * b]` | Arithmetic multiplication |
| `OP_DIVIDE` | None | `[a, b] -> [a / b]` | Arithmetic division (checks for division by zero) |
| `OP_MODULO` | None | `[a, b] -> [a % b]` | Modulo operation (checks for zero divisor) |
| `OP_CONCAT` | None | `[a, b] -> [a ++ b]` | Explicit string concatenation |
| `OP_EQUAL` | None | `[a, b] -> [a == b]` | Structural equality comparison |
| `OP_GREATER` | None | `[a, b] -> [a > b]` | Greater-than comparison |
| `OP_LESS` | None | `[a, b] -> [a < b]` | Less-than comparison |
| `OP_NOT` | None | `[a] -> [!a]` | Boolean negation |
| `OP_NEGATE` | None | `[a] -> [-a]` | Numeric unary negation |
| `OP_JUMP` | `[uint16_t offset]` | `[] -> []` | Unconditional forward relative jump |
| `OP_JUMP_IF_FALSE`| `[uint16_t offset]` | `[cond] -> [cond]` | Forward jump if top of stack is Boolean `false` |
| `OP_LOOP` | `[uint16_t offset]` | `[] -> []` | Unconditional backward relative jump |
| `OP_CALL` | `[uint8_t argc]` | `[callee, arg1..argN] -> [res]` | Dispatches native builtin or compiled function call |
| `OP_RETURN` | None | `[res] -> []` | Pops call frame and returns value to caller |
| `OP_BUILD_LIST` | `[uint16_t count]` | `[el1..elN] -> [list]` | Assembles dynamic list from stack elements |
| `OP_GET_INDEX` | None | `[target, idx] -> [val]` | Indexes list or string by integer offset |
| `OP_BUILD_STRUCT` | `[uint16_t name] [uint8_t count]` | `[arg1..argN] -> [struct]` | Constructs struct instance |
| `OP_GET_FIELD` | `[uint16_t name]` | `[struct] -> [field_val]` | Extracts struct property |
| `OP_SET_FIELD` | `[uint16_t name]` | `[struct, val] -> [()]` | Mutates struct property |
| `OP_SOME` | None | `[val] -> [Some(val)]` | Constructs Monadic Option |
| `OP_OK` | None | `[val] -> [Ok(val)]` | Constructs Monadic Result Ok |
| `OP_ERR` | None | `[val] -> [Err(val)]` | Constructs Monadic Result Err |
| `OP_HALT` | None | `[] -> []` | Terminates VM execution successfully |

---

## Memory & Zero-Leak Invariants
1. **Constant Table Ownership**: `qk_chunk_add_constant` claims ownership of the value passed into the chunk. When the chunk is freed via `qk_chunk_free`, all constants are recursively destructed.
2. **Local Scope Stack Discipline**: Upon leaving a block (`end_scope_with_result`), all variables declared within that block depth are popped, while preserving the expression result on top of the stack.
3. **Clean Shutdown**: `qk_vm_free` drains any residual stack elements, ensuring zero memory leaks under Valgrind.

---

## Interpreter & VM Parity Guarantees
In Quokka v0.9.5, the Bytecode VM and AST Tree-Walking Interpreter achieve 100% behavioral parity:
1. **Anonymous Lambdas & Closures**: `NODE_LAMBDA` nodes are compiled into constant function objects with runtime environment closure snapshots (`closure_env`), enabling first-class function returns and multi-threaded closure spawns.
2. **Thread Return Values**: Spawning threads from bytecode (`thread_spawn(fn() { return fib(20) })`) captures the parent execution context and executes with full environment linkage, ensuring thread-returning calculations evaluate identically under `--vm` and the interpreter.
3. **String Lexicographical Comparisons**: Binary comparison opcodes (`OP_GREATER`, `OP_LESS`) support lexicographical string comparisons (`"a" < "b"`), providing identical relational behavior.
4. **Module Imports**: `import "path.qk" as mod` is fully supported under the VM via `NODE_IMPORT` chunk compilation and dot-member invocation.
