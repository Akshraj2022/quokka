# Quokka Standard Library Specification

Quokka v0.9.5 ships with a pure, high-performance, modular standard library located under `src/std/`.

---

## 1. `std::io` (`src/std/io.qk`)

System and file I/O operations with path manipulation helpers.

### Functions
* **`read_file(path: String) -> Result<String, String>`**: Reads the entire contents of a file into a UTF-8 string.
* **`write_file(path: String, content: String) -> Result<None, String>`**: Writes content to a file, overwriting existing contents.
* **`append_file(path: String, content: String) -> Result<None, String>`**: Appends string data to the end of a file.
* **`file_exists(path: String) -> Bool`**: Returns `true` if the specified file exists.
* **`read_lines(path: String) -> Result<List<String>, String>`**: Reads file and splits into a list of lines.
* **`join_path(dir: String, file: String) -> String`**: Joins path components using the system separator (`/` or `\`).
* **`basename(path: String) -> String`**: Extracts the file name component from a path.
* **`extension(path: String) -> String`**: Extracts the file extension (e.g., `"qk"`).

---

## 2. `std::string` (`src/std/string.qk`)

String manipulation, search, trimming, and formatting.

### Functions
* **`char_at(s: String, idx: Int) -> String`**: Returns the character at index `idx`.
* **`starts_with(s: String, prefix: String) -> Bool`**: Returns `true` if `s` begins with `prefix`.
* **`ends_with(s: String, suffix: String) -> Bool`**: Returns `true` if `s` terminates with `suffix`.
* **`index_of(s: String, needle: String) -> Int`**: Returns the 0-based index of `needle` in `s`, or `-1` if absent.
* **`contains(s: String, needle: String) -> Bool`**: Returns `true` if `needle` occurs in `s`.
* **`split(s: String, delimiter: String) -> List<String>`**: Splits string into chunks separated by `delimiter`.
* **`join(parts: List<String>, separator: String) -> String`**: Concatenates list elements with `separator`.
* **`trim(s: String) -> String`**: Trims whitespace from both ends.
* **`replace(s: String, old_sub: String, new_sub: String) -> String`**: Replaces all occurrences of `old_sub` with `new_sub`.
* **`repeat(s: String, count: Int) -> String`**: Repeats string `count` times.
* **`pad_left(s: String, length: Int, pad_char: String) -> String`**: Pads string on the left up to `length`.
* **`pad_right(s: String, length: Int, pad_char: String) -> String`**: Pads string on the right up to `length`.

---

## 3. `std::collections` (`src/std/collections.qk`)

Common data structures and functional algorithms.

### Structs
* **`Queue { items }`**: FIFO queue (`queue_new()`, `queue_push(q, item)`, `queue_pop(q)`, `queue_len(q)`).
* **`Stack { items }`**: LIFO stack (`stack_new()`, `stack_push(s, item)`, `stack_pop(s)`, `stack_len(s)`).
* **`Set { items }`**: Unique collection (`set_new()`, `set_add(s, item)`, `set_has(s, item)`, `set_to_list(s)`).

### Functional Utilities
* **`map_list(list, fn)`**: Transforms list elements via callback function.
* **`filter_list(list, predicate)`**: Returns elements satisfying `predicate(item) == true`.
* **`reduce_list(list, reducer, initial)`**: Folds list with accumulator.
* **`find_item(list, predicate)`**: Returns `Some(item)` or `None`.
* **`any_match(list, predicate)`**: Returns `true` if any element matches.
* **`all_match(list, predicate)`**: Returns `true` if all elements match.
* **`reverse_list(list)`**: Returns a reversed copy of the list.
* **`zip_lists(a, b)`**: Pairs corresponding elements into 2-tuples `[a[i], b[i]]`.
* **`flatten_list(nested)`**: Flattens nested list by one level.
* **`chunk_list(list, size)`**: Groups elements into sublists of length `size`.
* **`unique_list(list)`**: Removes duplicate entries.

---

## 4. `std::json` (`src/std/json.qk`)

Recursive descent JSON parser, serializer, and dictionary object.

### Structs
* **`JsonObject { keys, values }`**: Key-value dictionary.
  - `json_object_new()`: Creates empty object.
  - `json_object_set(obj, key, val)`: Sets property.
  - `json_object_get(obj, key)`: Returns `Some(val)` or `None`.

### Core Functions
* **`json_parse(text: String) -> Result<Value, String>`**: Parses JSON into native types (numbers, booleans, strings, lists, `JsonObject`).
* **`json_stringify(val: Value) -> String`**: Serializes any native value or `JsonObject` into compact JSON.

---

## 5. `std::net` (`src/std/net.qk`)

Networking streams and HTTP protocol handling.

### Structs
* **`TcpStream { sock, host, port }`**: Wrapper for active TCP sockets.
* **`HttpResponse { status, body, raw }`**: Structured HTTP response container.

### Functions
* **`connect(host: String, port: Int) -> Result<TcpStream, String>`**: Connects to remote endpoint.
* **`send(stream: TcpStream, data: String) -> Result<None, String>`**: Transmits payload.
* **`recv(stream: TcpStream, max_len: Int) -> Result<String, String>`**: Reads up to `max_len` bytes.
* **`close(stream: TcpStream)`**: Closes the socket connection.
* **`http_get(host: String, port: Int, path: String) -> Result<HttpResponse, String>`**: Sends HTTP GET.
* **`http_post(host: String, port: Int, path: String, body: String, content_type: String) -> Result<HttpResponse, String>`**: Sends HTTP POST.
* **`parse_http_response(raw: String) -> HttpResponse`**: Decodes wire-format HTTP responses into headers and body.

---

## 6. `std::pkg` (`src/std/pkg.qk`)

Package manifest and project workspace initialization.

### Structs
* **`PackageManifest { name, version, entry, dependencies }`**: Project manifest representation.

### Functions
* **`manifest_new(name: String, version: String, entry: String) -> PackageManifest`**: Creates a manifest.
* **`manifest_to_string(manifest: PackageManifest) -> String`**: Formats manifest as JSON.
* **`init_package(name: String) -> Result<None, String>`**: Writes default `quokka.json` to the current working directory.
