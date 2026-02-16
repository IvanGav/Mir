#pragma once

#include "../core/prelude.h"
#include "../core/str.h"

#include "../lang/util.h"

enum class TokenType {
    Undefined, EndOfFile, EndOfLine, Comma, // special identifiers
    IntLiteral, FloatLiteral, StringLiteral, // literals
    If, Else, While, VarDecl, FunctionDecl, Return, // keywords (respective): `if`, `else`, `while`, `let`, `fn`, `return`
    LeftParenthese, RightParenthese, LeftBracket, RightBracket, LeftCurly, RightCurly, // brackets of all kinds
    Identifier, DataType, Special, // other
};

struct Token {
    Str val;
    TokenType tt;

    static const Token eof;
    static const Token empty;

    bool operator==(const Token& other) const { return val == other.val && tt == other.tt; }
};

const Token Token::empty = {}; // wtf c++
const Token Token::eof { .val={ .data=nullptr, .size=0 }, .tt=TokenType::EndOfFile }; // wtf c++

// `next_token`, `next_type` and `next_binary_op` are the only methods that should be called externally
// `parse_*` methods will assume that `this` is already at the correct token and will consume that token, returning it as a slice
// `next_*` methods will read the next token and return it (as a `Token`)
// `skip_*` methods will consume some amount of characters and not return any slice
struct Tokenizer {
    Str source;
    usize at;

    static Tokenizer create(Str src) {
        return Tokenizer { .source = src, .at = 0 };
    }

    void reset() { at = 0; }

    // return '\0' if at end of file
    // be careful at exclusive while loops (such as one in `Tokenizer::skip_comment()`)
    u8 peek() { if(this->eof()) { return '\0'; } return source[at]; }
    u8 peek_non_white() { this->skip_white(); while(this->is_at_comment()) { this->skip_comment(); this->skip_white(); } return this->peek(); }
    bool eof() { return at >= source.size; }
    bool is_at_comment() { return this->peek() == '#'; }
    void skip_white() {
        while(ch::white(this->peek())) {
            at++;
        }
    }
    void skip_comment() {
        if(!is_at_comment()) return;
        while(!this->eof() && this->peek() != '\n') {
            at++;
        }
    }
    // Calling this will make sure that `at` is pointing to the start of a significant symbol
    void skip_white_and_comment() {
        this->skip_white();
        while(this->is_at_comment()) {
            this->skip_comment();
            this->skip_white();
        }
    }

    // upon call, expected to have `source[at]` to be the first character of the number literal
    // after being called, `source[at]` will be the character right after the number literal
    // CANNOT PARSE METHODS OF NUMBER LITERALS
    Str parse_number_literal() {
        assert(ch::num(source[at]));
        usize start = at;
        while(ch::num(this->peek())) { at++; }
        if(this->peek() == '.') {
            // assume it's a float
            at++;
            while(ch::num(this->peek())) { at++; }
        }
        return source.slice_range(start, at);
    }

    // upon call, expected to have `source[at]` to be the first character of the identifier
    // after being called, `source[at]` will be the character right after the identifier
    Str parse_identifier() {
        assert(ch::alpha(source[at]));
        usize start = at;
        while(ch::alphanum(this->peek())) { at++; }
        return source.slice_range(start, at);
    }

    // upon call, expected to have `source[at]` to be the opening `"`
    // after being called, `source[at]` will be the character right after the closing `"`
    Str parse_string_literal() {
        assert(source[at] == '"');
        usize start = at;
        at++;
        while(!this->eof() && this->peek() != '"') { if(this->peek() == '\\') { at++; } at++; }
        at++;
        return source.slice_range(start, at);
    }

    // upon call, expected to have `source[at]` to be the beginning of a type
    // after being called, `source[at]` will be the character right after end of the type
    Str parse_type() {
        Str base = this->parse_identifier();
        // type can be followed by *
        // TODO expand to arrays later
        while(this->peek() == '*') { at++; base.size++; } // very unintended by the `Str`, but should be safe here
        return base;
    }

    Token next_bracket() {
        Token t { source.slice(at, 1) };
        at++;
        if(t.val[0] == '(') t.tt = TokenType::LeftParenthese;
        else if(t.val[0] == ')') t.tt = TokenType::RightParenthese;
        else if(t.val[0] == '[') t.tt = TokenType::LeftBracket;
        else if(t.val[0] == ']') t.tt = TokenType::RightBracket;
        else if(t.val[0] == '{') t.tt = TokenType::LeftCurly;
        else if(t.val[0] == '}') t.tt = TokenType::RightCurly;
        else panic;
        return t;
    }

    Token next_unary_op() {
        this->skip_white_and_comment();
        // if(this->eof()) return Token { source.slice(at, 0), TokenType::EndOfFile }; // TODO delete
        if(this->eof()) return Token::eof;
        // always single symbol
        switch(this->peek()) {
            case '-':
            case '!':
            case '~':
            case '&':
            case '*':
                return Token { .val=source.slice(at++, 1), .tt=TokenType::Special };
            default:
                return Token { .val=source.slice(at++, 1), .tt=TokenType::Undefined };
        }
    }

