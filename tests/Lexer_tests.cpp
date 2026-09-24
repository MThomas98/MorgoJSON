#include <gtest/gtest.h>

#include "Lexer.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    template <typename T>
    void expectNext(Lexer& lexer)
    {
        auto token = lexer.next();
        EXPECT_TRUE(token.isType<T>()) << "got variant index " << token.value.index();
    }

    template <typename... Ts>
    void expectTokens(std::string_view input)
    {
        SCOPED_TRACE(std::string("input: ") + std::string(input));
        Lexer lexer{input};
        (expectNext<Ts>(lexer), ...);
        expectNext<Token::EndOfFile>(lexer);
    }

    template <typename T>
    T nextAs(std::string_view input)
    {
        Lexer lexer{input};
        auto token = lexer.next();
        EXPECT_TRUE(token.isType<T>()) << "got variant index " << token.value.index();
        return std::get<T>(token.value);
    }
}

// ---- End of input ----

TEST(Lexer, EmptyInputIsEndOfFile)
{
    expectTokens<>("");
}

TEST(Lexer, WhitespaceOnlyIsEndOfFile)
{
    expectTokens<>(" \t\r\n ");
}

TEST(Lexer, EndOfFileRepeats)
{
    Lexer lexer{"{"};
    expectNext<Token::LBrace>(lexer);
    expectNext<Token::EndOfFile>(lexer);
    expectNext<Token::EndOfFile>(lexer);
}

// ---- Punctuation ----

TEST(Lexer, SingleCharacterTokens)
{
    expectTokens<Token::LBrace>("{");
    expectTokens<Token::RBrace>("}");
    expectTokens<Token::LBracket>("[");
    expectTokens<Token::RBracket>("]");
    expectTokens<Token::Colon>(":");
    expectTokens<Token::Comma>(",");
}

TEST(Lexer, PunctuationSequence)
{
    expectTokens<Token::LBrace, Token::RBrace, Token::LBracket, Token::RBracket,
                 Token::Colon, Token::Comma>("{}[]:,");
}

TEST(Lexer, SkipsWhitespaceBetweenTokens)
{
    expectTokens<Token::LBrace, Token::Colon, Token::RBrace>(" \t{\n\r :  }\n");
}

// ---- Keywords ----

TEST(Lexer, True)
{
    EXPECT_TRUE(nextAs<Token::Bool>("true").value);
}

TEST(Lexer, False)
{
    EXPECT_FALSE(nextAs<Token::Bool>("false").value);
}

TEST(Lexer, Null)
{
    expectTokens<Token::Null>("null");
}

TEST(Lexer, KeywordsInArray)
{
    expectTokens<Token::LBracket, Token::Bool, Token::Comma, Token::Bool,
                 Token::Comma, Token::Null, Token::RBracket>("[true, false, null]");
}

TEST(Lexer, AdjacentKeywordsAreSeparateTokens)
{
    // Rejecting this is the parser's job, not the lexer's.
    expectTokens<Token::Bool, Token::Null>("truenull");
}

TEST(Lexer, TruncatedKeywordThrows)
{
    for (std::string_view input : {"t", "tru", "fals", "nul"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}

TEST(Lexer, MisspelledKeywordThrows)
{
    for (std::string_view input : {"trUe", "fasle", "nill", "True", "NULL"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}

// ---- Invalid characters ----

TEST(Lexer, UnexpectedCharacterThrows)
{
    for (std::string_view input : {"@", "'", "=", "/", "+1", ".5"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}

TEST(Lexer, ThrowsAfterValidTokens)
{
    Lexer lexer{"[ ; ]"};
    expectNext<Token::LBracket>(lexer);
    EXPECT_THROW(lexer.next(), std::runtime_error);
}

// ---- Numbers (not implemented yet: remove DISABLED_ once they are) ----

TEST(Lexer, DISABLED_Integers)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("0").value, 0.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("42").value, 42.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-7").value, -7.0);
}

TEST(Lexer, DISABLED_Fractions)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("3.5").value, 3.5);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-0.25").value, -0.25);
}

TEST(Lexer, DISABLED_Exponents)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("1e3").value, 1000.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("1E+3").value, 1000.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-2.5e-2").value, -0.025);
}

TEST(Lexer, DISABLED_NumbersInArray)
{
    expectTokens<Token::LBracket, Token::Number, Token::Comma, Token::Number,
                 Token::RBracket>("[1, -2.5]");
}

TEST(Lexer, DISABLED_LeadingZeroIsNotPartOfTheNumber)
{
    // "01" lexes as 0 then 1; the parser rejects two numbers in a row.
    expectTokens<Token::Number, Token::Number>("01");
}

TEST(Lexer, DISABLED_MalformedNumbersThrow)
{
    for (std::string_view input : {"-", "1.", "1.e5", "1e", "1e+", "-a"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}

// ---- Strings ----

TEST(Lexer, EmptyString)
{
    EXPECT_EQ(nextAs<Token::String>(R"("")").value, "");
}

TEST(Lexer, SimpleString)
{
    EXPECT_EQ(nextAs<Token::String>(R"("hello")").value, "hello");
}

TEST(Lexer, StringKeepsInnerWhitespace)
{
    EXPECT_EQ(nextAs<Token::String>(R"("a b	c")").value, "a b\tc");
}

TEST(Lexer, SimpleEscapes)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\"\\\/\b\f\n\r\t")").value, "\"\\/\b\f\n\r\t");
}

TEST(Lexer, UnicodeEscape)
{
    EXPECT_EQ(nextAs<Token::String>(R"("A")").value, "A");
    EXPECT_EQ(nextAs<Token::String>(R"("é")").value, "\xC3\xA9");      // é
    EXPECT_EQ(nextAs<Token::String>(R"("€")").value, "\xE2\x82\xAC");  // €
}

TEST(Lexer, SurrogatePair)
{
    EXPECT_EQ(nextAs<Token::String>(R"("😀")").value, "\xF0\x9F\x98\x80");  // 😀
}

TEST(Lexer, KeyValuePair)
{
    expectTokens<Token::LBrace, Token::String, Token::Colon, Token::Bool,
                 Token::RBrace>(R"({"ok": true})");
}

TEST(Lexer, MalformedStringsThrow)
{
    std::string_view const inputs[] = {
        R"("abc)",            // unterminated
        R"("abc\)",           // unterminated escape
        R"("\x")",            // invalid escape
        R"("\u12")",          // too few hex digits
        R"("\uZZZZ")",        // not hex
        R"("\uDE00")",        // lone low surrogate
        R"("\uD83D")",        // high surrogate without low
        "\"a\nb\"",           // raw control character
    };
    for (auto input : inputs)
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}
