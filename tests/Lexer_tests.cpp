#include <gtest/gtest.h>

#include "Lexer.hpp"

#include <cmath>
#include <limits>
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

TEST(Lexer, Integers)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("0").value, 0.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("42").value, 42.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-7").value, -7.0);
}

TEST(Lexer, NegativeZeroKeepsItsSign)
{
    for (std::string_view input : {"-0", "-0.0"})
    {
        SCOPED_TRACE(input);
        double const value = nextAs<Token::Number>(input).value;
        EXPECT_EQ(value, 0.0);
        EXPECT_TRUE(std::signbit(value));
    }
}

TEST(Lexer, ZeroWithExponent)
{
    // Zero stays zero however large the exponent, so it's never out of range.
    for (std::string_view input : {"0e0", "0E-0", "0e400"})
    {
        SCOPED_TRACE(input);
        EXPECT_EQ(nextAs<Token::Number>(input).value, 0.0);
    }
}

TEST(Lexer, LargeNumbers)
{
    EXPECT_EQ(nextAs<Token::Number>("1.7976931348623157e308").value,
              std::numeric_limits<double>::max());
    // 2^53 + 1 isn't representable, so it rounds to 2^53.
    EXPECT_EQ(nextAs<Token::Number>("9007199254740993").value, 9007199254740992.0);
}

TEST(Lexer, TooLargeNumbersThrow)
{
    std::string const huge_integer = "1" + std::string(400, '0');
    std::string_view const inputs[] = {"1.8e308", "1e400", "-1e400", "1e+400", huge_integer};
    for (std::string_view input : inputs)
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}

TEST(Lexer, Fractions)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("3.5").value, 3.5);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-0.25").value, -0.25);
}

TEST(Lexer, Exponents)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("1e3").value, 1000.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("1E+3").value, 1000.0);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-2.5e-2").value, -0.025);
}

TEST(Lexer, SmallNumbers)
{
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("1e-300").value, 1e-300);
    EXPECT_DOUBLE_EQ(nextAs<Token::Number>("-2.5e-10").value, -2.5e-10);
    // Smallest positive subnormal double
    EXPECT_EQ(nextAs<Token::Number>("5e-324").value, std::numeric_limits<double>::denorm_min());
}

TEST(Lexer, TooSmallNumbersUnderflowToZero)
{
    // Valid JSON, but smaller than any double: rounds to zero, keeping its sign.
    double const positive = nextAs<Token::Number>("1e-400").value;
    EXPECT_EQ(positive, 0.0);
    EXPECT_FALSE(std::signbit(positive));

    double const negative = nextAs<Token::Number>("-1e-400").value;
    EXPECT_EQ(negative, 0.0);
    EXPECT_TRUE(std::signbit(negative));
}

TEST(Lexer, LongFractionUnderflowsWithoutExponent)
{
    std::string const tiny_fraction = "0." + std::string(400, '0') + "1";
    EXPECT_EQ(nextAs<Token::Number>(tiny_fraction).value, 0.0);
}

TEST(Lexer, LongIntegerOverflowsDespiteNegativeExponent)
{
    std::string const huge_with_negative_exponent = "1" + std::string(500, '0') + "e-100";
    Lexer lexer{huge_with_negative_exponent};
    EXPECT_THROW(lexer.next(), std::runtime_error);
}

TEST(Lexer, ExponentTooBigForLongLong)
{
    // The exponent alone decides these, even with a mantissa below 1.
    for (std::string_view input : {"0.0001e99999999999999999999", "0.0001e+99999999999999999999",
                                   "1e99999999999999999999"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }

    for (std::string_view input : {"1e-99999999999999999999", "1000e-99999999999999999999"})
    {
        SCOPED_TRACE(input);
        EXPECT_EQ(nextAs<Token::Number>(input).value, 0.0);
    }
}

TEST(Lexer, NegativeNumberWithExponentTooBigForLongLong)
{
    // A negative number with a huge positive exponent is hugely negative: overflow.
    for (std::string_view input : {"-0.0001e99999999999999999999", "-1e+99999999999999999999"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }

    // With a huge negative exponent it underflows to -0.0, keeping its sign.
    double const value = nextAs<Token::Number>("-1e-99999999999999999999").value;
    EXPECT_EQ(value, 0.0);
    EXPECT_TRUE(std::signbit(value));
}

TEST(Lexer, NumbersInArray)
{
    expectTokens<Token::LBracket, Token::Number, Token::Comma, Token::Number,
                 Token::RBracket>("[1, -2.5]");
}

TEST(Lexer, LeadingZeroIsNotPartOfTheNumber)
{
    // "01" lexes as 0 then 1; the parser rejects two numbers in a row.
    expectTokens<Token::Number, Token::Number>("01");
    expectTokens<Token::Number, Token::Number>("-01");
    expectTokens<Token::Number, Token::Number>("00");
}

TEST(Lexer, NumberEndsAtNonNumberCharacter)
{
    expectTokens<Token::Number, Token::Number>("1 2");
    expectTokens<Token::LBracket, Token::Number, Token::RBracket>("[-0]");
}

TEST(Lexer, ThrowsOnCharacterAfterNumber)
{
    // The number itself lexes; the stray character after it is the error.
    for (std::string_view input : {"1.5.5", "1e5e5", "1x", "0x1F"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        expectNext<Token::Number>(lexer);
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }
}

TEST(Lexer, MalformedNumbersThrow)
{
    for (std::string_view input : {"-", "1.", "1.e5", "1e", "1e+", "-a",
                                   "+1", ".5", "--1", "-e5", "1e-", "-Infinity"})
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
