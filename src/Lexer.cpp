#include "Lexer.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
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

    // JSON requires U+0000 to U+001F to be escaped inside strings.
    bool isControlChar(char c)
    {
        return static_cast<unsigned char>(c) < 0x20;
    }

    std::size_t findStringStop(std::string_view data, std::size_t pos)
    {
        auto const it = std::find_if(data.begin() + pos, data.end(),
            [](char c) 
            { 
                return c == '"' || c == '\\' || isControlChar(c); 
            });

        return it == data.end() ? 
            std::string_view::npos : 
            static_cast<std::size_t>(it - data.begin());
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
    skipWhitespace();
    if (atEnd()) return makeToken(Token::EndOfFile{});

    char const c = m_data[m_pos++];
    switch (c)
    {
        case NULL_C: throw std::runtime_error("saw invalid null character");

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

void Lexer::skipWhitespace()
{
    while (!atEnd() && isWhitespace(m_data[m_pos])) ++m_pos;
}

char Lexer::lookAhead(std::size_t n) const
{
    if (m_pos + n >= m_data.size()) return NULL_C;

    return m_data[m_pos + n];
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
    std::string value;
    std::size_t stop_index = findStringStop(m_data, m_pos);
    while (stop_index != std::string_view::npos)
    {
        value.append(m_data.substr(m_pos, stop_index - m_pos));
        m_pos = stop_index;

        if (m_data[m_pos] == '"')
        {
            ++m_pos; // Move past the trailing "
            return makeToken(Token::String{std::move(value)});
        }

        if (isControlChar(m_data[m_pos]))
        {
            throw std::runtime_error(std::format(
                "unexpectedly saw control character U+{:04X} in string",
                static_cast<unsigned char>(m_data[m_pos])));
        }

        ++m_pos; // Move past the escape char
        if (atEnd())
        {
            throw std::runtime_error("string not terminated");
        }

        if (m_data[m_pos] == 'u')
        {
            appendUnicode(value);
        }
        else 
        {
            value += parseEscapedChar(advance());
        }

        stop_index = findStringStop(m_data, m_pos);
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
                    std::format("saw unexpected EOF in number") :
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
        // If we've entered here, the number is either very large or very small.
        // If number is very small, need to round it to (+-)0.
        //
        // To achieve this break the number down in to mantissa and exponent before the "E"
        // (if it exists), then add the exponent value given after E.
        // If this comes out to less than 0, then the number must be small and 
        // we can round to 0.

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
                // Prevent an overflow when adding exponent and e-value
                if (e_value > 0 && exponent > std::numeric_limits<long long>::max() - e_value)
                {
                    throw std::runtime_error(std::format("number {} is out of range", value_str));
                }
                if (e_value < 0 && exponent < std::numeric_limits<long long>::min() - e_value)
                {
                    value = value_str.starts_with('-') ? -0.0 : +0.0;
                    return makeToken(Token::Number{value});
                }

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

void 
Lexer::appendUnicode(std::string& str)
{
    constexpr std::string_view HEX_CHARS = "0123456789ABCDEF";
    
    ++m_pos; // Move past u

    std::string_view hex_str = m_data.substr(m_pos, 4);
    if (hex_str.size() != 4)
    {
        throw std::runtime_error("not enough characters in unicode hex sequence");
    }

    std::uint32_t code_point = 0;
    auto const [ptr, error] = std::from_chars(hex_str.data(), hex_str.data() + 4, code_point, 16);
    if (error != std::errc{} || ptr != hex_str.data() + 4)
    {
        throw std::runtime_error("invalid hex digit in unicode hex sequence");
    }

    m_pos += 4; // Move past the hex digits

    // High surrogate - expect a low surrogate
    if (code_point >= 0xD800 && code_point <= 0xDBFF)
    {
        if (m_data.substr(m_pos, 2) != "\\u")
        {
            throw std::runtime_error("expected low surrogate unicode hex sequence");
        }

        hex_str = m_data.substr(m_pos + 2, 4);
        if (hex_str.size() != 4)
        {
            throw std::runtime_error("not enough characters in unicode hex sequence");
        }

        std::uint16_t high_surrogate = static_cast<std::uint16_t>(code_point);
        std::uint16_t low_surrogate = 0;
        auto const [low_ptr, low_error] = std::from_chars(hex_str.data(), hex_str.data() + 4, low_surrogate, 16);
        if (low_error != std::errc{} || low_ptr != hex_str.data() + 4)
        {
            throw std::runtime_error("invalid hex digit in unicode hex sequence");
        }

        if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF)
        {
            throw std::runtime_error("unicode sequence after high surrogate not a low surrogate");
        }

        m_pos += 6; // Move past \u and the hex digits

        code_point = 0x10000 + ((high_surrogate - 0xD800) << 10) + (low_surrogate - 0xDC00);
    }
    // Low surrogate before a high surrogate - invalid
    else if (code_point >= 0xDC00 && code_point <= 0xDFFF)
    {
        throw std::runtime_error("saw low surrogate unicode value before a high surrogate");
    }

    // Convert code_point to UTF-8 (as that's what our string token wants)
    // and append.
    if (code_point < 0x80)
    {
        str += static_cast<char>(code_point);
    }
    else if (code_point < 0x800)
    {
        str += static_cast<char>(0xC0 | (code_point >> 6));
        str += static_cast<char>(0x80 | (0x3F & code_point));
    }
    else if (code_point < 0x10000)
    {
        str += static_cast<char>(0xE0 | (code_point >> 12));
        str += static_cast<char>(0x80 | (0x3F & (code_point >> 6)));
        str += static_cast<char>(0x80 | (0x3F & code_point));
    }
    else
    {
        str += static_cast<char>(0xF0 | (code_point >> 18));
        str += static_cast<char>(0x80 | (0x3F & (code_point >> 12)));
        str += static_cast<char>(0x80 | (0x3F & (code_point >> 6)));
        str += static_cast<char>(0x80 | (0x3F & code_point));
    }
}
