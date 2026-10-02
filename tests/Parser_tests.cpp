#include <gtest/gtest.h>

#include "Parser.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    JSONValue parse(std::string_view input)
    {
        Parser parser{input};
        return parser.parse();
    }

    template <typename T>
    T const& as(JSONValue const& json_value)
    {
        EXPECT_TRUE(std::holds_alternative<T>(json_value.value))
            << "got variant index " << json_value.value.index();
        return std::get<T>(json_value.value);
    }

    template <typename T>
    T parseAs(std::string_view input)
    {
        auto json_value = parse(input);
        EXPECT_TRUE(std::holds_alternative<T>(json_value.value))
            << "got variant index " << json_value.value.index();
        return std::get<T>(std::move(json_value.value));
    }

    JSONValue::Object::ObjectMap const& membersOf(JSONValue const& json_value)
    {
        return *as<JSONValue::Object>(json_value).members;
    }

    std::vector<JSONValue> const& valuesOf(JSONValue const& json_value)
    {
        return as<JSONValue::Array>(json_value).values;
    }

    void expectThrows(std::string_view input)
    {
        SCOPED_TRACE(std::string("input: ") + std::string(input));
        EXPECT_THROW(parse(input), std::runtime_error);
    }

    void expectErrorAt(std::string_view input, std::size_t row, std::size_t col)
    {
        SCOPED_TRACE(std::string("input: ") + std::string(input));
        try
        {
            parse(input);
            ADD_FAILURE() << "expected a ParserError";
        }
        catch (ParserError const& parser_error)
        {
            EXPECT_EQ(parser_error.row(), row);
            EXPECT_EQ(parser_error.col(), col);
        }
    }
}

// ---- Literals ----

TEST(Parser, ParsesNull)
{
    parseAs<JSONValue::Null>("null");
}

TEST(Parser, ParsesBools)
{
    EXPECT_TRUE(parseAs<JSONValue::Bool>("true").value);
    EXPECT_FALSE(parseAs<JSONValue::Bool>("false").value);
}

TEST(Parser, ParsesNumber)
{
    EXPECT_DOUBLE_EQ(parseAs<JSONValue::Number>("-12.5e1").value, -125.0);
}

TEST(Parser, ParsesString)
{
    EXPECT_EQ(parseAs<JSONValue::String>(R"("hello\nworld")").value, "hello\nworld");
}

TEST(Parser, LiteralSurroundedByWhitespace)
{
    EXPECT_DOUBLE_EQ(parseAs<JSONValue::Number>(" \t\r\n 7 \n").value, 7.0);
}

// ---- Objects ----

TEST(Parser, EmptyObject)
{
    EXPECT_TRUE(membersOf(parse("{}")).empty());
    EXPECT_TRUE(membersOf(parse("{ \n }")).empty());
}

TEST(Parser, ObjectWithOneMember)
{
    auto const json_value = parse(R"({"a": 1})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("a")).value, 1.0);
}

TEST(Parser, ObjectWithEveryLiteralType)
{
    auto const json_value = parse(R"({"n": null, "t": true, "f": false, "d": 2.5, "s": "text"})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 5u);
    as<JSONValue::Null>(members.at("n"));
    EXPECT_TRUE(as<JSONValue::Bool>(members.at("t")).value);
    EXPECT_FALSE(as<JSONValue::Bool>(members.at("f")).value);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("d")).value, 2.5);
    EXPECT_EQ(as<JSONValue::String>(members.at("s")).value, "text");
}

TEST(Parser, ObjectKeysAreDecoded)
{
    auto const json_value = parse(R"({"A\n": 1})");
    EXPECT_TRUE(membersOf(json_value).contains("A\n"));
}

