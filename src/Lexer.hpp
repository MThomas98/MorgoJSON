#pragma once

#include "Tokens.hpp"

#include <string_view>

class Lexer
{
public:
    Lexer(std::string_view data);

    Token next();
    Token peek();

private:
    bool atEnd() const;

    char advance();
    char advancePastWhitespace();

    Token makeToken(Token::TokenValueType token) const;

    void consumeKeyword(std::string_view keyword);
    Token consumeString();
    Token consumeNumber();

    

    std::string_view m_data;
    std::size_t m_pos = 0;
    std::size_t m_col = 0, m_row = 0;
};