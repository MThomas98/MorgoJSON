# MorgoJSON

A JSON parsing library written from scratch in C++23.

## Status

| Component | State |
|-----------|-------|
| Lexer | Complete |
| Parser | Complete |
| Value tree | Complete |
| Public API | In progress |

## Requirements

- C++23 compiler (MSVC 19.38+, GCC 13+, Clang 17+)
- CMake 3.25 or newer
- Ninja (or another CMake generator of your choice)

## Building

```sh
# Configure (first time only — fetches GoogleTest over the network)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build library and tests
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure
```

To build only the library without tests:

```sh
cmake -S . -B build -G Ninja -DMORGOJSON_BUILD_TESTS=OFF
cmake --build build
```

## Running tests

```sh
# All tests
ctest --test-dir build --output-on-failure

# One suite by name (regex on Suite.Name)
ctest --test-dir build -R "Lexer.SurrogatePair"

# Or run the GoogleTest binary directly
build/tests/morgojson_tests --gtest_filter=Lexer.*
```

## Architecture

The parsing pipeline is: `JSONObject::read` → `Lexer` → `Parser` → `Value` tree.

### Lexer

Pull-based tokeniser (`src/Lexer.hpp`, `src/Lexer.cpp`). Holds a `std::string_view` over the input; `next()` returns one `Token` at a time, ending with `Token::EndOfFile`. Follows RFC 8259: raw control characters are rejected, `\uXXXX` escapes are decoded to UTF-8, surrogate pairs are combined, and lone or misordered surrogates are rejected.

### Token

`Token` (`src/Token.hpp`) holds a `std::variant` of tag structs — one per token kind. There is no separate enum: the active variant alternative *is* the kind. Payload kinds are `Bool`, `Number` (`double`), and `String` (decoded UTF-8). Query with `isType<T>()` and read payloads with `std::get<T>(token.value)`. Each token also carries its source `row` and `col` (1-based, byte-counted).

### Parser

Recursive-descent parser (`src/Parser.hpp`, `src/Parser.cpp`). Takes a `std::string_view` and returns a `Value` tree. Throws `ParserError` (a `std::runtime_error` with `row()` and `col()` accessors) on malformed input.

### Value

`Value` (`src/Value.hpp`) is a variant-based DOM node covering all six JSON types: `Null`, `Bool`, `Number` (`double`), `String` (UTF-8 `std::string`), `Array` (`std::vector<Value>`), and `Object` (`std::map<std::string, Value>`).

## Layout

```
include/morgojson/   Public headers (include as <morgojson/...>)
src/                 Implementation + private headers (Lexer, Token, Value, Parser)
tests/               GoogleTest test suite
tests/data/          JSON fixture files
cmake/               CMake helpers
```