TEST(Parser, ObjectWithoutWhitespace)
{
    auto const json_value = parse(R"({"a":1,"b":2,"c":3})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 3u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("a")).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("b")).value, 2.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("c")).value, 3.0);
}

TEST(Parser, ObjectAcrossLines)
{
    auto const json_value = parse("{\r\n  \"a\" : 1 ,\r\n  \"b\" : 2\r\n}\r\n");
    EXPECT_EQ(membersOf(json_value).size(), 2u);
}

TEST(Parser, DuplicateKeyLastWins)
{
    auto const json_value = parse(R"({"a": 1, "a": 2})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("a")).value, 2.0);
}

TEST(Parser, NestedObject)
{
    auto const json_value = parse(R"({"outer": {"inner": {"leaf": true}}})");
    auto const& inner = membersOf(membersOf(json_value).at("outer")).at("inner");

    EXPECT_TRUE(as<JSONValue::Bool>(membersOf(inner).at("leaf")).value);
}

TEST(Parser, MemberAfterNestedObject)
{
    auto const json_value = parse(R"({"a": {"b": 1}, "c": 2})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(membersOf(members.at("a")).at("b")).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("c")).value, 2.0);
}

TEST(Parser, EmptyNestedObject)
{
    auto const json_value = parse(R"({"a": {}, "b": 1})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_TRUE(membersOf(members.at("a")).empty());
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("b")).value, 1.0);
}

TEST(Parser, EmptyStringKey)
{
    auto const json_value = parse(R"({"": 1})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("")).value, 1.0);
}

TEST(Parser, ObjectKeysAreCaseSensitive)
{
    auto const json_value = parse(R"({"key": 1, "Key": 2, "KEY": 3})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 3u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("key")).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("Key")).value, 2.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("KEY")).value, 3.0);
}

TEST(Parser, ObjectKeysCanLookLikeOtherTokens)
{
    auto const json_value = parse(R"({"null": 1, "true": 2, "12": 3, "{": 4, ":": 5, ",": 6, " ": 7})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 7u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("null")).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("true")).value, 2.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("12")).value, 3.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("{")).value, 4.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at(":")).value, 5.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at(",")).value, 6.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at(" ")).value, 7.0);
}

TEST(Parser, ObjectKeysWithUnicodeEscapes)
{
    auto const json_value = parse(R"({"é": 1, "😀": 2})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_TRUE(members.contains("\xC3\xA9"));
    EXPECT_TRUE(members.contains("\xF0\x9F\x98\x80"));
}

TEST(Parser, ObjectKeysWithRawUtf8)
{
    auto const json_value = parse("{\"\xC3\xA9\": 1}");
    EXPECT_TRUE(membersOf(json_value).contains("\xC3\xA9"));
}

TEST(Parser, EscapedAndUnescapedKeysAreDuplicates)
{
    auto const json_value = parse(R"({"a": 1, "a": 2})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("a")).value, 2.0);
}

TEST(Parser, DuplicateKeyCanChangeType)
{
    auto const json_value = parse(R"({"a": {"b": 1}, "a": "text"})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_EQ(as<JSONValue::String>(members.at("a")).value, "text");
}

TEST(Parser, DuplicateKeysInNestedObjectsAreIndependent)
{
    auto const json_value = parse(R"({"a": 1, "inner": {"a": 2}})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("a")).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(membersOf(members.at("inner")).at("a")).value, 2.0);
}

TEST(Parser, ObjectStringValuesAreDecoded)
{
    auto const json_value = parse(R"({"empty": "", "escaped": "tab\there", "braces": "{\"a\": 1}"})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 3u);
    EXPECT_TRUE(as<JSONValue::String>(members.at("empty")).value.empty());
    EXPECT_EQ(as<JSONValue::String>(members.at("escaped")).value, "tab\there");
    EXPECT_EQ(as<JSONValue::String>(members.at("braces")).value, R"({"a": 1})");
}

TEST(Parser, ObjectWithWhitespaceAroundEveryToken)
{
    auto const json_value = parse(" { \"a\" : { } , \"b\" : [ ] } ");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_TRUE(membersOf(members.at("a")).empty());
    EXPECT_TRUE(valuesOf(members.at("b")).empty());
}

TEST(Parser, SiblingNestedObjects)
{
    auto const json_value = parse(R"({"a": {"x": 1}, "b": {"x": 2}, "c": {}})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 3u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(membersOf(members.at("a")).at("x")).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(membersOf(members.at("b")).at("x")).value, 2.0);
    EXPECT_TRUE(membersOf(members.at("c")).empty());
}

TEST(Parser, DeeplyNestedObject)
{
    constexpr int depth = 50;

    std::string input;
    for (int level = 0; level < depth; ++level)
        input += R"({"k": )";
    input += "null";
    input.append(depth, '}');

    auto const json_value = parse(input);

    JSONValue const* current = &json_value;
    for (int level = 0; level < depth; ++level)
    {
        ASSERT_EQ(membersOf(*current).size(), 1u);
        current = &membersOf(*current).at("k");
    }
    as<JSONValue::Null>(*current);
}

TEST(Parser, ObjectWithManyMembers)
{
    constexpr int count = 200;

    std::string input = "{";
    for (int index = 0; index < count; ++index)
    {
        if (index != 0)
            input += ", ";
        input += "\"key" + std::to_string(index) + "\": " + std::to_string(index);
    }
    input += "}";

    auto const json_value = parse(input);
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), static_cast<std::size_t>(count));
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("key0")).value, 0.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(members.at("key199")).value, 199.0);
}

// ---- Arrays ----

TEST(Parser, EmptyArray)
{
    EXPECT_TRUE(valuesOf(parse("[]")).empty());
    EXPECT_TRUE(valuesOf(parse("[ \n ]")).empty());
}

TEST(Parser, ArrayWithOneValue)
{
    auto const json_value = parse("[1]");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 1u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[0]).value, 1.0);
}

TEST(Parser, ArrayKeepsOrder)
{
    auto const json_value = parse(R"([null, true, 2, "three"])");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 4u);
    as<JSONValue::Null>(values[0]);
    EXPECT_TRUE(as<JSONValue::Bool>(values[1]).value);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[2]).value, 2.0);
    EXPECT_EQ(as<JSONValue::String>(values[3]).value, "three");
}

TEST(Parser, NestedArrays)
{
    auto const json_value = parse("[[], [1, [2]], 3]");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 3u);
    EXPECT_TRUE(valuesOf(values[0]).empty());
    ASSERT_EQ(valuesOf(values[1]).size(), 2u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(valuesOf(valuesOf(values[1])[1])[0]).value, 2.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[2]).value, 3.0);
}

