#include "Lexer.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
    constexpr char NULL_C = '\0';

    bool isWhitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    char parseEscapedChar(char c)
    {
        switch (c)
        {
            case '"':  return '"';
            case '\\': return '\\';
            case '/':  return '/'; 
            case 'b':  return '\b';
            case 'f':  return '\f';
            case 'n':  return '\n';
            case 'r':  return '\r';
            case 't':  return '\t';
            // TODO: \uXXXX parsing 
            default: 
                throw std::runtime_error(
                    std::format("invalid escape character \\{}", c));
        }
    }
}

Lexer::Lexer(std::string_view data)
    : m_data(data) {}

Token Lexer::next()
{
    char const c = advancePastWhitespace();
    switch (c)
    {
        case NULL_C: return makeToken(Token::EndOfFile{});

        case '{': return makeToken(Token::LBrace{});
        case '}': return makeToken(Token::RBrace{});
        case '[': return makeToken(Token::LBracket{});
        case ']': return makeToken(Token::RBracket{});
        case ':': return makeToken(Token::Colon{});
        case ',': return makeToken(Token::Comma{});

        case 't': consumeKeyword("true");  return makeToken(Token::Bool{true});
        case 'f': consumeKeyword("false"); return makeToken(Token::Bool{false});
        case 'n': consumeKeyword("null");  return makeToken(Token::Null{});

        case '"': return consumeString();

        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return consumeNumber();

        default:
            throw std::runtime_error(
                std::format("unexpected character '{}'", c));
    }
}

bool Lexer::atEnd() const
{
    return m_pos >= m_data.size();
}

char Lexer::advance()
{
    return atEnd() ? NULL_C : m_data[m_pos++];
}

char Lexer::advancePastWhitespace()
{
    while (!atEnd() && isWhitespace(m_data[m_pos])) ++m_pos;

    return atEnd() ? NULL_C : m_data[m_pos++];
}

Token Lexer::makeToken(Token::TokenValueType token) const
{
    return Token {
        .value = std::move(token),
        .col = m_col,
        .row = m_row
    };
}

void Lexer::consumeKeyword(std::string_view keyword)
{
    if (!m_data.substr(m_pos - 1).starts_with(keyword))
    {
        throw std::runtime_error(
            std::format("invalid literal, expected '{}'", keyword));
    }

    m_pos += keyword.size() - 1;
}

Token Lexer::consumeString()
{
    constexpr std::string_view STRING_STOP_CHARS = "\"\n\r\\";

    std::string value;
    std::size_t stop_index = m_data.find_first_of(STRING_STOP_CHARS, m_pos);
    while (stop_index != std::string_view::npos)
    {
        value.append(m_data.substr(m_pos, stop_index - m_pos));
        m_pos = stop_index;

        if (m_data[m_pos] == '"')
        {
            ++m_pos; // Move past the trailing "
            return makeToken(Token::String{std::move(value)});
        }

        if (m_data[m_pos] == '\n' || m_data[m_pos] == '\r')
        {
            throw std::runtime_error("unexpectedly saw newline in string");
        }

        ++m_pos; // Move past the escape char
        if (atEnd())
        {
            throw std::runtime_error("string not terminated");
        }

        value += parseEscapedChar(advance());

        stop_index = m_data.find_first_of(STRING_STOP_CHARS, m_pos);
    }

    throw std::runtime_error("string not terminated");
}

Token Lexer::consumeNumber()
{
    constexpr std::string_view INT_CHARS = "0123456789";

    auto const consumeDigits = 
        [this, &INT_CHARS]()
        {
            std::size_t stop_index = 
                std::min(m_data.find_first_not_of(INT_CHARS, m_pos), m_data.size());
            
            if (stop_index == m_pos)
            {
                throw std::runtime_error(atEnd() ?
                    std::format("unexpected EOF in number") :
                    std::format("saw unexpected character {} in number", m_data[m_pos]));
            }

            m_pos = stop_index;
        };

    --m_pos; // Unconsume first digit
    std::size_t start = m_pos;

    // -- Minus part --
    if (m_data[m_pos] == '-') ++m_pos;
    
    // -- Integer part --
    if (!atEnd() && m_data[m_pos] == '0')
    {
        ++m_pos;
    }
    else 
    {
        consumeDigits();
    }

    // -- Fractional part --
    if (!atEnd() && m_data[m_pos] == '.')
    {
        ++m_pos;
        consumeDigits();
    }

    // -- Exponential part --
    if (!atEnd() && (m_data[m_pos] == 'e' || m_data[m_pos] == 'E'))
    {
        ++m_pos;
        
        if (atEnd())
        {
            throw std::runtime_error("saw exponent, but no integer afterwards");
        }

        if (m_data[m_pos] == '+' || m_data[m_pos] == '-')
        {
            ++m_pos;
        }
        
        consumeDigits();
    }

    std::string_view const value_str = m_data.substr(start, m_pos - start);
    double value = 0.0;
    [[maybe_unused]] auto const [ptr, error] = 
        std::from_chars(value_str.data(), value_str.data() + value_str.size(), value);

    if (error == std::errc::result_out_of_range)
    {
        // If we've entered here, the number is either very large or incredibly small.
        // If number is very small, need to round it to (+-)0.
        //
        // To achieve this break the number down in to mantissa and exponent before the "E"
        // (if it exists), then add the exponent given after E.
        // If this comes out to less than 0, then the number must be small and 
        // we cna round to 0.

        std::size_t const e_index = std::min(value_str.find_first_of("eE"), value_str.size());
        std::string_view mantissa = value_str.substr(0, e_index);

        std::size_t point_index = std::min(mantissa.find_first_of('.'), mantissa.size());
        std::size_t first_sig_digit_index = mantissa.find_first_of("123456789");
        
        long long exponent = point_index > first_sig_digit_index ?
            static_cast<long long>(point_index - first_sig_digit_index - 1) :
            static_cast<long long>(point_index - first_sig_digit_index);

        if (e_index < value_str.size())
        {
            std::string_view e_str = value_str.substr(e_index + 1);
            if (e_str.starts_with('+')) e_str.remove_prefix(1);
            
            long long e_value = 0;
            [[maybe_unused]] auto const [e_ptr, e_error] = 
                std::from_chars(e_str.data(), e_str.data() + e_str.size(), e_value);

            if (e_error == std::errc::result_out_of_range)
            {
                if (e_str.starts_with('-'))
                {
                    // The number after E is very negative, so just round to 0 now and return 
                    value = value_str.starts_with('-') ? -0.0 : +0.0;
                    return makeToken(Token::Number{value});
                }
                else
                {
                    // The number after E is very large, so just throw now
                    throw std::runtime_error(
                        std::format("number {} is out of range", value_str));
                }
            }
            else 
            {
                exponent += e_value;
            }
        }

        if (exponent < 0)
        {
            value = value_str.starts_with('-') ? -0.0 : +0.0;
        }
        else 
        {
            throw std::runtime_error(
                std::format("number {} is out of range", value_str));
        }
    }

    return makeToken(Token::Number{value});
}
