#pragma once

#include "Tokens.hpp"

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
    Lexer(std::string_view data);

    Token next();
    Token peek();

private:
    bool atEnd() const;

    char advance();
    void skipWhitespace();

    char lookAhead(std::size_t n = 1) const;

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
};