TEST(Parser, ArrayInsideObject)
{
    auto const json_value = parse(R"({"list": [1, 2], "after": true})");
    auto const& members = membersOf(json_value);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_EQ(valuesOf(members.at("list")).size(), 2u);
    EXPECT_TRUE(as<JSONValue::Bool>(members.at("after")).value);
}

TEST(Parser, ObjectsInsideArray)
{
    auto const json_value = parse(R"([{"a": 1}, {}, {"b": 2}])");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 3u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(membersOf(values[0]).at("a")).value, 1.0);
    EXPECT_TRUE(membersOf(values[1]).empty());
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(membersOf(values[2]).at("b")).value, 2.0);
}

TEST(Parser, ArrayWithoutWhitespace)
{
    auto const json_value = parse("[1,2,3]");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 3u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[0]).value, 1.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[1]).value, 2.0);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[2]).value, 3.0);
}

TEST(Parser, ArrayAcrossLines)
{
    auto const json_value = parse("[\r\n  1 ,\r\n  2\r\n]\r\n");
    EXPECT_EQ(valuesOf(json_value).size(), 2u);
}

TEST(Parser, ArrayKeepsDuplicateValues)
{
    auto const json_value = parse("[1, 1, 1]");
    EXPECT_EQ(valuesOf(json_value).size(), 3u);
}

TEST(Parser, ArrayStringsAreDecoded)
{
    auto const json_value = parse(R"(["A\n", ""])");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(as<JSONValue::String>(values[0]).value, "A\n");
    EXPECT_TRUE(as<JSONValue::String>(values[1]).value.empty());
}

TEST(Parser, DeeplyNestedArray)
{
    auto json_value = parse("[[[[[]]]]]");

    JSONValue const* current = &json_value;
    for (int depth = 0; depth < 4; ++depth)
    {
        ASSERT_EQ(valuesOf(*current).size(), 1u);
        current = &valuesOf(*current)[0];
    }
    EXPECT_TRUE(valuesOf(*current).empty());
}

TEST(Parser, ValueAfterNestedArray)
{
    auto const json_value = parse("[[1, 2], 3]");
    auto const& values = valuesOf(json_value);

    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(valuesOf(values[0]).size(), 2u);
    EXPECT_DOUBLE_EQ(as<JSONValue::Number>(values[1]).value, 3.0);
}

