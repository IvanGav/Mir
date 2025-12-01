#pragma once

#include "../core/prelude.h"
#include "../core/vec.h"
#include "../core/pair.h"

#include "../token/tokenizer.h"

// IMPORTANT:
// Any time you see `P<Token,Token>`, that's representing a `var_name: var_type` pair

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

    Assignment, // special; RIGHT ASSOCIATIVE
};

std::ostream& operator<<(std::ostream& os, OpType op) {
    switch (op) {
        case OpType::Neg:      return os << "-";
        case OpType::LogiNot:  return os << "!";
        case OpType::BitNot:   return os << "~";
        case OpType::Add:      return os << "+";
        case OpType::Sub:      return os << "-";
        case OpType::Mul:      return os << "*";
        case OpType::Div:      return os << "/";
        case OpType::Mod:      return os << "%";
        case OpType::LogiOr:   return os << "||";
        case OpType::LogiAnd:  return os << "&&";
        case OpType::BitOr:    return os << "|";
        case OpType::BitAnd:   return os << "&";
        case OpType::BitXor:   return os << "^";
        case OpType::Assignment: return os << "=";
    }
    unreachable;
}

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

        case OpType::LogiAnd:
        case OpType::LogiOr:
            return 2;

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

    // TODO check for right associative ones
    return p(left) >= p(right);
}

struct Node {
    Token token;
    // virtual void codegen(Vec<u8>& gen) {};
    // virtual void debug_print() { std::cout << "I fucking despise OOP" << std::endl; };
    virtual void codegen(Vec<u8>& gen) = 0;
    virtual void debug_print() = 0;
};

/* specialized nodes */

struct NodeRet : Node {
    Node* val;
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << "return {" << std::endl;
        val->debug_print(); std::cout << std::endl;
        std::cout << "} return" << std::endl;
    }
};

struct NodeConst : Node {
    u64 val;
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << "(const " << val << ")";
    }
};

// If `lhs` is nullptr, this is a unary operator
// unary operators can be promoted to binary operators
struct NodeOp : Node {
    OpType op;
    Node* lhs; // nullable
    Node* rhs;
    bool is_unary() { return u8(op) >= UNARY_NUM; }
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << '(';
        if(lhs != nullptr) lhs->debug_print();
        std::cout << op;
        rhs->debug_print();
        std::cout << ')';
    }
};

// Var name stored in the `token`
struct NodeVar : Node {
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << "(var " << token.val << ")";
    }
};

// Callee name stored in the `token`
struct NodeCall : Node {
    Vec<Node*> args;
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << "(call " << token.val << ")->(";
        for(Node* n : args) { n->debug_print(); std::cout << ","; }
        std::cout << ")";
    }
};

// `token` will be "fn"
struct NodeFnProto : Node {
    Token name;
    Vec<P<Token,Token>> args;
    P<Token,Token> ret;
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        panic;
    }
};

// `token` is undefined
struct NodeFnDef : Node {
    Node* proto;
    Node* body;
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        panic;
    }
};

struct NodeBlock : Node {
    Vec<Node*> list;
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << "block {" << std::endl;
        for(Node* n : list) { n->debug_print(); std::cout << std::endl; }
        std::cout << "} block" << std::endl;
    }
};

// `condition` and `body` must have same size and correspond to:
// - first `if` clause
// - subsequent `else if` clauses
struct NodeIf : Node {
    Vec<Node*> condition;
    Vec<Node*> body;
    Node* else_clause; // nullable
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        std::cout << "ifblock {" << std::endl;
        for(usize i = 0; i < body.size; i++) {
            std::cout << "if {";
            condition[i]->debug_print();
            std::cout << "} then {";
            body[i]->debug_print();
            std::cout << "}";
            std::cout << std::endl;
        }
        if(else_clause != nullptr) {
            std::cout << "else {";
            else_clause->debug_print();
            std::cout << "}";
            std::cout << std::endl;
        }
        std::cout << "} ifblock" << std::endl;
    }
};

// `token` will be "let"
struct NodeVarDecl : Node {
    Token name;
    Token declared_type; // TODO
    Node* init; // nullable
    void codegen(Vec<u8>& gen) {}
    void debug_print() {
        assert(init != nullptr); // TODO
        std::cout << "vardecl " << name.val << " type " << declared_type.val << " = {" << std::endl;
        init->debug_print(); std::cout << std::endl;
        std::cout << "} vardecl" << std::endl;
    }
};