# Quokka Modules, User-Defined Structs, Networking & FFI Guide

## 1. Modular Code Architecture & Imports

Quokka organizes codebases cleanly across files and directories using the `import` keyword:
```quokka
import "math_utils"
import "geometry/shapes" as shapes

let sum = add(10, 20)
let circle = shapes.Circle(5)
```

### Module Resolution Rules
1. Relative path resolution: `import "helpers"` searches `./helpers.qk`, `./helpers.qka`, and `./helpers/module.qk`.
2. Standard library resolution: If not found locally, the runtime searches `src/std/` and the installation library directory.
3. Cycle-Safe Deduplication: Each imported file is compiled and executed only once. Subsequent imports reuse the existing module environment without re-running side effects.

---

## 2. User-Defined Structs

Quokka supports first-class user-defined structs with named fields, constructor functions, and mutable property updates:
```quokka
struct Vector2 { x, y }

// Instantiation
let mut v = Vector2(3, 4)

// Property extraction
println(v.x) // 3
println(v.y) // 4

// Property mutation
v.x = 10
println(v.x) // 10
```

### Memory Management
Struct instances (`VAL_STRUCT`) are reference-counted with `QkStructInstance`. When references drop to zero, all field values and metadata are recursively deallocated.

---

## 3. Cross-Platform Networking Sockets

Quokka provides socket primitives that operate across Windows (Winsock2) and POSIX (Linux/macOS BSD sockets):

### TCP Client
```quokka
let sock_res = socket_connect("127.0.0.1", 8080)
match sock_res {
    Ok(fd) => {
        socket_send(fd, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        let data = socket_recv(fd, 4096)
        println("Received: " ++ unwrap_or(data, ""))
        socket_close(fd)
    },
    Err(e) => println("Connection failed: " ++ e)
}
```

### TCP Server
```quokka
let listen_res = socket_listen(9000)
match listen_res {
    Ok(server_fd) => {
        println("Server listening on port 9000...")
        let client_res = socket_accept(server_fd)
        match client_res {
            Ok(client_fd) => {
                socket_send(client_fd, "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!")
                socket_close(client_fd)
            },
            Err(e) => println("Accept failed: " ++ e)
        }
        socket_close(server_fd)
    },
    Err(e) => println("Listen failed: " ++ e)
}
```

---

## 4. Foreign Function Interface (FFI)

Quokka enables dynamic loading of native shared libraries without recompiling the interpreter:
- **Windows**: `LoadLibraryA` / `GetProcAddress` / `FreeLibrary`.
- **Linux & macOS**: `dlopen` / `dlsym` / `dlclose`.

```quokka
let lib_name = if os_name() == "windows" { "msvcrt.dll" } else { "libc.so.6" }
let handle_res = ffi_open(lib_name)

match handle_res {
    Ok(h) => {
        let has_puts = ffi_available(h, "puts")
        println("puts available: " ++ to_string(has_puts))
        ffi_close(h)
    },
    Err(e) => println("Could not open library: " ++ e)
}
```
All FFI operations return monadic `Result` types for deterministic error handling.
