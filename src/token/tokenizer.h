#pragma once

#include "../core/prelude.h"
#include "../core/str.h"

#include "util.h"

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
};

// `next_type` and `next_token` are the only methods that should be called externally
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
    // can return a very limited amount of types of tokens
    // specifically, it will only look at the first character of the next token and try to decide on that
    // TokenType peek_token() {
    //     u8 next_char = this->peek_non_white();
    //     return TokenType::Undefined;
    // }
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

    // TODO rework; this sucks
    // upon call, expected to have `source[at]` to be the first character of the operator
    // after being called, `source[at]` will be the character right after the operator
    Str parse_operator() {
        assert(!(
            ch::alphanum(source[at]) ||
            ch::white(source[at]) ||
            ch::bracket(source[at]) ||
            ch::delim(source[at]) ||
            this->is_at_comment()
        ));
        // if a unary op that needs to be read as a single symbol (think `!!true`), return it
        if(ch::unary_op(source[at])) { return source.slice(at++, 1); }
        usize start = at;
        // not unary
        while(!(
            this->eof() ||
            ch::alphanum(source[at]) ||
            ch::white(source[at]) ||
            ch::bracket(source[at]) ||
            ch::delim(source[at]) ||
            this->is_at_comment() ||
            ch::unary_op(source[at]) // if reading a binop, stop reading when seeing a unary op
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

    // upon call, expected to have `source[at]` to be the beginning of a type
    // after being called, `source[at]` will be the character right after end of the type
    Str parse_type() {
        Str base = this->parse_identifier();
        // type can be followed by *
        // TODO expand to arrays later
        while(this->peek() == '*') { at++; base.size++; } // very unintended by the `Str`, but should be safe here
        return base;
        // // TODO do more than just this
        // Str t = source.slice(at, 3);
        // at+=3;
        // assert(t == "u64"_s || t == "Str"_s);
        // return t;
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

    Token next_type() {
        this->skip_white();
        while(this->is_at_comment()) {
            this->skip_comment();
            this->skip_white();
        }
        if(this->eof()) return Token { source.slice(at, 0), TokenType::EndOfFile };
        // ^^^ basic error skipping and error checking
        Token type_token = { this->parse_type(), TokenType::DataType };
        return type_token;
    }

    // Cannot read a `TokenType::DataType` token type; if you expect a type to be read, use `Tokenizer::next_type_token()` instead
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
        return Token { this->parse_operator(), TokenType::Special };
    }
};