#pragma once

#include "../core/prelude.h"
#include "../core/str.h"

enum class Op {
    // Ambiguous
    Undefined, Minus, Star, Ampersand,

    // Unary
    Neg,
    LogiNot, // logical
    BitNot, // bitwise

    // Biary
    Add, Sub, Mul, Div, Mod, // arithmetic
    LogiOr, LogiAnd, // logical
    BitOr, BitAnd, BitXor, // bitwise
    Eq, Less, Greater, LessEq, GreaterEq, // comparison

    Assignment, // special; RIGHT ASSOCIATIVE
};

std::ostream& operator<<(std::ostream& os, Op op);

namespace op {
    // high priority = apply first
    u8 p(Op op) {
        switch(op) {
            case Op::Neg:
            case Op::LogiNot:
            case Op::BitNot:
                return 10;

            case Op::Mul:
            case Op::Div:
            case Op::Mod:
                return 8;

            case Op::Add:
            case Op::Sub:
                return 6;

            case Op::BitAnd:
            case Op::BitXor:
            case Op::BitOr:
                return 4;

            case Op::Eq:
            case Op::Less:
            case Op::Greater:
            case Op::LessEq:
            case Op::GreaterEq:
                return 3;

            case Op::LogiAnd:
            case Op::LogiOr:
                return 2;

            case Op::Assignment:
                return 0;
            default:
                printe("getting priority of an invalid operator", op);
                panic;
        }
    }

    bool unary(Op op) {
        switch(op) {
            case Op::Neg:
            case Op::LogiNot:
            case Op::BitNot:
                return true;
            
            case Op::Mul:
            case Op::Div:
            case Op::Mod:
            case Op::Add:
            case Op::Sub:
            case Op::BitAnd:
            case Op::BitXor:
            case Op::BitOr:
            case Op::Eq:
            case Op::Less:
            case Op::Greater:
            case Op::LessEq:
            case Op::GreaterEq:
            case Op::LogiAnd:
            case Op::LogiOr:
            case Op::Assignment:
                return false;
            
            default:
                printe("checking `unary` of an ambiguous op", op);
                panic;
        }
    }

    bool binary(Op op) {
        switch(op) {
            case Op::Neg:
            case Op::LogiNot:
            case Op::BitNot:
                return false;
            
            case Op::Mul:
            case Op::Div:
            case Op::Mod:
            case Op::Add:
            case Op::Sub:
            case Op::BitAnd:
            case Op::BitXor:
            case Op::BitOr:
            case Op::Eq:
            case Op::Less:
            case Op::Greater:
            case Op::LessEq:
            case Op::GreaterEq:
            case Op::LogiAnd:
            case Op::LogiOr:
            case Op::Assignment:
                return true;
            
            default:
                printe("checking `binary` of an ambiguous op", op);
                panic;
        }
    }

    bool ambiguous(Op op) {
        switch(op) {
            case Op::Undefined:
            case Op::Minus:
            case Op::Star:
            case Op::Ampersand:
                return true;
            
            default:
                return false;
        }
    }

    bool logi(Op op) {
        switch(op) {
            case Op::LogiOr:
            case Op::LogiAnd:
            case Op::LogiNot:
            case Op::Eq:
            case Op::Less:
            case Op::Greater:
            case Op::LessEq:
            case Op::GreaterEq:
                return true;
            default:
                return false;
        }
    }

    Op make_unary(Op op) {
        if(op == Op::Minus) return Op::Neg;
        if(op == Op::Star) todo; //return Op::Dereference;
        if(op == Op::Ampersand) todo; //return Op::AddrOf;
        printe("Cannot make unary", op);
        panic;
    }

    Op make_binary(Op op) {
        if(op == Op::Minus) return Op::Neg;
        if(op == Op::Star) return Op::Mul;
        if(op == Op::Ampersand) return Op::BitAnd;
        printe("Cannot make binary", op);
        panic;
    }

    // return true if `left` has precedence over `right`
    // aka in expression `a left b right c` we apply `(a left b) right c`
    bool has_precedence(Op left, Op right) {
        if(op::p(left) == 0) return false; // `=` is right associative AND lowest priority
        return op::p(left) >= op::p(right);
    }

