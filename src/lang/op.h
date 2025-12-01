#pragma once

#include "../core/prelude.h"

namespace op {
    constexpr u8 UNARY_NUM = 3; // number of unary operators
    enum class OpType {
        // Unary
        Neg, // arithmetic
        LogiNot, // logical
        BitNot, // bitwise

        // Biary
        Add, Sub, Mul, Div, Mod, // arithmetic
        LogiOr, LogiAnd, // logical
        BitOr, BitAnd, BitXor, // bitwise
        Eq, Less, Greater, LessEq, GreaterEq, // comparison

        Assignment, // special; RIGHT ASSOCIATIVE
        Undefined,
    };

    // high priority = apply first
    u8 p(OpType op) {
        switch(op) {
            case OpType::Neg:
            case OpType::LogiNot:
            case OpType::BitNot:
                return 10;

            case OpType::Mul:
            case OpType::Div:
            case OpType::Mod:
                return 8;

            case OpType::Add:
            case OpType::Sub:
                return 6;

            case OpType::BitAnd:
            case OpType::BitXor:
            case OpType::BitOr:
                return 4;

            case OpType::Eq:
            case OpType::Less:
            case OpType::Greater:
            case OpType::LessEq:
            case OpType::GreaterEq:
                return 3;

            case OpType::LogiAnd:
            case OpType::LogiOr:
                return 2;

            case OpType::Undefined:
            case OpType::Assignment:
                return 0;
        }
        unreachable;
    }

    // return true if `left` has precedence over `right`
    // aka in expression `a left b right c` we apply `(a left b) right c`
    bool has_precedence(OpType left, OpType right) {
        assert(u8(left) >= UNARY_NUM); //  there's no `a`
        assert(u8(right) >= UNARY_NUM); //  there's no `(a left b)`

        if(p(left) == 0) return false; // `=` are right associative AND lowest priority
        return p(left) >= p(right);
    }

    Str symbol(OpType op) {
        switch (op) {
            case OpType::Neg:           return "-"_s;
            case OpType::LogiNot:       return "!"_s;
            case OpType::BitNot:        return "~"_s;

            case OpType::Add:           return "+"_s;
            case OpType::Sub:           return "-"_s;
            case OpType::Mul:           return "*"_s;
            case OpType::Div:           return "/"_s;
            case OpType::Mod:           return "%"_s;
            case OpType::LogiOr:        return "||"_s;
            case OpType::LogiAnd:       return "&&"_s;
            case OpType::BitOr:         return "|"_s;
            case OpType::BitAnd:        return "&"_s;
            case OpType::BitXor:        return "^"_s;
            case OpType::Eq:            return "=="_s;
            case OpType::Less:          return "<"_s;
            case OpType::Greater:       return ">"_s;
            case OpType::LessEq:        return "<="_s;
            case OpType::GreaterEq:     return ">="_s;
            case OpType::Assignment:    return "="_s;
            case OpType::Undefined:     panic;
        }
        unreachable;
    }

    OpType from_str(Str op) {
        if (op == "-"_s) {
            return OpType::Neg;
        } else if (op == "!"_s) {
            return OpType::LogiNot;
        } else if (op == "~"_s) {
            return OpType::BitNot;
        } else 

        if (op == "+"_s) {
            return OpType::Add;
        // } else if (op == "-"_s) { // unary takes priority
        //     return OpType::Sub;
        } else if (op == "*"_s) {
            return OpType::Mul;
        } else if (op == "/"_s) {
            return OpType::Div;
        } else if (op == "%"_s) {
            return OpType::Mod;
        } else if (op == "||"_s) {
            return OpType::LogiOr;
        } else if (op == "&&"_s) {
            return OpType::LogiAnd;
        } else if (op == "|"_s) {
            return OpType::BitOr;
        } else if (op == "&"_s) {
            return OpType::BitAnd;
        } else if (op == "^"_s) {
            return OpType::BitXor;
        } else if (op == "="_s) {
            return OpType::Assignment;
        } else if (op == "=="_s) {
            return OpType::Eq;
        } else if (op == "<"_s) {
            return OpType::Less;
        } else if (op == ">"_s) {
            return OpType::Greater;
        } else if (op == "<="_s) {
            return OpType::LessEq;
        } else if (op == ">="_s) {
            return OpType::GreaterEq;
        } else {
            return OpType::Undefined;
        }
    }

    bool is_unary(OpType op) {
        return u8(op) >= op::UNARY_NUM;
    }
}

std::ostream& operator<<(std::ostream& os, op::OpType op) {
    return os << op::symbol(op);
}