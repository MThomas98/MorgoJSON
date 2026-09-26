#include <gtest/gtest.h>

#include "Lexer.hpp"

#include <cmath>
#include <limits>

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

TEST(Lexer, ErrorMessagesDescribeCharactersReadably)
{
    // Printable characters are quoted, control characters are shown as code
    // points, and non-ASCII bytes in hex, so the message never contains raw bytes.
    struct Case { std::string_view input; std::string_view expected; };
    Case const cases[] = {
        {"@",            "unexpected character '@'"},
        {"\x01",         "unexpected character U+0001"},
        {"\x7F",         "unexpected character U+007F"},
        {"\xC3\xA9",     "unexpected character byte 0xC3"},
        {"\"a\tb\"",     "control character U+0009 in string"},
        {R"("\q")",      "invalid escape character 'q' after backslash"},
        {"\"\\\x01\"",   "invalid escape character U+0001 after backslash"},
        {"-x",           "unexpected character 'x' in number"},
        {"1.\xC3",       "unexpected character byte 0xC3 in number"},
    };

    for (auto const& [input, expected] : cases)
    {
        SCOPED_TRACE(testing::Message() << "expected: " << expected);
        Lexer lexer{input};
        try
        {
            lexer.next();
            ADD_FAILURE() << "expected a LexerError";
        }
        catch (LexerError const& e)
        {
            EXPECT_NE(std::string_view{e.what()}.find(expected), std::string_view::npos)
                << "message was: " << e.what();
        }
    }
}

TEST(Lexer, ErrorsAreLexerErrors)
{
    // One input from each part of the lexer
    for (std::string_view input : {"@", "tru", "1e", "\"abc", "\"\\x\"", "\"\\uZZZZ\"", "1e400"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), LexerError);
    }
}

TEST(Lexer, ThrowsAfterValidTokens)
{
    Lexer lexer{"[ ; ]"};
    expectNext<Token::LBracket>(lexer);
    EXPECT_THROW(lexer.next(), std::runtime_error);
}

TEST(Lexer, NullCharacterThrows)
{
    using namespace std::string_view_literals;

    for (auto input : {"\0"sv, " \0"sv, "\0[]"sv})
    {
        SCOPED_TRACE(testing::Message() << "input size " << input.size());
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }

    Lexer lexer{"[1]\0garbage"sv};
    expectNext<Token::LBracket>(lexer);
    expectNext<Token::Number>(lexer);
    expectNext<Token::RBracket>(lexer);
    EXPECT_THROW(lexer.next(), std::runtime_error);
}

// ---- Numbers ----

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

TEST(Lexer, ExponentAtLongLongLimitsWithMantissaExponent)
{
    // The exponents fit in a long long exactly (LLONG_MAX and LLONG_MIN), but
    // adding the mantissa's own exponent must not overflow and flip the result.

    // 10 = 1.0e1, so the total exponent is LLONG_MAX + 1: hugely large, must throw.
    for (std::string_view input : {"10e9223372036854775807", "10e+9223372036854775807",
                                   "-10e9223372036854775807"})
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error);
    }

    // 0.01 = 1.0e-2, so the total exponent is LLONG_MIN - 2: hugely small, rounds to zero.
    double const positive = nextAs<Token::Number>("0.01e-9223372036854775808").value;
    EXPECT_EQ(positive, 0.0);
    EXPECT_FALSE(std::signbit(positive));

    double const negative = nextAs<Token::Number>("-0.01e-9223372036854775808").value;
    EXPECT_EQ(negative, 0.0);
    EXPECT_TRUE(std::signbit(negative));
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

TEST(Lexer, StringKeepsInnerSpaces)
{
    EXPECT_EQ(nextAs<Token::String>(R"("  a b  ")").value, "  a b  ");
}

TEST(Lexer, StringContainingPunctuationAndKeywords)
{
    EXPECT_EQ(nextAs<Token::String>(R"("{[:,]} true null 1.5")").value, "{[:,]} true null 1.5");
}

TEST(Lexer, SimpleEscapes)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\"\\\/\b\f\n\r\t")").value, "\"\\/\b\f\n\r\t");
}

TEST(Lexer, EscapesMixedWithText)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\nabc")").value, "\nabc");
    EXPECT_EQ(nextAs<Token::String>(R"("abc\n")").value, "abc\n");
    EXPECT_EQ(nextAs<Token::String>(R"("a\"b\"c")").value, "a\"b\"c");
    EXPECT_EQ(nextAs<Token::String>(R"("C:\\dir\\file")").value, "C:\\dir\\file");
}