TEST(Parser, ArrayOfObjectsWithArrays)
{
    auto const json_value = parse(R"([{"list": [1, {"deep": [true]}]}])");
    auto const& list = valuesOf(membersOf(valuesOf(json_value)[0]).at("list"));

    ASSERT_EQ(list.size(), 2u);
    auto const& deep = valuesOf(membersOf(list[1]).at("deep"));
    ASSERT_EQ(deep.size(), 1u);
    EXPECT_TRUE(as<JSONValue::Bool>(deep[0]).value);
}

TEST(Parser, UnterminatedArrayThrows)
{
    expectThrows("[");
    expectThrows("[1");
    expectThrows("[1,");
    expectThrows("[[1]");
    expectThrows(R"({"a": [1, 2})");
}

TEST(Parser, TrailingCommaInArrayThrows)
{
    expectThrows("[1,]");
    expectThrows("[,]");
}

TEST(Parser, LeadingOrDoubleCommaInArrayThrows)
{
    expectThrows("[,1]");
    expectThrows("[1,,2]");
}

TEST(Parser, MissingCommaInArrayThrows)
{
    expectThrows("[1 2]");
    expectThrows("[[] []]");
}

TEST(Parser, ArrayCannotContainMembers)
{
    expectThrows(R"(["a": 1])");
}

TEST(Parser, MismatchedArrayCloserThrows)
{
    expectThrows("[1}");
    expectThrows("[}");
}

TEST(Parser, LexerErrorsPropagateFromArray)
{
    EXPECT_THROW(parse("[1, tru]"), LexerError);
    EXPECT_THROW(parse(R"(["unterminated])"), LexerError);
}

TEST(Parser, ArrayErrorReportsOffendingToken)
{
    expectErrorAt("[1,]", 1, 4);        // the ']' where a value was expected
    expectErrorAt("[1 2]", 1, 4);       // the number where ',' or ']' was expected
    expectErrorAt("[1}", 1, 3);
    expectErrorAt("[\n  1,\n  ,\n]", 3, 3);
}

TEST(Parser, ArrayErrorAtEndOfInputReportsEndPosition)
{
    expectErrorAt("[1", 1, 3);
}

// ---- Malformed input ----

TEST(Parser, EmptyInputThrows)
{
    expectThrows("");
    expectThrows(" \n ");
}

TEST(Parser, ValueCannotStartWithPunctuation)
{
    expectThrows("}");
    expectThrows("]");
    expectThrows(",");
    expectThrows(":");
}

TEST(Parser, UnterminatedObjectThrows)
{
    expectThrows("{");
    expectThrows(R"({"a")");
    expectThrows(R"({"a":)");
    expectThrows(R"({"a": 1)");
    expectThrows(R"({"a": 1,)");
    expectThrows(R"({"a": {"b": 1})");
}

TEST(Parser, TrailingCommaInObjectThrows)
{
    expectThrows(R"({"a": 1,})");
    expectThrows("{,}");
}

TEST(Parser, MissingCommaInObjectThrows)
{
    expectThrows(R"({"a": 1 "b": 2})");
}

TEST(Parser, MissingColonThrows)
{
    expectThrows(R"({"a" 1})");
    expectThrows(R"({"a", 1})");
}

TEST(Parser, MissingMemberValueThrows)
{
    expectThrows(R"({"a":})");
    expectThrows(R"({"a":, "b": 1})");
}

TEST(Parser, ObjectKeyMustBeString)
{
    expectThrows("{1: 2}");
    expectThrows("{null: 2}");
    expectThrows("{true: 2}");
    expectThrows(R"({{}: 2})");
}

TEST(Parser, MismatchedCloserThrows)
{
    expectThrows(R"({"a": 1])");
}

TEST(Parser, LeadingOrDoubleCommaInObjectThrows)
{
    expectThrows(R"({, "a": 1})");
    expectThrows(R"({"a": 1,, "b": 2})");
}

TEST(Parser, DoubleColonThrows)
{
    expectThrows(R"({"a":: 1})");
}

TEST(Parser, KeyWithoutValueThrows)
{
    expectThrows(R"({"a"})");
    expectThrows(R"({"a": 1, "b"})");
}

TEST(Parser, ValueWithoutKeyThrows)
{
    expectThrows("{1}");
    expectThrows(R"({"a": 1, 2})");
    expectThrows("{[]}");
}

