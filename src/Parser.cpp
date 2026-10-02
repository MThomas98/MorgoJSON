#include "Parser.hpp"
#include "JSONValue.hpp"

#include <format>


ParserError::ParserError(std::string_view message, Token const& error_token)
    : std::runtime_error(std::format("({}:{}): {}", error_token.row, error_token.col, message))
    , m_col(error_token.col)
    , m_row(error_token.row) {}

std::size_t ParserError::col() const noexcept
{
    return m_col;
}

std::size_t ParserError::row() const noexcept
{
    return m_row;
}


Parser::Parser(std::string_view data)
    : m_lexer(data) {}

JSONValue Parser::parse()
{
    JSONValue value = parseNext();

    m_current_token = m_lexer.next();
    expectTokenType<Token::EndOfFile>();

    return value;
}

JSONValue Parser::parseNext()
{
    m_current_token = m_lexer.next();

    if (m_current_token.isType<Token::LBrace>()) 
        return parseObject();
    if (m_current_token.isType<Token::LBracket>()) 
        return parseArray();
    if (m_current_token.isLiteralType())
        return parseLiteral();

    error(
        std::format("saw unexpected token {}", m_current_token.getTypeName()), 
        m_current_token);
}

JSONValue Parser::parseLiteral()
{
    if (auto* typed_token = getTypedCurrentToken<Token::Bool>())
        return JSONValue{JSONValue::Bool{typed_token->value}};
    if (auto* typed_token = getTypedCurrentToken<Token::Number>())
        return JSONValue{JSONValue::Number{typed_token->value}};
    if (auto* typed_token = getTypedCurrentToken<Token::String>())
        return JSONValue{JSONValue::String{std::move(typed_token->value)}};
    if (m_current_token.isType<Token::Null>())
        return JSONValue{JSONValue::Null{}};

    error(
        std::format("expected a literal, saw {}", m_current_token.getTypeName()), 
        m_current_token);
}

JSONValue Parser::parseObject()
{
    auto object_map = std::make_unique<JSONValue::Object::ObjectMap>();

    auto const addKeyValuePair = 
        [this, &object_map]()
        {
            m_current_token = m_lexer.next();
            std::string key = std::move(expectTokenType<Token::String>().value);

            m_current_token = m_lexer.next();
            expectTokenType<Token::Colon>();

            JSONValue value = parseNext();
            
            // Last duplicate key wins
            object_map->insert_or_assign(std::move(key), std::move(value));
        };

    // Empty object
    if (m_lexer.peek().isType<Token::RBrace>())
    {
        m_current_token = m_lexer.next();
        return JSONValue{JSONValue::Object{std::move(object_map)}};
    }

    do 
    {
        addKeyValuePair();
        m_current_token = m_lexer.next();
    } 
    while (m_current_token.isType<Token::Comma>());

    expectTokenType<Token::RBrace>();

    return JSONValue{JSONValue::Object{std::move(object_map)}};
}

JSONValue Parser::parseArray()
{
    std::vector<JSONValue> values;

    // Empty array
    if (m_lexer.peek().isType<Token::RBracket>())
    {
        m_current_token = m_lexer.next();
        return JSONValue{JSONValue::Array{std::move(values)}};
    }

    do
    {
        values.push_back(parseNext());
        m_current_token = m_lexer.next();
    }
    while (m_current_token.isType<Token::Comma>());

    expectTokenType<Token::RBracket>();

    return JSONValue{JSONValue::Array{std::move(values)}};
}

template <typename T>
T* Parser::getTypedCurrentToken()
{
    return std::get_if<T>(&m_current_token.value);
}

template <typename T>
T& Parser::expectTokenType()
{
    if (auto* typed_token = getTypedCurrentToken<T>())
        return *typed_token;

    error(
        std::format("saw unexpected token {}, expected {}", 
            m_current_token.getTypeName(), T::name),
        m_current_token);
}

void Parser::error(std::string_view message, Token const& error_token) const
{
    throw ParserError(message, error_token);
}