    Str symbol(Op op) {
        switch (op) {
            case Op::Neg:           return "-"_s;
            case Op::LogiNot:       return "!"_s;
            case Op::BitNot:        return "~"_s;

            case Op::Add:           return "+"_s;
            case Op::Sub:           return "-"_s;
            case Op::Mul:           return "*"_s;
            case Op::Div:           return "/"_s;
            case Op::Mod:           return "%"_s;

            case Op::LogiOr:        return "||"_s;
            case Op::LogiAnd:       return "&&"_s;

            case Op::BitOr:         return "|"_s;
            case Op::BitAnd:        return "&"_s;
            case Op::BitXor:        return "^"_s;

            case Op::Eq:            return "=="_s;
            case Op::Less:          return "<"_s;
            case Op::Greater:       return ">"_s;
            case Op::LessEq:        return "<="_s;
            case Op::GreaterEq:     return ">="_s;

            case Op::Assignment:    return "="_s;

            case Op::Undefined:     { printe("unexpected op symbol request", "undefined"); return "undefined"_s; }
            case Op::Minus:         { printe("unexpected op symbol request", "-"); return "-(bad)"_s; }
            case Op::Star:          { printe("unexpected op symbol request", "*"); return "*(bad)"_s; }
            case Op::Ampersand:     { printe("unexpected op symbol request", "&"); return "&(bad)"_s; }
        }
        unreachable;
    }

    Op from_str(Str op) {
        // Ambiguous
        if (op == "-"_s) {
            return Op::Minus;
        } else if (op == "*"_s) {
            return Op::Star;
        } else if (op == "&"_s) {
            return Op::Ampersand;
        } else
        // Unambiguous
        if (op == "!"_s) {
            return Op::LogiNot;
        } else if (op == "~"_s) {
            return Op::BitNot;
        } else if (op == "+"_s) {
            return Op::Add;
        } else if (op == "/"_s) {
            return Op::Div;
        } else if (op == "%"_s) {
            return Op::Mod;
        } else if (op == "||"_s) {
            return Op::LogiOr;
        } else if (op == "&&"_s) {
            return Op::LogiAnd;
        } else if (op == "|"_s) {
            return Op::BitOr;
        } else if (op == "^"_s) {
            return Op::BitXor;
        } else if (op == "="_s) {
            return Op::Assignment;
        } else if (op == "=="_s) {
            return Op::Eq;
        } else if (op == "<"_s) {
            return Op::Less;
        } else if (op == ">"_s) {
            return Op::Greater;
        } else if (op == "<="_s) {
            return Op::LessEq;
        } else if (op == ">="_s) {
            return Op::GreaterEq;
        } else {
            return Op::Undefined;
        }
    }

    Op binary(Str str) {
        Op op = op::from_str(str);
        if(op::ambiguous(op)) return op::make_binary(op);
        else return op;
    }

    Op unary(Str str) {
        Op op = op::from_str(str);
        if(op::ambiguous(op)) return op::make_unary(op);
        else return op;
    }

    i64 apply(Op op, i64 left, i64 right) {
        assert(op::binary(op));
        switch (op) {
            case Op::Add:           return left + right;
            case Op::Sub:           return left - right;
            case Op::Mul:           return left * right;
            case Op::Div:           { if(right == 0) return 0; return left / right; }
            case Op::Mod:           { if(right == 0) return 0; return left % right; }

            case Op::LogiOr:        todo;
            case Op::LogiAnd:       todo;

            case Op::BitOr:         todo;
            case Op::BitAnd:        todo;
            case Op::BitXor:        todo;

            case Op::Eq:            todo;
            case Op::Less:          todo;
            case Op::Greater:       todo;
            case Op::LessEq:        todo;
            case Op::GreaterEq:     todo;

            case Op::Assignment:    todo;

            default: panic;
        }
        unreachable;
    }
}

/* Debug */

std::ostream& operator<<(std::ostream& os, Op op) {
    return os << op::symbol(op);
}