TEST(Lexer, EscapedBackslashBeforeClosingQuote)
{
    // The \\ is a complete escape, so the next " ends the string.
    expectTokens<Token::String, Token::Comma>(R"("\\",)");
    EXPECT_EQ(nextAs<Token::String>(R"("\\")").value, "\\");
}

TEST(Lexer, RawUtf8PassesThrough)
{
    EXPECT_EQ(nextAs<Token::String>("\"\xC3\xA9\"").value, "\xC3\xA9");                  // é
    EXPECT_EQ(nextAs<Token::String>("\"\xF0\x9F\x98\x80\"").value, "\xF0\x9F\x98\x80");  // 😀
}

TEST(Lexer, DeleteCharacterIsAllowedRaw)
{
    // Only U+0000 to U+001F must be escaped; DEL (U+007F) is fine.
    EXPECT_EQ(nextAs<Token::String>("\"\x7F\"").value, "\x7F");
}

TEST(Lexer, UnicodeEscape)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\u0041")").value, "A");
    EXPECT_EQ(nextAs<Token::String>(R"("\u00e9")").value, "\xC3\xA9");      // é (2 bytes)
    EXPECT_EQ(nextAs<Token::String>(R"("\u20AC")").value, "\xE2\x82\xAC");  // € (3 bytes)
    EXPECT_EQ(nextAs<Token::String>(R"("\uFFFF")").value, "\xEF\xBF\xBF");  // largest BMP code point
}

TEST(Lexer, UnicodeEscapeUtf8Boundaries)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\u007F")").value, "\x7F");          // last 1-byte
    EXPECT_EQ(nextAs<Token::String>(R"("\u0080")").value, "\xC2\x80");      // first 2-byte
    EXPECT_EQ(nextAs<Token::String>(R"("\u07FF")").value, "\xDF\xBF");      // last 2-byte
    EXPECT_EQ(nextAs<Token::String>(R"("\u0800")").value, "\xE0\xA0\x80");  // first 3-byte
}

TEST(Lexer, UnicodeEscapeHexIsCaseInsensitive)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\u00E9")").value,
              nextAs<Token::String>(R"("\u00e9")").value);
    EXPECT_EQ(nextAs<Token::String>(R"("\uaBcD")").value,
              nextAs<Token::String>(R"("\uABCD")").value);
}

TEST(Lexer, UnicodeEscapeOfNullCharacter)
{
    // \u0000 is valid and must produce a real NUL byte inside the string.
    EXPECT_EQ(nextAs<Token::String>(R"("a\u0000b")").value, std::string("a\0b", 3));
}

TEST(Lexer, UnicodeEscapeFollowedByDigits)
{
    // Only four hex digits belong to the escape.
    EXPECT_EQ(nextAs<Token::String>(R"("\u00411")").value, "A1");
}

TEST(Lexer, SurrogatePair)
{
    EXPECT_EQ(nextAs<Token::String>(R"("\uD83D\uDE00")").value, "\xF0\x9F\x98\x80");  // 😀
    EXPECT_EQ(nextAs<Token::String>(R"("\uD800\uDC00")").value, "\xF0\x90\x80\x80");  // U+10000
    EXPECT_EQ(nextAs<Token::String>(R"("\uDBFF\uDFFF")").value, "\xF4\x8F\xBF\xBF");  // U+10FFFF
}

TEST(Lexer, KeyValuePair)
{
    expectTokens<Token::LBrace, Token::String, Token::Colon, Token::Bool,
                 Token::RBrace>(R"({"ok": true})");
}

TEST(Lexer, AdjacentStringsAreSeparateTokens)
{
    // Rejecting this is the parser's job, not the lexer's.
    expectTokens<Token::String, Token::String>(R"("a""b")");
}

TEST(Lexer, StringsInArray)
{
    Lexer lexer{R"(["a", "b\n"])"};
    expectNext<Token::LBracket>(lexer);
    EXPECT_EQ(std::get<Token::String>(lexer.next().value).value, "a");
    expectNext<Token::Comma>(lexer);
    EXPECT_EQ(std::get<Token::String>(lexer.next().value).value, "b\n");
    expectNext<Token::RBracket>(lexer);
    expectNext<Token::EndOfFile>(lexer);
}

