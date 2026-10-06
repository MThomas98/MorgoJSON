#include "Parser.hpp"
#include "Value.hpp"

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

Value Parser::parse()
{
    Value value = parseNext();

    m_current_token = m_lexer.next();
    expectTokenType<Token::EndOfFile>();

    return value;
}

Value Parser::parseNext()
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

Value Parser::parseLiteral()
{
    if (auto* typed_token = getTypedCurrentToken<Token::Bool>())
        return Value{Value::Bool{typed_token->value}};
    if (auto* typed_token = getTypedCurrentToken<Token::Number>())
        return Value{Value::Number{typed_token->value}};
    if (auto* typed_token = getTypedCurrentToken<Token::String>())
        return Value{Value::String{std::move(typed_token->value)}};
    if (m_current_token.isType<Token::Null>())
        return Value{Value::Null{}};

    error(
        std::format("expected a literal, saw {}", m_current_token.getTypeName()), 
        m_current_token);
}

Value Parser::parseObject()
{
    auto object_map = std::make_unique<Value::Object::ObjectMap>();

    auto const addKeyValuePair = 
        [this, &object_map]()
        {
            m_current_token = m_lexer.next();
            std::string key = std::move(expectTokenType<Token::String>().value);

            m_current_token = m_lexer.next();
            expectTokenType<Token::Colon>();

            Value value = parseNext();
            
            // Last duplicate key wins
            object_map->insert_or_assign(std::move(key), std::move(value));
        };

    // Empty object
    if (m_lexer.peek().isType<Token::RBrace>())
    {
        m_current_token = m_lexer.next();
        return Value{Value::Object{std::move(object_map)}};
    }

    do 
    {
        addKeyValuePair();
        m_current_token = m_lexer.next();
    } 
    while (m_current_token.isType<Token::Comma>());

    expectTokenType<Token::RBrace>();

    return Value{Value::Object{std::move(object_map)}};
}

Value Parser::parseArray()
{
    std::vector<Value> values;

    // Empty array
    if (m_lexer.peek().isType<Token::RBracket>())
    {
        m_current_token = m_lexer.next();
        return Value{Value::Array{std::move(values)}};
    }

    do
    {
        values.push_back(parseNext());
        m_current_token = m_lexer.next();
    }
    while (m_current_token.isType<Token::Comma>());

    expectTokenType<Token::RBracket>();

    return Value{Value::Array{std::move(values)}};
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
