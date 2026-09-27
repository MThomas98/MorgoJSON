#pragma once

#include "Tokens.hpp"

#include <optional>
#include <stdexcept>
#include <string_view>

class LexerError : public std::runtime_error
{
public:
    LexerError(std::string const& message, std::size_t col, std::size_t row);

    std::size_t col() const noexcept;
    std::size_t row() const noexcept;

private:
    std::size_t m_col = 0, m_row = 0;
};

class Lexer
{
public:
    explicit Lexer(std::string_view data);

    // Returns the next token and moves past it. After EndOfFile, keeps
    // returning EndOfFile.
    Token next();

    // Returns the next token without moving past it, so the following next()
    // returns the same token. The reference is valid until the next call to
    // next().
    Token const& peek();

private:
    // Scans the next token from the input, ignoring any peeked token.
    Token lex();

    bool atEnd() const;
    void skipWhitespace();

    Token makeToken(Token::TokenValueType token) const;

    // 1-based column of the given offset. Only valid for offsets on the
    // current line, which holds for everything inside a token: JSON tokens
    // can't contain raw newlines, so lines only change in skipWhitespace().
    std::size_t colAt(std::size_t pos) const;

    // Throws a LexerError for the current position, or for the given offset.
    [[noreturn]] void error(std::string const& message) const;
    [[noreturn]] void errorAt(std::size_t pos, std::string const& message) const;

    Token consumeChar(Token::TokenValueType token);
    void consumeKeyword(std::string_view keyword);
    Token consumeString();
    Token consumeNumber();

    void appendUnicode(std::string& str);

    std::string_view m_data;
    std::size_t m_pos = 0;
    std::size_t m_row = 1;          // 1-based, like editors
    std::size_t m_line_start = 0;   // Offset of the first character on the current line
    std::size_t m_token_start = 0;  // Offset of the first character of the current token

    std::optional<Token> m_peeked;  // Token scanned by peek() but not yet returned by next()
};