TEST(Parser, ArrayAsObjectKeyThrows)
{
    expectThrows(R"({["a"]: 1})");
}

TEST(Parser, UnquotedOrSingleQuotedKeyThrows)
{
    expectThrows("{a: 1}");
    expectThrows("{'a': 1}");
}

TEST(Parser, MalformedNestedObjectThrows)
{
    expectThrows(R"({"a": {"b" 1}})");
    expectThrows(R"({"a": {"b": 1,}})");
    expectThrows(R"({"a": {"b": 1]})");
    expectThrows(R"({"a": {)");
}

TEST(Parser, LexerErrorsPropagateFromObjectKey)
{
    EXPECT_THROW(parse(R"({"a\x": 1})"), LexerError);
    EXPECT_THROW(parse(R"({"\uD83D": 1})"), LexerError);
    EXPECT_THROW(parse(R"({"unterminated: 1})"), LexerError);
}

TEST(Parser, LexerErrorsPropagate)
{
    EXPECT_THROW(parse(R"({"a": tru})"), LexerError);
    EXPECT_THROW(parse(R"({"a": "unterminated})"), LexerError);
}

// ---- Trailing content ----

TEST(Parser, TrailingWhitespaceIsAllowed)
{
    EXPECT_TRUE(membersOf(parse("{} \t\r\n")).empty());
    EXPECT_TRUE(valuesOf(parse("[]\n\n")).empty());
    parseAs<JSONValue::Null>("null ");
}

TEST(Parser, SecondTopLevelValueThrows)
{
    expectThrows("{} {}");
    expectThrows("[] []");
    expectThrows("1 2");
    expectThrows("null null");
    expectThrows(R"("a" "b")");
    expectThrows("{}\n[]");
}

TEST(Parser, TrailingPunctuationThrows)
{
    expectThrows("{}}");
    expectThrows("[]]");
    expectThrows("{},");
    expectThrows("[1],");
    expectThrows("1,");
    expectThrows(R"("a": 1)");
}

TEST(Parser, LexerErrorsPropagateFromTrailingContent)
{
    EXPECT_THROW(parse("{} tru"), LexerError);
    EXPECT_THROW(parse("[] x"), LexerError);
}

TEST(Parser, TrailingContentErrorReportsOffendingToken)
{
    expectErrorAt("{} {}", 1, 4);
    expectErrorAt("1 2", 1, 3);
    expectErrorAt("null,", 1, 5);
    expectErrorAt("[1]\n]", 2, 1);
}

// ---- Error positions ----

TEST(Parser, ErrorReportsOffendingToken)
{
    expectErrorAt(R"({"a" 1})", 1, 6);       // the number where ':' was expected
    expectErrorAt(R"({"a": 1,})", 1, 9);     // the '}' where a key was expected
    expectErrorAt("  ,", 1, 3);
}

TEST(Parser, ErrorReportsRowOnLaterLines)
{
    expectErrorAt("{\n  \"a\": 1,\n  2: 3\n}", 3, 3);
}

TEST(Parser, ErrorAtEndOfInputReportsEndPosition)
{
    expectErrorAt(R"({"a": 1)", 1, 8);
}

TEST(Parser, ObjectErrorReportsOffendingToken)
{
    expectErrorAt(R"({"a":})", 1, 6);               // the '}' where a value was expected
    expectErrorAt(R"({"a"})", 1, 5);                // the '}' where ':' was expected
    expectErrorAt(R"({"a": 1,, "b": 2})", 1, 9);    // the second ','
    expectErrorAt(R"({"a": 1 "b": 2})", 1, 9);      // the string where ',' or '}' was expected
}

TEST(Parser, NestedObjectErrorReportsOffendingToken)
{
    expectErrorAt(R"({"a": {"b" 1}})", 1, 12);
    expectErrorAt("{\n  \"a\": {\n    \"b\": ]\n  }\n}", 3, 10);
}

TEST(Parser, ErrorMessageStartsWithPosition)
{
    try
    {
        parse(R"({"a" 1})");
        FAIL() << "expected a ParserError";
    }
    catch (ParserError const& parser_error)
    {
        EXPECT_TRUE(std::string_view(parser_error.what()).starts_with("(1:6): "))
            << "got: " << parser_error.what();
    }
}
