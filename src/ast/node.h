#pragma once

#include "../core/prelude.h"
#include "../core/vec.h"
#include "../core/pair.h"

#include "../token/tokenizer.h"

// IMPORTANT:
// Any time you see `P<Token,Token>`, that's representing a `var_name: var_type` pair

enum class OpType {
    // Biary
    Add, Sub, Mul, Div, Mod, // arithmetic
    LogiOr, LogiAnd, // logical
    BitOr, BitAnd, BitXor, // bitwise

    // Unary
    Neg, // arithmetic
    LogiNot, // logical
    BitNot, // bitwise
};

struct Node {
    Token token;
    virtual void codegen(Vec<u8>& gen) = 0;
    virtual ~Node() = 0;
};

/* specialized nodes */

struct NodeRet : Node {
    Node* val;
    void codegen(Vec<u8>& gen) {}
};

struct NodeConst : Node {
    u64 val;
    void codegen(Vec<u8>& gen) {}
};

// If `lhs` is nullptr, this is a unary operator
struct NodeOp : Node {
    OpType op;
    Node* lhs;
    Node* rhs;
    void codegen(Vec<u8>& gen) {}
};

// Var name stored in the `token`
struct NodeVar : Node {
    void codegen(Vec<u8>& gen) {}
};

// Callee name stored in the `token`
struct NodeCall : Node {
    Vec<Node*> args;
    void codegen(Vec<u8>& gen) {}
};

// `token` will be "fn"
struct NodeFnProto : Node {
    Token name;
    Vec<P<Token,Token>> args;
    P<Token,Token> ret;
    void codegen(Vec<u8>& gen) {}
};

// `token` is undefined
struct NodeFnDef : Node {
    Node* proto;
    Node* body;
    void codegen(Vec<u8>& gen) {}
};