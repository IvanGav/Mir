#pragma once

#include "../core/prelude.h"
#include "../core/str.h"

// character category helper functions
namespace ch {
    bool eof(u8 ch) {
        return ch == '\0';
    }
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
    bool delim(u8 ch) {
        return ch == ';' || ch == ',';
    }
    // symbols that terminate an "expression"
    bool terminal(u8 ch) {
        return ch == ')' || ch == ']' || ch == '}' || ch::delim(ch) || ch::eof(ch);
    }
}