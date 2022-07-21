#include "lox/scanner.h"

#include <unordered_map>
#include <charconv>

#include "lox/cint.h"
#include "lox/errorhandler.h"

namespace lox { namespace
{


bool
isDigit(char c)
{
    return c >= '0' && c <= '9';
}


bool
isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_';
}


bool
isAlphaNumeric(char c)
{
    return isAlpha(c) || isDigit(c);
}


float
parseDouble(std::string_view str)
{
    float val;
    std::from_chars(str.data(), str.data() + str.size(), val);
    return val;
}


std::string_view
substr(std::string_view str, std::size_t start_index, std::size_t end_index)
{
    assert(end_index >= start_index);
    return str.substr(start_index, end_index - start_index);
}


struct Scanner
{
    std::string_view source;
    ErrorHandler* error_handler;
    std::vector<Token> tokens;
    std::size_t start = 0;
    std::size_t current = 0;

    std::unordered_map<std::string_view, TokenType> keywords;

    explicit Scanner(std::string_view s, ErrorHandler* eh)
        : source(s)
        , error_handler(eh)
    {
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
    }

    void
    scanTokens()
    {
        while (!isAtEnd())
        {
            // We are at the beginning of the next lexeme.
            start = current;
            scanToken();
        }

        tokens.emplace_back(TokenType::EOF, "", nullptr, Offset{start, current});
    }

    void
    scanToken()
    {
        char c = advance();
        switch (c)
        {
        case '(': addToken(TokenType::LEFT_PAREN);  break;
        case ')': addToken(TokenType::RIGHT_PAREN); break;
        case '{': addToken(TokenType::LEFT_BRACE);  break;
        case '}': addToken(TokenType::RIGHT_BRACE); break;
        case ',': addToken(TokenType::COMMA);       break;
        case '.': addToken(TokenType::DOT);         break;
        case '-': addToken(TokenType::MINUS);       break;
        case '+': addToken(TokenType::PLUS);        break;
        case ';': addToken(TokenType::SEMICOLON);   break;
        case '*': addToken(TokenType::STAR);        break;

        case '!': addToken(match('=') ? TokenType::BANG_EQUAL    : TokenType::BANG);    break;
        case '=': addToken(match('=') ? TokenType::EQUAL_EQUAL   : TokenType::EQUAL);   break;
        case '<': addToken(match('=') ? TokenType::LESS_EQUAL    : TokenType::LESS);    break;
        case '>': addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER); break;

        case '/':
            if (match('/'))
            {
                // A comment goes until the end of the line.
                while (peek() != '\n' && !isAtEnd())
                {
                    advance();
                }
            }
            else
            {
                addToken(TokenType::SLASH);
            }
            break;

        // Ignore whitespace.
        case ' ': case '\r': case '\t': case '\n':
            break;

        case '"': case '\'':
            string(c);
            break;

        default:
            if (isDigit(c))
            {
                number();
            }
            else if (isAlpha(c))
            {
                identifier();
            }
            else
            {
                error_handler->on_error(start, "Unexpected character.");
            }
            break;
        }
    }

    void
    identifier()
    {
        while (isAlphaNumeric(peek()))
        {
            advance();
        }

        const auto text = substr(source, start, current);
        const auto type = keywords.find(text);
        if (type == keywords.end())
        {
            addToken(TokenType::IDENTIFIER);
        }
        else
        {
            addToken(type->second);
        }
    }

    void
    number()
    {
        while (isDigit(peek()))
        {
            advance();
        }

        // Look for a fractional part.
        if (peek() == '.' && isDigit(peekNext()))
        {
            // Consume the "."
            advance();

            while (isDigit(peek()))
            {
                advance();
            }
        }

        const auto str = substr(source, start, current);
        addToken(TokenType::NUMBER, std::make_shared<Number>(parseDouble(str)));
    }

    void
    string(char end_char)
    {
        while (peek() != end_char && !isAtEnd())
        {
            advance();
        }

        if (isAtEnd())
        {
            error_handler->on_error(Offset{start, current}, "Unterminated string.");
            return;
        }

        // The closing ".
        advance();

        // Trim the surrounding quotes.
        auto value = substr(source, start + 1, current - 1);
        addToken(TokenType::STRING, std::make_shared<String>(value));
    }

    bool
    match(char expected)
    {
        if (isAtEnd())
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
        if (isAtEnd())
        {
            return '\0';
        }
        else
        {
            return source[current];
        }
    }

    char
    peekNext()
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
    isAtEnd() const
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
    addToken(TokenType type)
    {
        addToken(type, nullptr);
    }

    void
    addToken(TokenType type, std::shared_ptr<Object> literal)
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
    scanner.scanTokens();
    return scanner.tokens;
}

}
