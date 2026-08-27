# library-cpp-wrapper

A small, dependency-free **C++17 library** that wraps the standard
`<filesystem>` facilities into a friendly, consistent API with rich error
handling, configurable logging, and a wide range of file & directory
operations.

The library ships in two interchangeable flavors:

- **Modular** — `filemanager.h` (declarations) + `filemanager.cpp`
  (implementation) + `filemanager_errors.h` + `filemanager_glob.h`. Use this
  when you want to link against a static library.
- **Header-only** — `filemanager.hpp` (single self-contained file). Use this
  when you want to drop the library into a project without any build step.

Both flavors expose exactly the same API.

---

## Table of contents

1. [Features](#features)
2. [Project structure](#project-structure)
3. [Requirements](#requirements)
4. [Building](#building)
   - [Make (no CMake required)](#make-no-cmake-required)
   - [CMake](#cmake)
   - [Header-only mode](#header-only-mode)
5. [Quick start](#quick-start)
6. [Error handling](#error-handling)
7. [Logging](#logging)
8. [API reference](#api-reference)
   - [Configuration](#configuration)
   - [File creation & writing](#file-creation--writing)
   - [File deletion & movement](#file-deletion--movement)
   - [File reading](#file-reading)
   - [File metadata](#file-metadata)
   - [Directory operations](#directory-operations)
   - [File statistics](#file-statistics)
   - [Search](#search)
   - [Disk information](#disk-information)
   - [Path utilities](#path-utilities)
   - [Error helpers](#error-helpers)
   - [Deprecated aliases](#deprecated-aliases-backward-compatibility)
9. [Examples](#examples)
10. [Testing](#testing)
11. [Migration guide (v1 → v2)](#migration-guide-v1--v2)
12. [Changelog](#changelog)
13. [License](#license)

---

## Features

### File operations

- **create** a file with optional initial content
- **write** (overwrite), **append**, and **clear** (truncate)
- **read** a file as text (`std::string`), as lines (`std::vector<std::string>`),
  or as raw bytes (`std::vector<std::uint8_t>`)
- **print** a file to any `std::ostream`
- **delete**, **rename**, **move**, and **copy** files
- **binary I/O** for non-text data

### Directory operations

- **create** directories, optionally recursive (like `mkdir -p`)
- **remove** directories, optionally recursive (like `rm -rf`)
- **list** the immediate children of a directory
- **list recursively** every descendant path
- **find** files matching a glob pattern (`*.cpp`, `test_*.txt`, …)

### File metadata & statistics

- **exists**, **isFile**, **isDirectory**, **isEmpty**
- **fileSize** in bytes
- **lastModified** timestamp
- **setPermissions**
- **countLines**, **countWords**, **countChars** for text files

### Disk information

- **diskUsage** — total size of a file or directory tree
- **freeSpace** — available bytes on the containing filesystem

### Path utilities (static)

- **getFileName**, **getStem**, **getExtension**, **getParentPath**
- **joinPaths**, **getAbsolutePath**, **normalizePath**

### Error handling

- A structured **`ErrorCode`** enum covering every common filesystem failure
- A **`Result<T>`** type for value-returning operations (no exceptions needed)
- An optional **throw-on-error** mode that raises **`FileError`** exceptions
- A **last-error** slot queryable after every call
- All errors are translated from `std::error_code` into the structured enum

### Logging

- Three log levels: `Silent`, `Error` (default), `Info`
- Configurable output streams for info and error messages

### Backward compatibility

- The original v1 method names (`CatFile`, `TotalLinescode`, `dirfile`,
  `SizeFile`) are kept as inline deprecated aliases that delegate to the new
  equivalents — old code keeps compiling without changes.

---

## Project structure

```text
project/
├── core/
│   ├── filemanager.h            # Public API (modular)
│   ├── filemanager.hpp          # Header-only single-file version
│   ├── filemanager.cpp          # Implementation for modular version
│   ├── filemanager_errors.h     # ErrorCode, FileError, Result<T>
│   └── filemanager_glob.h       # Tiny internal glob matcher
├── examples/
│   ├── basic_usage.cpp          # Minimal walkthrough
│   └── advanced_features.cpp    # Full feature tour
├── tests/
│   └── test_filemanager.cpp     # Lightweight test suite (no deps)
├── CMakeLists.txt
├── makefile
└── README.md
```

---

## Requirements

- A **C++17**-compatible compiler (g++ 7+, clang++ 7+, MSVC 19.14+)
- For the CMake build: **CMake 3.15** or newer
- No third-party dependencies

---

## Building

### Make (no CMake required)

The Makefile builds everything with `g++` directly, so it works on any
machine that has `make` and a C++17 compiler.

```bash
# Build only the static library (liblibrary-cpp-wrapper.a)
make lib

# Build the library + both example programs
make examples

# Build the library + the test suite
make tests

# Run the test suite
make test

# Everything
make all examples tests

# Clean up build artifacts
make clean
```

### CMake

```bash
# Configure + build the library, examples, and tests in one go
cmake -S . -B build -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON
cmake --build build

# Run the test suite via CTest
cd build && ctest --output-on-failure
```

Available CMake options:

| Option               | Default | Description                              |
| -------------------- | :-----: | ---------------------------------------- |
| `BUILD_EXAMPLES`     | `OFF`   | Build the example programs.              |
| `BUILD_TESTS`        | `OFF`   | Build the test suite and register CTest. |
| `FSW_HEADER_ONLY`    | `OFF`   | Build as a header-only interface library.|

To install the library, headers, and CMake package files:

```bash
cmake --install build --prefix install
```

This produces:

```text
install/
├── include/
│   ├── filemanager.h
│   ├── filemanager.hpp
│   ├── filemanager_errors.h
│   └── filemanager_glob.h
└── lib/
    ├── liblibrary-cpp-wrapper.a
    └── cmake/library-cpp-wrapper/
        ├── library-cpp-wrapper-config.cmake
        └── library-cpp-wrapper-config-version.cmake
```

### Header-only mode

If you prefer not to link anything, just drop `core/filemanager.hpp` into
your project and `#include` it directly. **Do not** also link
`filemanager.cpp` in this mode — that would cause duplicate-symbol errors.

CMake users can enable this with `-DFSW_HEADER_ONLY=ON`.

---

## Quick start

```cpp
#include "filemanager.h"

int main() {
    FileManager fm;

    fm.createFile("data.txt", "hello from the library\n");
    fm.appendFile("data.txt", "second line\n");
    fm.printFile("data.txt");

    auto lines = fm.countLines("data.txt");
    if (lines) {
        std::cout << "lines: " << lines.value() << '\n';
    }

    fm.deleteFile("data.txt");
    return 0;
}
```

Compile (modular):

```bash
g++ -std=c++17 -Icore app.cpp core/filemanager.cpp -o app
```

Or with the static library:

```bash
g++ -std=c++17 -Icore app.cpp -L. -llibrary-cpp-wrapper -o app
```

Compile (header-only):

```bash
g++ -std=c++17 -Icore app.cpp -o app     # just include filemanager.hpp
```

---

## Error handling

The library exposes **three** complementary error-handling strategies. Pick
the one that best fits the surrounding codebase — they can even be mixed.

### 1. Inspect the return value

Every operation that returns a value uses `Result<T>`:

```cpp
auto r = fm.readFile("config.json");
if (r.ok()) {
    std::cout << r.value();
} else {
    std::cerr << "failed: " << r.error() << " (" << errorCodeLabel(r.code()) << ")\n";
}
```

Operations that do not return a value return `bool`:

```cpp
if (!fm.deleteFile("temp.log")) {
    std::cerr << "could not delete: " << fm.lastErrorMessage() << '\n';
}
```

### 2. Query the last-error slot

After any operation, the most recent error is available from the instance:

```cpp
fm.readFile("missing.txt");
if (fm.lastError() != fsw::ErrorCode::None) {
    std::cerr << "code=" << errorCodeLabel(fm.lastError())
              << " msg=" << fm.lastErrorMessage()
              << " path=" << fm.lastErrorPath() << '\n';
}
```

### 3. Enable throw-on-error

For code that prefers exceptions, flip the instance into throw mode:

```cpp
fm.setThrowOnError(true);
try {
    auto content = fm.readFile("missing.txt");
    // ...
} catch (const fsw::FileError& ex) {
    std::cerr << "code=" << errorCodeLabel(ex.code())
              << " what=" << ex.what()
              << " path=" << ex.path() << '\n';
}
```

The last-error slot is still updated before the throw, so a catch handler can
read either source.

### Error codes

| `ErrorCode`            | Label                  | When it is reported                                     |
| ---------------------- | ---------------------- | ------------------------------------------------------- |
| `None`                 | `none`                 | Operation succeeded.                                    |
| `FileNotFound`         | `file_not_found`       | The path does not exist.                                |
| `PermissionDenied`     | `permission_denied`    | The caller lacks required permissions.                  |
| `AlreadyExists`        | `already_exists`       | A path already exists where one was not expected.       |
| `NotAFile`             | `not_a_file`           | The path exists but is not a regular file.              |
| `NotADirectory`        | `not_a_directory`      | The path exists but is not a directory.                 |
| `DirectoryNotEmpty`    | `directory_not_empty`  | `removeDirectory(recursive=false)` on a non-empty dir.  |
| `InvalidArgument`      | `invalid_argument`     | An argument (e.g. empty path) was rejected.             |
| `PathTooLong`          | `path_too_long`        | The path exceeds the platform limit.                    |
| `OutOfSpace`           | `out_of_space`         | The device reported no space left.                      |
| `IOError`              | `io_error`             | Generic low-level I/O error.                            |
| `Unsupported`          | `unsupported`          | The operation is not supported for this path type.      |
| `Unknown`              | `unknown`              | Anything else.                                          |

---

## Logging

By default the library writes error messages to `std::cerr` and stays silent
on success. The behavior is configurable:

```cpp
fm.setLogLevel(fsw::LogLevel::Silent);  // no output at all
fm.setLogLevel(fsw::LogLevel::Error);   // errors only (default)
fm.setLogLevel(fsw::LogLevel::Info);    // errors + success messages

fm.setErrorStream(&my_error_stream);    // redirect errors (nullptr → std::cerr)
fm.setInfoStream(&my_info_stream);      // redirect info   (nullptr → std::cout)
```

A common pattern in libraries is to silence the library entirely and surface
errors only through the return value:

```cpp
fm.setLogLevel(fsw::LogLevel::Silent);
if (!fm.deleteFile(path)) {
    log_to_my_system("fsw: " + fm.lastErrorMessage());
}
```

---

## API reference

All public symbols live in the `fsw` namespace. A convenience alias
`using FileManager = fsw::FileManager;` is provided at file scope so existing
user code keeps compiling unchanged.

### Configuration

| Method                                              | Description                                   |
| --------------------------------------------------- | --------------------------------------------- |
| `void setLogLevel(LogLevel level)`                  | Set verbosity (`Silent` / `Error` / `Info`).  |
| `void setThrowOnError(bool enabled)`                | Enable throw-on-error mode.                   |
| `void setInfoStream(std::ostream* stream)`          | Redirect info messages. `nullptr` → `cout`.   |
| `void setErrorStream(std::ostream* stream)`         | Redirect error messages. `nullptr` → `cerr`.  |

### Last-error introspection

| Method                                              | Description                                   |
| --------------------------------------------------- | --------------------------------------------- |
| `bool lastOperationSucceeded() const noexcept`      | `true` if the last call succeeded.            |
| `ErrorCode lastError() const noexcept`              | Structured code from the last call.           |
| `const std::string& lastErrorMessage() const`       | Human-readable message from the last call.    |
| `const std::string& lastErrorPath() const`          | Path associated with the last failure.        |

### File creation & writing

| Method                                                              | Returns | Notes                                   |
| ------------------------------------------------------------------- | :-----: | --------------------------------------- |
| `createFile(path, content = "")`                                    | `bool`  | Overwrites if exists.                   |
| `writeFile(path, content)`                                          | `bool`  | Alias for `createFile`.                 |
| `appendFile(path, content)`                                         | `bool`  | Creates if absent.                      |
| `writeFileBinary(path, std::vector<std::uint8_t>)`                  | `bool`  | Raw byte write.                         |
| `clearFile(path)`                                                   | `bool`  | Truncate to zero bytes.                 |

### File deletion & movement

| Method                                                | Returns | Notes                                              |
| ----------------------------------------------------- | :-----: | -------------------------------------------------- |
| `deleteFile(path)`                                    | `bool`  | Idempotent — succeeds even if file already absent. |
| `renameFile(oldPath, newPath)`                        | `bool`  | Cross-filesystem moves are handled.                |
| `moveFile(src, dst)`                                  | `bool`  | Alias for `renameFile`.                            |
| `copyFile(src, dst, overwrite = true)`               | `bool`  | `overwrite=false` fails with `AlreadyExists`.      |

### File reading

| Method                                  | Returns                              | Notes                          |
| --------------------------------------- | ------------------------------------ | ------------------------------ |
| `readFile(path)`                        | `Result<std::string>`                | Whole file as text.            |
| `readLines(path)`                       | `Result<std::vector<std::string>>`   | One entry per line.            |
| `readFileBinary(path)`                  | `Result<std::vector<std::uint8_t>>`  | Whole file as raw bytes.       |
| `printFile(path, ostream = std::cout)`  | `bool`                               | Stream a file to an ostream.   |

### File metadata

| Method                                              | Returns                                  | Notes                                  |
| --------------------------------------------------- | ---------------------------------------- | -------------------------------------- |
| `exists(path)`                                      | `bool`                                   | noexcept.                              |
| `fileExists(path)`                                  | `bool`                                   | Alias for `exists`.                    |
| `isFile(path)`                                      | `bool`                                   | noexcept.                              |
| `isDirectory(path)`                                 | `bool`                                   | noexcept.                              |
| `isEmpty(path)`                                     | `bool`                                   | File: zero bytes. Dir: no entries.     |
| `fileSize(path)`                                    | `Result<std::uintmax_t>`                 |                                        |
| `lastModified(path)`                                | `Result<std::filesystem::file_time_type>`|                                        |
| `setPermissions(path, std::filesystem::perms)`      | `bool`                                   | Replaces permissions.                  |

### Directory operations

| Method                                                  | Returns                              | Notes                                  |
| ------------------------------------------------------- | ------------------------------------ | -------------------------------------- |
| `createDirectory(path, recursive = true)`               | `bool`                               | Like `mkdir -p`. Idempotent.           |
| `removeDirectory(path, recursive = false)`              | `bool`                               | `recursive=true` is `rm -rf`. Careful. |
| `listDirectory(path)`                                   | `Result<std::vector<std::string>>`   | Sorted, names only.                    |
| `listDirectoryRecursive(path)`                          | `Result<std::vector<std::string>>`   | Sorted, relative paths.                |

### File statistics

| Method                                              | Returns                       | Notes                            |
| --------------------------------------------------- | ----------------------------- | -------------------------------- |
| `countLines(path)`                                  | `Result<std::size_t>`         |                                  |
| `countWords(path)`                                  | `Result<std::size_t>`         | Whitespace-separated.            |
| `countChars(path, includeWhitespace = true)`        | `Result<std::size_t>`         |                                  |

### Search

| Method                                                | Returns                              | Notes                                  |
| ----------------------------------------------------- | ------------------------------------ | -------------------------------------- |
| `findFiles(path, pattern = "*")`                      | `Result<std::vector<std::string>>`   | Glob with `*` and `?`, recursive.      |

### Disk information

| Method                          | Returns                       | Notes                                  |
| ------------------------------- | ----------------------------- | -------------------------------------- |
| `diskUsage(path)`               | `Result<std::uintmax_t>`      | Recursive size sum for directories.    |
| `freeSpace(path)`               | `Result<std::uintmax_t>`      | Available bytes on the device.         |

### Path utilities

All static, all `noexcept`-friendly, all return `std::string`:

| Method                                | Example                                    |
| ------------------------------------- | ------------------------------------------ |
| `getFileName(path)`                   | `"/x/y/a.txt"` → `"a.txt"`                 |
| `getStem(path)`                       | `"/x/y/a.txt"` → `"a"`                     |
| `getExtension(path)`                  | `"/x/y/a.txt"` → `".txt"`                  |
| `getParentPath(path)`                 | `"/x/y/a.txt"` → `"/x/y"`                  |
| `joinPaths(a, b)`                     | `"a", "b/c"` → `"a/b/c"`                   |
| `getAbsolutePath(path)`               | `"."` → `"/home/user/project"`             |
| `normalizePath(path)`                 | `"a/./b//c/.."` → `"a/b/"`                 |

### Error helpers

| Symbol                                       | Description                                  |
| -------------------------------------------- | -------------------------------------------- |
| `enum class ErrorCode`                       | Structured error codes (see table above).    |
| `const char* errorCodeLabel(ErrorCode)`      | Stable snake_case label for a code.          |
| `class FileError : public std::runtime_error`| Thrown when throw-on-error is enabled.       |
| `template <typename T> class Result<T>`      | Value-or-error discriminated union.          |

`Result<T>` API:

```cpp
Result<T> r = fm.readFile("x.txt");
r.ok();                  // bool
explicit operator bool() // same as ok()
r.code();                // ErrorCode
r.error();               // const std::string& (empty when ok)
r.value();               // T& / const T&  (UB if !ok)
r.valueOr(fallback);     // T — never fails
```

### Deprecated aliases (backward compatibility)

The original v1 method names are kept so existing code keeps compiling.
They print to `std::cout`/`std::cerr` directly and do not return a value.

| v1 name              | Replaced by         |
| -------------------- | ------------------- |
| `CatFile(path)`      | `printFile(path)`   |
| `TotalLinescode(p)`  | `countLines(p)`     |
| `dirfile(path)`      | `listDirectory(p)`  |
| `SizeFile(path)`     | `fileSize(p)`       |

These aliases will be removed in v3.0.

---

## Examples

Two example programs are provided in `examples/`:

- **`basic_usage.cpp`** — minimal walkthrough of the most common operations.
- **`advanced_features.cpp`** — full tour of every feature including
  Result-based error handling, throw-on-error mode, custom log streams,
  glob search, binary I/O, permissions, disk usage, path utilities, and the
  deprecated aliases.

Build & run:

```bash
make examples
./basic_usage
./advanced_features
```

---

## Testing

The test suite in `tests/test_filemanager.cpp` is a lightweight, dependency-
free program with inline `CHECK` macros. It covers all major operations
including the error paths.

```bash
make tests
make test         # builds and runs the suite

# or via CMake:
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
(cd build && ctest --output-on-failure)
```

Expected output:

```text
=== FileManager test suite ===
[ RUN      ] testCreateAndReadFile
[       OK ] testCreateAndReadFile
... (15 tests total)
=== Summary ===
  Checks: 68
  Failed: 0
```

---

## Migration guide (v1 → v2)

The v2 release is **source-compatible** with v1: any existing code that called
the original `FileManager` methods will keep compiling and behaving the same
way. To take advantage of the new API:

1. **Switch to the new method names** for new code:

   | Old (still works)        | New                             |
   | ------------------------ | ------------------------------- |
   | `createFile(p, c)`       | (unchanged)                     |
   | `CatFile(p)`             | `printFile(p)`                  |
   | `TotalLinescode(p)`      | `countLines(p).valueOr(0)`      |
   | `dirfile(p)`             | `listDirectory(p).valueOr({})`  |
   | `SizeFile(p)`            | `fileSize(p).valueOr(0)`        |
   | `deleteFile(p)`          | (unchanged, now returns `bool`) |
   | `renameFile(a, b)`       | (unchanged, now returns `bool`) |

2. **Use the return value** instead of inspecting `std::cerr`:

   ```cpp
   // v1 style
   fm.deleteFile("x.txt");   // prints errors to stderr

   // v2 style
   if (!fm.deleteFile("x.txt")) {
       handle_error(fm.lastErrorMessage());
   }
   ```

3. **Or opt into exceptions**:

   ```cpp
   fm.setThrowOnError(true);
   try {
       fm.deleteFile("x.txt");
   } catch (const fsw::FileError& ex) {
       handle_error(ex.what());
   }
   ```

4. **Silence the library's stderr output** if you handle errors yourself:

   ```cpp
   fm.setLogLevel(fsw::LogLevel::Silent);
   ```

5. **Qualify the namespace** when using the new types:

   ```cpp
   fsw::ErrorCode code = fm.lastError();
   fsw::Result<std::string> r = fm.readFile("x.txt");
   ```

   Or import what you need:

   ```cpp
   using fsw::ErrorCode;
   using fsw::Result;
   using fsw::FileError;
   ```

---

## Changelog

### v2.0.0

**Added**

- Structured `ErrorCode` enum with 13 categories.
- `Result<T>` template for value-or-error returns.
- `FileError` exception class with structured code and associated path.
- `errorCodeLabel()` helper for stable string labels.
- `LogLevel` enum and configurable info/error output streams.
- Throw-on-error mode (`setThrowOnError`).
- Last-error introspection (`lastError`, `lastErrorMessage`, `lastErrorPath`).
- New file operations: `writeFile`, `appendFile`, `writeFileBinary`,
  `readFile`, `readLines`, `readFileBinary`, `clearFile`, `copyFile`,
  `moveFile`, `printFile` (streamable).
- New metadata ops: `exists`, `fileExists`, `isFile`, `isDirectory`,
  `isEmpty`, `lastModified`, `setPermissions`.
- New directory ops: `createDirectory` (recursive), `removeDirectory`
  (recursive), `listDirectory`, `listDirectoryRecursive`.
- New statistics: `countWords`, `countChars` (with whitespace toggle).
- New search: `findFiles` with glob (`*` and `?`) matching.
- New disk ops: `diskUsage`, `freeSpace`.
- New path utilities: `getFileName`, `getStem`, `getExtension`,
  `getParentPath`, `joinPaths`, `getAbsolutePath`, `normalizePath`.
- Examples: `examples/basic_usage.cpp`, `examples/advanced_features.cpp`.
- Tests: `tests/test_filemanager.cpp` (15 tests, 68 checks).
- CMake options: `BUILD_EXAMPLES`, `BUILD_TESTS`, `FSW_HEADER_ONLY`.
- Generated CMake package version file.
- `filemanager_glob.h` — internal glob matcher.

**Changed**

- Library version bumped to 2.0.0.
- All symbols moved into `fsw` namespace (with `FileManager` alias kept at
  file scope for backward compatibility).
- `deleteFile` and `renameFile` now return `bool`.
- `deleteFile` is now idempotent (deleting a non-existent file succeeds).
- All operations update an internal last-error slot.
- Header-only `.hpp` now mirrors the full new API.
- README completely rewritten.

**Deprecated** (will be removed in v3.0)

- `CatFile`, `TotalLinescode`, `dirfile`, `SizeFile` — kept as inline
  aliases that delegate to the new equivalents.

### v1.0.0

Initial release. Provided 7 methods on `FileManager`:

- `createFile`, `deleteFile`, `renameFile`, `CatFile`, `TotalLinescode`,
  `dirfile`, `SizeFile`.

All returned `void` and reported failures by printing to `std::cerr`.

---

## License

This library is provided as-is for educational and production use. See the
project repository for license details.
