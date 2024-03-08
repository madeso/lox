#pragma once

#ifdef EOF
#undef EOF
#endif

#include <string_view>
#include <cassert>


namespace lox
{
    enum class TokenType
    {
        // Single-character tokens.
        LEFT_PAREN, RIGHT_PAREN,
        LEFT_BRACE, RIGHT_BRACE,
        LEFT_BRACKET, RIGHT_BRACKET,
        COMMA, DOT, SEMICOLON,
        COLON,

        // One or two character tokens.
        BANG, BANG_EQUAL,
        EQUAL, EQUAL_EQUAL,
        GREATER, GREATER_EQUAL,
        LESS, LESS_EQUAL,

        PLUS, PLUSEQ,
        SLASH, SLASHEQ,
        STAR, STAREQ,

        MINUS, ARROW, MINUSEQ,

        // Literals.
        IDENTIFIER, STRING, NUMBER_INT, NUMBER_FLOAT,

        // Keywords.
        AND, CLASS, ELSE, FALSE, FUN, FOR, IF, NIL, OR,
        PRINT, RETURN, SUPER, THIS, TRUE, VAR, WHILE,

        NEW, STATIC, CONST,

        EOF
    };

    constexpr std::string_view tokentype_to_string(TokenType tt)
    {
        switch (tt)
        {
        case TokenType::LEFT_PAREN:     return "left_paren";
        case TokenType::RIGHT_PAREN:    return "right_paren";
        case TokenType::LEFT_BRACE:     return "left_brace";
        case TokenType::RIGHT_BRACE:    return "right_brace";
        case TokenType::LEFT_BRACKET:   return "left_bracket";
        case TokenType::RIGHT_BRACKET:  return "right_bracket";
        case TokenType::COMMA:          return "comma";
        case TokenType::DOT:            return "dot";
        case TokenType::SEMICOLON:      return "semicolon";
        case TokenType::COLON:          return "colon";
        case TokenType::BANG:           return "bang";
        case TokenType::BANG_EQUAL:     return "bang_equal";
        case TokenType::EQUAL:          return "equal";
        case TokenType::EQUAL_EQUAL:    return "equal_equal";
        case TokenType::GREATER:        return "greater";
        case TokenType::GREATER_EQUAL:  return "greater_equal";
        case TokenType::LESS:           return "less";
        case TokenType::LESS_EQUAL:     return "less_equal";
        case TokenType::PLUS:           return "plus";
        case TokenType::PLUSEQ:         return "plus_eq";
        case TokenType::SLASH:          return "slash";
        case TokenType::SLASHEQ:        return "slash_eq";
        case TokenType::STAR:           return "star";
        case TokenType::STAREQ:         return "star_eq";
        case TokenType::MINUS:          return "minus";
        case TokenType::ARROW:          return "arrow";
        case TokenType::MINUSEQ:        return "minus_eq";
        case TokenType::IDENTIFIER:     return "identifier";
        case TokenType::STRING:         return "string";
        case TokenType::NUMBER_FLOAT:   return "float";
        case TokenType::NUMBER_INT:     return "int";
        case TokenType::AND:            return "and";
        case TokenType::CLASS:          return "class";
        case TokenType::ELSE:           return "else";
        case TokenType::FALSE:          return "false";
        case TokenType::FUN:            return "fun";
        case TokenType::FOR:            return "for";
        case TokenType::IF:             return "if";
        case TokenType::NIL:            return "nil";
        case TokenType::OR:             return "or";
        case TokenType::PRINT:          return "print";
        case TokenType::RETURN:         return "return";
        case TokenType::SUPER:          return "super";
        case TokenType::THIS:           return "this";
        case TokenType::TRUE:           return "true";
        case TokenType::VAR:            return "var";
        case TokenType::WHILE:          return "while";
        case TokenType::NEW:            return "new";
        case TokenType::STATIC:         return "static";
        case TokenType::CONST:          return "const";
        case TokenType::EOF:            return "eof";
        default:                        assert(false); return "???";
        }
    }

    constexpr std::string_view tokentype_to_string_short(TokenType tt)
    {
        switch (tt)
        {
        case TokenType::LEFT_PAREN:     return "(";
        case TokenType::RIGHT_PAREN:    return ")";
        case TokenType::LEFT_BRACE:     return "{";
        case TokenType::RIGHT_BRACE:    return "}";
        case TokenType::LEFT_BRACKET:   return "[";
        case TokenType::RIGHT_BRACKET:  return "]";
        case TokenType::COMMA:          return ",";
        case TokenType::DOT:            return ".";
        case TokenType::SEMICOLON:      return ";";
        case TokenType::COLON:          return ":";
        case TokenType::BANG:           return "!";
        case TokenType::BANG_EQUAL:     return "!=";
        case TokenType::EQUAL:          return "=";
        case TokenType::EQUAL_EQUAL:    return "==";
        case TokenType::GREATER:        return ">";
        case TokenType::GREATER_EQUAL:  return ">=";
        case TokenType::LESS:           return "<";
        case TokenType::LESS_EQUAL:     return "<=";
        case TokenType::PLUS:           return "+";
        case TokenType::PLUSEQ:         return "+=";
        case TokenType::SLASH:          return "/";
        case TokenType::SLASHEQ:        return "/=";
        case TokenType::STAR:           return "*";
        case TokenType::STAREQ:         return "*=";
        case TokenType::MINUS:          return "-";
        case TokenType::ARROW:          return "->";
        case TokenType::MINUSEQ:        return "-=";
        case TokenType::IDENTIFIER:     return "identifier";
        case TokenType::STRING:         return "string";
        case TokenType::NUMBER_FLOAT:   return "float";
        case TokenType::NUMBER_INT:     return "int";
        case TokenType::AND:            return "and";
        case TokenType::CLASS:          return "class";
        case TokenType::ELSE:           return "else";
        case TokenType::FALSE:          return "false";
        case TokenType::FUN:            return "fun";
        case TokenType::FOR:            return "for";
        case TokenType::IF:             return "if";
        case TokenType::NIL:            return "nil";
        case TokenType::OR:             return "or";
        case TokenType::PRINT:          return "print";
        case TokenType::RETURN:         return "return";
        case TokenType::SUPER:          return "super";
        case TokenType::THIS:           return "this";
        case TokenType::TRUE:           return "true";
        case TokenType::VAR:            return "var";
        case TokenType::WHILE:          return "while";
        case TokenType::NEW:            return "new";
        case TokenType::STATIC:         return "static";
        case TokenType::CONST:          return "const";
        case TokenType::EOF:            return "eof";
        default:                        assert(false); return "???";
        }
    }
}
