#pragma once

#include "../core/prelude.h"
#include "../core/str.h"

namespace ch {
    // end of a character string
    bool white(u8 ch) {
        return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r';
    }
    bool num(u8 ch) {
        return ch >= '0' && ch <= '9';
    }
    bool alpha(u8 ch) {
        return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    }
    bool alphanum(u8 ch) {
        return ch::num(ch) || ch::alpha(ch);
    }
    bool bracket(u8 ch) {
        return ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '(' || ch == ')';
    }
    bool semicol(u8 ch) {
        return ch == ';';
    }
    bool comment(u8 ch) {
        return ch == '#';
    }
}

enum class TokenType {
    EndOfFile, EndOfLine, IntLiteral, FloatLiteral, StringLiteral, Return, Operator, Identifier,
    If, While, VarDecl, FunctionDecl,
    LeftParenthese, RightParenthese, LeftBracket, RightBracket, LeftCurly, RightCurly,
};

struct Token {
    Str val;
    TokenType tt;
};

struct Parser {
    Str source;
    usize at;

    void reset() { at = 0; }

    // return '\0' if at end of file
    // be careful at exclusive while loops (such as one in `Parser::skip_comment()`)
    u8 peek() { if(this->eof()) { return '\0'; } return source[at]; }
    u8 peek_non_white() { this->skip_white(); return this->peek(); }
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

    // upon call, expected to have `source[at]` to be the first character of the number literal
    // after being called, `source[at]` will be the character right after the number literal
    // TODO support for parsing float values
    Str parse_number_literal() {
        assert(ch::num(source[at]));
        usize start = at;
        while(ch::num(this->peek())) { at++; }
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

    // upon call, expected to have `source[at]` to be the first character of the operator
    // after being called, `source[at]` will be the character right after the operator
    Str parse_operator() {
        assert(!(
            ch::alphanum(source[at]) ||
            ch::white(source[at]) ||
            ch::bracket(source[at]) ||
            ch::semicol(source[at]) ||
            this->is_at_comment()
        ));
        usize start = at;
        while(!(
            this->eof() ||
            ch::alphanum(source[at]) ||
            ch::white(source[at]) ||
            ch::bracket(source[at]) ||
            ch::semicol(source[at]) ||
            this->is_at_comment()
        )) { at++; }
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

    Token next_bracket() {
        Token t { source.slice(at, 1) };
        at++;
        if(t.val[0] == '(') t.tt = TokenType::LeftParenthese;
        else if(t.val[0] == ')') t.tt = TokenType::RightParenthese;
        else if(t.val[0] == '[') t.tt = TokenType::LeftBracket;
        else if(t.val[0] == ']') t.tt = TokenType::RightBracket;
        else if(t.val[0] == '{') t.tt = TokenType::LeftCurly;
        else if(t.val[0] == '}') t.tt = TokenType::RightCurly;
        return t;
    }

    // Cannot read a `TokenType::Type` token type; if you expect a type to be read, use `Parser::next_type_token()` instead
    Token next_token() {
        this->skip_white(); // ignore leading whitespace
        while(this->is_at_comment()) {
            this->skip_comment();
            this->skip_white();
        }
        if(this->eof()) return Token { source.slice(at, 0), TokenType::EndOfFile };
        if(this->peek() == ';') {
            return Token { source.slice(at++, 1), TokenType::EndOfLine };
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
            if(token_val == "while"_s) return Token { token_val, TokenType::While };
            if(token_val == "let"_s) return Token { token_val, TokenType::VarDecl };
            if(token_val == "fn"_s) return Token { token_val, TokenType::FunctionDecl };
            return Token { token_val, TokenType::Identifier }; // generic identifier
        }
        // assume an operator
        return Token { this->parse_operator(), TokenType::Operator };
    }
};