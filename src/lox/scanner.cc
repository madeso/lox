#include "lox/scanner.h"

#include <unordered_map>
#include <charconv>

#include "lox/errorhandler.h"

namespace lox { namespace
{


bool
is_num_char(char c)
{
    return '0' <= c && c <= '9';
}


bool
is_alpha_char(char c)
{
    const bool is_lower_case = 'a' <= c && c <= 'z';
    const bool is_upper_case = 'A' <= c && c <= 'Z';
    return is_lower_case || is_upper_case || c == '_';
}


bool
is_alphanum_char(char c)
{
    return is_alpha_char(c) || is_num_char(c);
}


float
parse_double(std::string_view str)
{
    float val;
    std::from_chars(str.data(), str.data() + str.size(), val);
    return val;
}

// todo(Gustav): remove this
std::string_view
substr(std::string_view str, std::size_t start_index, std::size_t end_index)
{
    assert(end_index >= start_index);
    return str.substr(start_index, end_index - start_index);
}


std::optional<TokenType>
find_keyword_or_null(std::string_view str)
{
    // todo(Gustav): just constinit this instead of going via a local function
    static const std::unordered_map<std::string_view, TokenType> keywords = []() -> auto
    {
        std::unordered_map<std::string_view, TokenType> keywords;
        keywords["and"] = TokenType::AND;
        keywords["class"] = TokenType::CLASS;
        keywords["else"] = TokenType::ELSE;
        keywords["false"] = TokenType::FALSE;
        keywords["for"] = TokenType::FOR;
        keywords["fun"] = TokenType::FUN;
        keywords["if"] = TokenType::IF;
        keywords["nil"] = TokenType::NIL;
        keywords["or"] = TokenType::OR;
        keywords["print"] = TokenType::PRINT;
        keywords["return"] = TokenType::RETURN;
        keywords["super"] = TokenType::SUPER;
        keywords["this"] = TokenType::THIS;
        keywords["true"] = TokenType::TRUE;
        keywords["var"] = TokenType::VAR;
        keywords["while"] = TokenType::WHILE;
        return keywords;
    }();

    const auto found = keywords.find(str);
    if(found != keywords.end())
    {
        return found->second;
    }
    else
    {
        return std::nullopt;
    }
}


struct Scanner
{
    std::string_view source;
    ErrorHandler* error_handler;
    std::vector<Token> tokens;
    std::size_t start = 0;
    std::size_t current = 0;

    explicit Scanner(std::string_view s, ErrorHandler* eh)
        : source(s)
        , error_handler(eh)
    {
    }

    void
    scan_many_tokens()
    {
        while (!is_at_end())
        {
            // We are at the beginning of the next lexeme.
            start = current;
            scan_single_token();
        }

        tokens.emplace_back(TokenType::EOF, "", nullptr, Offset{start, current});
    }

    void
    scan_single_token()
    {
        const char first_char = advance();
        switch (first_char)
        {
        case '(': add_token(TokenType::LEFT_PAREN);  break;
        case ')': add_token(TokenType::RIGHT_PAREN); break;
        case '{': add_token(TokenType::LEFT_BRACE);  break;
        case '}': add_token(TokenType::RIGHT_BRACE); break;
        case ',': add_token(TokenType::COMMA);       break;
        case '.': add_token(TokenType::DOT);         break;
        case '-': add_token(TokenType::MINUS);       break;
        case '+': add_token(TokenType::PLUS);        break;
        case ';': add_token(TokenType::SEMICOLON);   break;
        case '*': add_token(TokenType::STAR);        break;

        case '!': add_token(match('=') ? TokenType::BANG_EQUAL    : TokenType::BANG);    break;
        case '=': add_token(match('=') ? TokenType::EQUAL_EQUAL   : TokenType::EQUAL);   break;
        case '<': add_token(match('=') ? TokenType::LESS_EQUAL    : TokenType::LESS);    break;
        case '>': add_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER); break;

        case '/':
            if (match('/'))
            {
                // A comment goes until the end of the line.
                eat_line();
            }
            else
            {
                add_token(TokenType::SLASH);
            }
            break;

        // Ignore whitespace.
        case ' ': case '\r': case '\t': case '\n':
            break;

        case '"': case '\'':
            parse_string(first_char);
            break;

        default:
            if (is_num_char(first_char))
            {
                parse_number();
            }
            else if (is_alpha_char(first_char))
            {
                parse_identifier_or_keyword();
            }
            else
            {
                error_handler->on_error(start, "Unexpected character.");
            }
            break;
        }
    }

    void
    parse_identifier_or_keyword()
    {
        while (is_alphanum_char(peek()))
        {
            advance();
        }

        const auto text = substr(source, start, current);
        const auto keyword_type = find_keyword_or_null(text);
        add_token(keyword_type.value_or(TokenType::IDENTIFIER));
    }

    void
    parse_number()
    {
        while (is_num_char(peek()))
        {
            advance();
        }

        // Look for a fractional part.
        if (peek() == '.' && is_num_char(peek_next()))
        {
            // Consume the "."
            advance();

            while (is_num_char(peek()))
            {
                advance();
            }
        }

        const auto str = substr(source, start, current);
        add_token(TokenType::NUMBER, std::make_shared<Number>(parse_double(str)));
    }

    void
    parse_string(char end_char)
    {
        while (peek() != end_char && !is_at_end())
        {
            advance();
        }

        if (is_at_end())
        {
            error_handler->on_error(Offset{start, current}, "Unterminated parse_string.");
            return;
        }

        // The closing ".
        advance();

        // Trim the surrounding quotes.
        assert(current > 0);
        auto value = substr(source, start + 1, current - 1);
        add_token(TokenType::STRING, std::make_shared<String>(value));
    }

    void
    eat_line()
    {
        while (peek() != '\n' && !is_at_end())
        {
            advance();
        }
    }

    bool
    match(char expected)
    {
        if (is_at_end())
        {
            return false;
        }

        if (source[current] != expected)
        {
            return false;
        }

        current++;
        return true;
    }

    char
    peek()
    {
        if (is_at_end())
        {
            return '\0';
        }
        else
        {
            return source[current];
        }
    }

    char
    peek_next()
    {
        if (current + 1 >= source.length())
        {
            return '\0';
        }
        else
        {
            return source[current + 1];
        }
    }

    bool
    is_at_end() const
    {
        return current >= source.length();
    }

    char
    advance()
    {
        const auto r = source[current];
        current++;
        return r;
    }

    void
    add_token(TokenType type)
    {
        add_token(type, nullptr);
    }

    void
    add_token(TokenType type, std::shared_ptr<Object> literal)
    {
        auto text = substr(source, start, current);
        tokens.emplace_back(Token(type, text, literal, Offset{start, current}));
    }
};
} }



namespace lox
{

std::vector<Token>
ScanTokens(std::string_view source, ErrorHandler* error_handler)
{
    auto scanner = Scanner(source, error_handler);
    scanner.scan_many_tokens();
    return scanner.tokens;
}

}
