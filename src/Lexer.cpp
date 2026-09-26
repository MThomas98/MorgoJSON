#include "Lexer.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <utility>

namespace
{
    bool isWhitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // JSON requires U+0000 to U+001F to be escaped inside strings.
    bool isControlChar(char c)
    {
        return static_cast<unsigned char>(c) < 0x20;
    }

    std::string describeChar(char c)
    {
        auto const byte = static_cast<unsigned char>(c);
        if (byte >= 0x80) return std::format("byte 0x{:02X}", byte);
        if (byte < 0x20 || byte == 0x7F) return std::format("U+{:04X}", byte);

        return std::format("'{}'", c);
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

    std::optional<char> parseEscapedChar(char c)
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
            default:   return std::nullopt;
        }
    }

    bool underflowed(std::string_view number)
    {
        std::size_t const e_index = std::min(number.find_first_of("eE"), number.size());
        std::string_view const mantissa = number.substr(0, e_index);

        // Power of ten of the mantissa's first significant digit,
        // e.g. 1 for "12.5" (1.25e1) and -2 for "0.01" (1e-2).
        std::size_t const point_index = std::min(mantissa.find('.'), mantissa.size());
        std::size_t const first_sig_digit_index = mantissa.find_first_of("123456789");
        long long const exponent = point_index > first_sig_digit_index ?
            static_cast<long long>(point_index - first_sig_digit_index - 1) :
            static_cast<long long>(point_index - first_sig_digit_index);

        if (e_index == number.size()) return exponent < 0;

        std::string_view e_str = number.substr(e_index + 1);
        if (e_str.starts_with('+')) e_str.remove_prefix(1);

        long long e_value = 0;
        auto const [e_ptr, e_error] = std::from_chars(e_str.data(), e_str.data() + e_str.size(), e_value);

        // The exponent alone is too big for a long long, or adding the two
        // would overflow: either way the exponent's sign decides.
        if (e_error == std::errc::result_out_of_range) return e_str.starts_with('-');
        if (e_value > 0 && exponent > std::numeric_limits<long long>::max() - e_value) return false;
        if (e_value < 0 && exponent < std::numeric_limits<long long>::min() - e_value) return true;

        return exponent + e_value < 0;
    }

    bool isHighSurrogate(std::uint32_t unit)
    { 
        return unit >= 0xD800 && unit <= 0xDBFF; 
    }
    bool isLowSurrogate(std::uint32_t unit)
    {
        return unit >= 0xDC00 && unit <= 0xDFFF; }

    std::optional<std::uint16_t> parseHex4(std::string_view hex)
    {
        if (hex.size() != 4) return std::nullopt;

        std::uint16_t value = 0;
        auto const [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), value, 16);
        if (ec != std::errc{} || ptr != hex.data() + hex.size()) return std::nullopt;

        return value;
    }

    void appendUtf8(std::string& str, std::uint32_t code_point)
    {
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
}


LexerError::LexerError(std::string const& message, std::size_t col, std::size_t row)
    : std::runtime_error(std::format("({}:{}): {}", row, col, message))
    , m_col(col)
    , m_row(row) {}

std::size_t LexerError::col() const noexcept
{
    return m_col;
}

std::size_t LexerError::row() const noexcept
{
    return m_row;
}


Lexer::Lexer(std::string_view data)
    : m_data(data) {}

Token Lexer::next()
{
    skipWhitespace();
    m_token_start = m_pos;
    if (atEnd()) return makeToken(Token::EndOfFile{});

    char const c = m_data[m_pos];
    switch (c)
    {
        case '\0': error("saw invalid null character");

        case '{': return consumeChar(Token::LBrace{});
        case '}': return consumeChar(Token::RBrace{});
        case '[': return consumeChar(Token::LBracket{});
        case ']': return consumeChar(Token::RBracket{});
        case ':': return consumeChar(Token::Colon{});
        case ',': return consumeChar(Token::Comma{});

        case 't': consumeKeyword("true");  return makeToken(Token::Bool{true});
        case 'f': consumeKeyword("false"); return makeToken(Token::Bool{false});
        case 'n': consumeKeyword("null");  return makeToken(Token::Null{});

        case '"': return consumeString();

        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return consumeNumber();

        default:
            error(std::format("unexpected character {}", describeChar(c)));
    }
}

std::size_t Lexer::colAt(std::size_t pos) const
{
    return pos - m_line_start + 1;
}

void Lexer::error(std::string const& message) const
{
    errorAt(m_pos, message);
}

void Lexer::errorAt(std::size_t pos, std::string const& message) const
{
    throw LexerError(message, colAt(pos), m_row);
}

