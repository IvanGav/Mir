#pragma once

#include "parser.h"

std::ostream& operator<<(std::ostream& os, TokenType type) {
    switch (type) {
        case TokenType::EndOfFile:          return os << "EndOfFile";
        case TokenType::EndOfLine:          return os << "EndOfLine";
        case TokenType::IntLiteral:         return os << "IntLiteral";
        case TokenType::FloatLiteral:       return os << "FloatLiteral";
        case TokenType::StringLiteral:      return os << "StringLiteral";
        case TokenType::Return:             return os << "Return";
        case TokenType::Operator:           return os << "Operator";
        case TokenType::Identifier:         return os << "Identifier";
        case TokenType::LeftParenthese:     return os << "LeftParenthese";
        case TokenType::RightParenthese:    return os << "RightParenthese";
        case TokenType::LeftBracket:        return os << "LeftBracket";
        case TokenType::RightBracket:       return os << "RightBracket";
        case TokenType::LeftCurly:          return os << "LeftCurly";
        case TokenType::RightCurly:         return os << "RightCurly";
        
        case TokenType::If:                 return os << "If";
        case TokenType::While:              return os << "While";
        case TokenType::VarDecl:            return os << "VarDecl";
        case TokenType::FunctionDecl:       return os << "FunctionDecl";
    }
    return os << "<Unknown TokenType>";
}

std::ostream& operator<<(std::ostream& os, const Token& tok) {
    os << "{ type: " << tok.tt << ", val: \"" << tok.val << "\" }";
    return os;
}