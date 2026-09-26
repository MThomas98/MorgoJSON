#pragma once

#include <cstddef>
#include <string>
#include <variant>

struct Token
{
    struct LBrace {}; 
    struct RBrace {}; 
    struct LBracket {}; 
    struct RBracket {};
    struct Colon {}; 
    struct Comma {}; 
    struct Null {};
    struct EndOfFile {};
    
    struct Bool { bool value; };
    struct Number { double value; };
    struct String { std::string value; }; // NOTE: Storing value as a sequence of UTF-8 bytes

    using TokenValueType = std::variant<
        LBrace, RBrace, LBracket, RBracket, Colon, Comma,
        Null, EndOfFile, Bool, Number, String>;

    template <typename T>
    bool isType() const
    {
        return std::holds_alternative<T>(value);
    }

    TokenValueType value;
    std::size_t col = 0, row = 0;
};