    Token next_binary_op() {
        this->skip_white_and_comment();
        // if(this->eof()) return Token { source.slice(at, 0), TokenType::EndOfFile }; // TODO delete
        if(this->eof()) return Token::eof;
        switch(this->peek()) {
            // single symbol
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '^':
                return Token { .val=source.slice(at++, 1), .tt=TokenType::Special };

            // can be single or double symbol
            case '&':
            case '|':
            case '=': {
                usize start = at; at++;
                if(this->peek() == source[start]) {
                    at++;
                    return Token { .val=source.slice(start, 2), .tt=TokenType::Special };
                } else {
                    return Token { .val=source.slice(start, 1), .tt=TokenType::Special };
                }
            }

            // can be followed by `=`
            case '<':
            case '>': {
                usize start = at; at++;
                if(this->peek() == '=') {
                    at++;
                    return Token { .val=source.slice(start, 2), .tt=TokenType::Special };
                } else {
                    return Token { .val=source.slice(start, 1), .tt=TokenType::Special };
                }
            }

            default:
                return Token { .val=source.slice(at++, 1), .tt=TokenType::Undefined };
        }
    }

    Token next_type() {
        this->skip_white_and_comment();
        // if(this->eof()) return Token { source.slice(at, 0), TokenType::EndOfFile }; // TODO delete
        if(this->eof()) return Token::eof;
        Token type_token = { this->parse_type(), TokenType::DataType };
        return type_token;
    }

    // Cannot read a `TokenType::DataType` token type; if you expect a type to be read, use `Tokenizer::next_type()` instead
    // Cannot read a binary operator; if you expect a binop to be read, use `Tokenizer::next_binary_op()` instead
    Token next_token() {
        this->skip_white_and_comment();
        // if(this->eof()) return Token { source.slice(at, 0), TokenType::EndOfFile }; // TODO delete
        if(this->eof()) return Token::eof;
        if(this->peek() == ';') {
            return Token { source.slice(at++, 1), TokenType::EndOfLine };
        }
        if(this->peek() == ',') {
            return Token { source.slice(at++, 1), TokenType::Comma };
        }
        if(this->peek() == '"') {
            return Token { this->parse_string_literal(), TokenType::StringLiteral };
        }
        if(ch::bracket(this->peek())) {
            return this->next_bracket();
        }
        if(ch::num(this->peek())) {
            return Token { this->parse_number_literal(), TokenType::IntLiteral };
        }
        if(ch::alpha(this->peek())) {
            Str token_val = this->parse_identifier();
            if(token_val == "if"_s) return Token { token_val, TokenType::If };
            if(token_val == "else"_s) return Token { token_val, TokenType::Else };
            if(token_val == "while"_s) return Token { token_val, TokenType::While };
            if(token_val == "let"_s) return Token { token_val, TokenType::VarDecl };
            if(token_val == "fn"_s) return Token { token_val, TokenType::FunctionDecl };
            if(token_val == "return"_s) return Token { token_val, TokenType::Return };
            return Token { token_val, TokenType::Identifier }; // generic identifier
        }
        // assume an operator
        return this->next_unary_op();
    }
};

/* Printing */

Str to_str(TokenType tt) {
    switch (tt) {
        case TokenType::Undefined:          return "Undefined"_s;
        case TokenType::EndOfFile:          return "EndOfFile"_s;
        case TokenType::EndOfLine:          return "EndOfLine"_s;
        case TokenType::Comma:              return "Comma"_s;

        case TokenType::IntLiteral:         return "IntLiteral"_s;
        case TokenType::FloatLiteral:       return "FloatLiteral"_s;
        case TokenType::StringLiteral:      return "StringLiteral"_s;
        
        case TokenType::If:                 return "If"_s;
        case TokenType::Else:               return "Else"_s;
        case TokenType::While:              return "While"_s;
        case TokenType::VarDecl:            return "VarDecl"_s;
        case TokenType::FunctionDecl:       return "FunctionDecl"_s;
        case TokenType::Return:             return "Return"_s;

        case TokenType::LeftParenthese:     return "LeftParenthese"_s;
        case TokenType::RightParenthese:    return "RightParenthese"_s;
        case TokenType::LeftBracket:        return "LeftBracket"_s;
        case TokenType::RightBracket:       return "RightBracket"_s;
        case TokenType::LeftCurly:          return "LeftCurly"_s;
        case TokenType::RightCurly:         return "RightCurly"_s;
        
        case TokenType::Identifier:         return "Identifier"_s;
        case TokenType::DataType:           return "DataType"_s;
        case TokenType::Special:            return "Special"_s;
    }
    unreachable;
}

std::ostream& operator<<(std::ostream& os, TokenType type) {
    return os << to_str(type);
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "{ type: " << token.tt << ", val: \"" << token.val << "\" }";
    return os;
}