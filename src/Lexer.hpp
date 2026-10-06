#pragma once

#include "Token.hpp"

#include <optional>
#include <stdexcept>
#include <string_view>

class LexerError : public std::runtime_error
{
public:
    LexerError(std::string_view message, std::size_t col, std::size_t row);

    std::size_t col() const noexcept;
    std::size_t row() const noexcept;

private:
    std::size_t m_col = 0, m_row = 0;
};

class Lexer
{
public:
    explicit Lexer(std::string_view data);

    Token next();
    Token const& peek();

private:
    Token lex();

    bool atEnd() const;
    void skipWhitespace();

    Token makeToken(Token::TokenValueType token) const;

    std::size_t colAt(std::size_t pos) const;

    [[noreturn]] void error(std::string_view message) const;
    [[noreturn]] void errorAt(std::size_t pos, std::string_view message) const;

    Token consumeChar(Token::TokenValueType token);
    void consumeKeyword(std::string_view keyword);
    Token consumeString();
    Token consumeNumber();

    void appendUnicode(std::string& str);

    std::string_view m_data;
    std::size_t m_pos = 0;
    std::size_t m_row = 1;
    std::size_t m_line_start = 0;
    std::size_t m_token_start = 0;

    std::optional<Token> m_peeked;
};