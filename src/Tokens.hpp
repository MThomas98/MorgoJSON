#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

struct Token
{
    struct LBrace    { static constexpr std::string_view name = "'{'"; };
    struct RBrace    { static constexpr std::string_view name = "'}'"; };
    struct LBracket  { static constexpr std::string_view name = "'['"; };
    struct RBracket  { static constexpr std::string_view name = "']'"; };
    struct Colon     { static constexpr std::string_view name = "':'"; };
    struct Comma     { static constexpr std::string_view name = "','"; };
    struct Null      { static constexpr std::string_view name = "null"; };
    struct EndOfFile { static constexpr std::string_view name = "EOF"; };

    struct Bool   
    { 
        static constexpr std::string_view name = "boolean"; 
        bool value; 
    };

    struct Number
    { 
        static constexpr std::string_view name = "number";  
        double value;
    };

    struct String {
        static constexpr std::string_view name = "string";
        std::string value; // NOTE: Value is a sequence of UTF-8 bytes

    };

    using TokenValueType = std::variant<
        LBrace, RBrace, LBracket, RBracket, Colon, Comma,
        Null, EndOfFile, Bool, Number, String>;

    template <typename T>
    bool isType() const
    {
        return std::holds_alternative<T>(value);
    }

    bool isLiteralType()
    {
        return isType<Bool>() || isType<Number>() || 
               isType<String>() || isType<Null>();
    }

    std::string_view getTypeName() const
    {
        return std::visit([](auto const& token) { return token.name; }, value);
    }

    TokenValueType value;
    std::size_t col = 0, row = 0;
};