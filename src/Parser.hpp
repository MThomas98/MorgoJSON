#pragma once

#include "Value.hpp"
#include "Lexer.hpp"

#include <string_view>

class ParserError : public std::runtime_error
{
public:
    ParserError(std::string_view message, Token const& error_token);

    std::size_t col() const noexcept;
    std::size_t row() const noexcept;

private:
    std::size_t m_col = 0, m_row = 0;
};

class Parser
{
public:
    explicit Parser(std::string_view data);

    Value parse();

private:
    Value parseNext();

    Value parseLiteral();
    Value parseObject();
    Value parseArray();

    template <typename T>
    T* getTypedCurrentToken();

    template <typename T>
    T& expectTokenType();

    [[noreturn]] void error(std::string_view message, Token const& current_token) const;

    Lexer m_lexer;
    Token m_current_token;
};