TEST(Lexer, LongString)
{
    std::string const long_text(10'000, 'x');
    EXPECT_EQ(nextAs<Token::String>("\"" + long_text + "\"").value, long_text);
}

TEST(Lexer, MalformedStringsThrow)
{
    using namespace std::string_view_literals;
    std::string_view const inputs[] = {
        R"(")",               // lone quote
        R"("abc)",            // unterminated
        R"("abc\)",           // unterminated escape
        R"("\")",             // escaped quote, then unterminated
        R"("\x")",            // invalid escape
        R"("\N")",            // escapes are case-sensitive
        R"("\U0041")",        // escapes are case-sensitive
        R"("\')",             // not a JSON escape
        R"("\u)",             // EOF straight after \u
        R"("\u12")",          // too few hex digits
        R"("\uZZZZ")",        // not hex
        R"("\u-123")",        // not hex
        R"("\uDE00")",        // lone low surrogate
        R"("\uD83D")",        // high surrogate without low
        R"("\uD83Dx")",       // high surrogate followed by a normal character
        R"("\uD83D\u0041")",  // high surrogate followed by a non-surrogate escape
        R"("\uD83D\uD83D")",  // two high surrogates
        R"("\uDE00\uD83D")",  // pair in the wrong order
        R"("\uD83D\n")",      // high surrogate followed by a simple escape
        "\"a\nb\"",           // raw newline
        "\"a\rb\"",           // raw carriage return
        "\"a\tb\"",           // raw tab
        "\"a\x01z\"",         // raw control character
        "\"a\x1Fz\"",         // highest control character
        "\"a\0b\""sv,         // raw NUL
    };
    for (auto input : inputs)
    {
        SCOPED_TRACE(input);
        Lexer lexer{input};
        EXPECT_THROW(lexer.next(), std::runtime_error) << "input was:" << input;
    }
}

// ---- Positions ----

namespace
{
    void expectPosition(Token const& token, std::size_t row, std::size_t col)
    {
        EXPECT_EQ(token.row, row);
        EXPECT_EQ(token.col, col);
    }

    // Lexes input until it throws, then checks where the error was reported.
    void expectErrorAt(std::string_view input, std::size_t row, std::size_t col)
    {
        SCOPED_TRACE(testing::Message() << "input: " << input);
        Lexer lexer{input};
        try
        {
            while (!lexer.next().isType<Token::EndOfFile>()) {}
            ADD_FAILURE() << "expected a LexerError";
        }
        catch (LexerError const& e)
        {
            EXPECT_EQ(e.row(), row) << e.what();
            EXPECT_EQ(e.col(), col) << e.what();
        }
    }
}

TEST(Lexer, TokenPositions)
{
    Lexer lexer{"[\n  1,\n  \"a\"]"};
    expectPosition(lexer.next(), 1, 1);  // [
    expectPosition(lexer.next(), 2, 3);  // 1
    expectPosition(lexer.next(), 2, 4);  // ,
    expectPosition(lexer.next(), 3, 3);  // "a"
    expectPosition(lexer.next(), 3, 6);  // ]
    expectPosition(lexer.next(), 3, 7);  // end of file
}

TEST(Lexer, TokenPositionsAfterMultiCharacterTokens)
{
    Lexer lexer{R"(true, -1.5e3, "ab\ncd", null)"};
    expectPosition(lexer.next(), 1, 1);   // true
    expectPosition(lexer.next(), 1, 5);   // ,
    expectPosition(lexer.next(), 1, 7);   // -1.5e3
    expectPosition(lexer.next(), 1, 13);  // ,
    expectPosition(lexer.next(), 1, 15);  // "ab\ncd"
    expectPosition(lexer.next(), 1, 23);  // ,
    expectPosition(lexer.next(), 1, 25);  // null
}

TEST(Lexer, CrLfCountsAsOneLine)
{
    Lexer lexer{"[\r\n  1,\r\n\r\n2]"};
    expectPosition(lexer.next(), 1, 1);  // [
    expectPosition(lexer.next(), 2, 3);  // 1
    expectPosition(lexer.next(), 2, 4);  // ,
    expectPosition(lexer.next(), 4, 1);  // 2
}

TEST(Lexer, ErrorPositions)
{
    // Errors point at the offending character...
    expectErrorAt("[\n  1,\n  x]", 3, 3);     // unexpected character
    expectErrorAt("  tru", 1, 3);             // truncated keyword (start of keyword)
    expectErrorAt(R"("ab\xcd")", 1, 5);       // invalid escape character
    expectErrorAt("\"a\tb\"", 1, 3);          // raw control character
    expectErrorAt(R"("\u12G4")", 1, 4);       // bad hex digits (start of the hex)
    expectErrorAt("1.x", 1, 3);               // missing fraction digits

    // ...unless the whole token is the problem, when they point at its start
    expectErrorAt("[1,\n 1e400]", 2, 2);      // number out of range
    expectErrorAt("[\n  \"abc", 2, 3);        // unterminated string
}
