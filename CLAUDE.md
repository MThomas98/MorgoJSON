# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

MorgoJSON is a JSON parsing library written from scratch in C++23 (CMake ≥ 3.25). It is a learning project in early development: the lexer is implemented and tested, while the parser and value tree have not been started (`JSONObject::parse` is empty).

## Build and test

The existing `build/` directory is configured with Ninja in Debug mode.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # configure (first time only)
cmake --build build                                     # build library + tests
ctest --test-dir build --output-on-failure              # run all tests
ctest --test-dir build -R "Lexer.SurrogatePair"         # run one test (regex on Suite.Name)
build/tests/morgojson_tests --gtest_filter=Lexer.*      # or run the gtest binary directly
```

- Tests use GoogleTest v1.17.0, fetched with `FetchContent` during configure, so the first configure needs network access.
- Set `-DMORGOJSON_BUILD_TESTS=OFF` to build only the library.
- clangd reads `build/clangd/compile_commands.json` (set in `.clangd`). The `clangd_compile_commands` target writes it on every build as a copy of `build/compile_commands.json` with lowercase drive letters, because clangd's rename breaks when CMake's `C:/` paths don't match VS Code's `c:/`.

## Layout and architecture

- `include/morgojson/`: **public** headers, included as `<morgojson/...>`. `JSONObject` is the public entry point: `read()` loads a file into `m_data`, and its private `parse()` is where parsing will happen.
- `src/`: library implementation plus **private** headers (`Lexer.hpp`, `Tokens.hpp`), included with quotes. These are not part of the public API and must not be included from `include/`.
- New source files must be added to `src/CMakeLists.txt` explicitly (there is no globbing). Link against the `MorgoJSON::morgojson` alias.
- Planned pipeline: `JSONObject::read` → `Lexer` → recursive-descent parser → value tree (DOM).

### Tokens

`Token` (`src/Tokens.hpp`) holds a `std::variant` of nested tag structs, one per token kind. There is no separate `TokenType` enum: the variant alternative *is* the token kind. Payload-carrying kinds are `Bool`, `Number` (`double`) and `String` (decoded, stored as UTF-8 bytes in a `std::string`). Query the kind with `isType<T>()` and read payloads with `std::get<T>(token.value)`.

### Lexer

`Lexer` (`src/Lexer.hpp`, `src/Lexer.cpp`) is pull-based: it holds a `std::string_view` over the input and `next()` returns one `Token` at a time, ending with `Token::EndOfFile`. It follows RFC 8259:

- **Strings:** raw control characters (U+0000–U+001F) are rejected. Escapes are decoded, and `\uXXXX` escapes are converted to UTF-8 in `appendUnicode`. Surrogate pairs are combined into one code point; lone or misordered surrogates are rejected.
- **Numbers:** they are validated against the JSON number grammar and then converted with `std::from_chars`. Values too small for a `double` round to ±0, and values too large throw.
- **Keywords:** `true`, `false` and `null` must match exactly.
- **Errors:** the lexer throws `LexerError` (a `std::runtime_error` carrying `row()` and `col()`) on malformed input. Throw it through the private `[[noreturn]]` helpers: `error(message)` reports the current position (`m_pos`), and `errorAt(m_token_start, message)` reports the start of the token when the whole token is at fault (a number out of range, an unterminated string). Don't name a local variable `error` inside `Lexer` members, because it hides that helper.
- **Positions:** rows and columns are 1-based and count bytes. JSON tokens can't contain raw newlines, so the row only changes in `skipWhitespace()`. It counts `\n`, so `\r\n` line endings also work. There is no column counter: `colAt(pos)` computes `pos - m_line_start + 1`. Tokens report where they start (`m_token_start`, set in `lex()`).
- **Scanning:** the private `lex()` does the scanning. It looks at the first character without consuming it, so each `consume*` method starts at its token's first character.
- **Peeking:** `peek()` scans one token ahead and caches it in `m_peeked`, and `next()` returns the cached token first. `peek()` returns a reference that is valid until the next `next()` call. Scanning code belongs in `lex()`, not `next()`, so it isn't skipped when a token has been peeked.

### Tests

- `tests/Lexer_tests.cpp` holds the lexer tests (suite `Lexer`). They include the private `"Lexer.hpp"` directly, because the test target adds `src/` to its include path. The helpers `expectTokens<Ts...>(input)` (checks the full token sequence, including the trailing `EndOfFile`) and `nextAs<T>(input)` (lexes one token and returns its payload) keep individual tests short.
- `tests/tests.cpp` holds the `JSONObject` tests.
- New test files must be added to `tests/CMakeLists.txt`.
- JSON fixtures live in `tests/data/` and are found at runtime through the `MORGOJSON_TEST_DATA_DIR` compile definition (`test_data_dir / "file.json"`).

## Conventions

- Members use the `m_` prefix; methods are `camelCase`; `East const` style (`std::string const&`, `auto const`).
- File-local helpers go in an anonymous namespace at the top of the `.cpp` file.
- Error handling is mixed: `JSONObject::read` returns `bool` (and test helpers use `std::expected<T, std::error_code>`), while the lexer throws `LexerError`. Tests for malformed input use `EXPECT_THROW(..., std::runtime_error)`.