bool Lexer::atEnd() const
{
    return m_pos >= m_data.size();
}

void Lexer::skipWhitespace()
{
    while (!atEnd() && isWhitespace(m_data[m_pos]))
    {
        // Counting only \n handles both \n and \r\n line endings
        if (m_data[m_pos] == '\n')
        {
            ++m_row;
            m_line_start = m_pos + 1;
        }

        ++m_pos;
    }
}

Token Lexer::makeToken(Token::TokenValueType token) const
{
    return Token {
        .value = std::move(token),
        .col = colAt(m_token_start),
        .row = m_row
    };
}

Token Lexer::consumeChar(Token::TokenValueType token)
{
    ++m_pos;
    return makeToken(std::move(token));
}

void Lexer::consumeKeyword(std::string_view keyword)
{
    if (!m_data.substr(m_pos).starts_with(keyword))
    {
        error(std::format("invalid literal, expected '{}'", keyword));
    }

    m_pos += keyword.size();
}

Token Lexer::consumeString()
{
    ++m_pos; // Move past the opening "

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
            error(std::format(
                "unexpectedly saw control character {} in string",
                describeChar(m_data[m_pos])));
        }

        ++m_pos; // Move past the backslash
        if (atEnd())
        {
            errorAt(m_token_start, "string not terminated");
        }

        if (m_data[m_pos] == 'u')
        {
            appendUnicode(value);
        }
        else 
        {
            char const escape_char = m_data[m_pos];
            std::optional<char> const escaped = parseEscapedChar(escape_char);
            if (!escaped)
            {
                error(std::format("invalid escape character {} after backslash", describeChar(escape_char)));
            }

            value += *escaped;
            ++m_pos; // Move past the escape character
        }

        stop_index = findStringStop(m_data, m_pos);
    }

    errorAt(m_token_start, "string not terminated");
}

Token Lexer::consumeNumber()
{
    static constexpr std::string_view INT_CHARS = "0123456789";

    auto const consumeDigits =
        [this]()
        {
            std::size_t stop_index = 
                std::min(m_data.find_first_not_of(INT_CHARS, m_pos), m_data.size());
            
            if (stop_index == m_pos)
            {
                error(atEnd() ?
                    std::format("saw unexpected EOF in number") :
                    std::format("saw unexpected character {} in number", describeChar(m_data[m_pos])));
            }

            m_pos = stop_index;
        };

    std::size_t const start = m_pos;

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
            error("saw exponent, but no integer afterwards");
        }

        if (m_data[m_pos] == '+' || m_data[m_pos] == '-')
        {
            ++m_pos;
        }
        
        consumeDigits();
    }

    std::string_view const value_str = m_data.substr(start, m_pos - start);
    double value = 0.0;
    [[maybe_unused]] auto const [ptr, ec] = 
        std::from_chars(value_str.data(), value_str.data() + value_str.size(), value);

    if (ec == std::errc::result_out_of_range)
    {
        // Too small for a double rounds to 0, keeping its sign; too large is an error.
        if (!underflowed(value_str))
        {
            errorAt(m_token_start, std::format("number {} is out of range", value_str));
        }

        value = value_str.starts_with('-') ? -0.0 : +0.0;
    }

    return makeToken(Token::Number{value});
}

void 
Lexer::appendUnicode(std::string& str)
{
    auto const readHex4 =
        [this]()
        {
            std::string_view const hex_str = m_data.substr(m_pos, 4);
            if (hex_str.size() != 4)
            {
                error("not enough characters in unicode hex sequence");
            }

            std::optional<std::uint16_t> const unit = parseHex4(hex_str);
            if (!unit)
            {
                error("invalid hex digit in unicode hex sequence");
            }

            m_pos += 4;
            return *unit;
        };

    ++m_pos; // Move past u

    std::size_t const unit_start = m_pos;
    std::uint16_t const unit = readHex4();

    if (isLowSurrogate(unit))
    {
        errorAt(unit_start, "saw low surrogate unicode value before a high surrogate");
    }

    if (!isHighSurrogate(unit))
    {
        appendUtf8(str, unit);
        return;
    }

    // High surrogate - must be followed by a low surrogate
    if (m_data.substr(m_pos, 2) != "\\u")
    {
        error("expected low surrogate unicode hex sequence");
    }

    m_pos += 2; // Move past \u

    std::size_t const low_start = m_pos;
    std::uint16_t const low_surrogate = readHex4();
    if (!isLowSurrogate(low_surrogate))
    {
        errorAt(low_start, "unicode sequence after high surrogate not a low surrogate");
    }

    appendUtf8(str, 0x10000 + ((unit - 0xD800u) << 10) + (low_surrogate - 0xDC00u));
}
