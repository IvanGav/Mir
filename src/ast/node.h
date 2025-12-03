#pragma once

#include "../core/prelude.h"
#include "../core/vec.h"
#include "../core/pair.h"

#include "../lang/op.h"
#include "../lang/type.h"

#include "../token/tokenizer.h"

#include "../llvm/static.h"

// IMPORTANT:
// Any time you see `P<Token,Token>`, that's representing a `var_name: var_type` pair

struct Node {
    Token token;
    virtual llvm::Value* codegen() = 0;
    virtual void debug_print() = 0;
};

/* specialized nodes */

struct NodeConst : Node {
    Type* val;

    llvm::Value* codegen() {
        assert(type::is_const(val));
        switch(val->ttype) {
            case TypeT::Int: {
                // TypeInt* ty = reinterpret_cast<TypeInt*>(val);
                // return llvm::ConstantInt::get(*llvm_context, llvm::APInt(ty->get_size_bytes()*8, ty->val_min, true)); // TODO signed or negative?
            }
            case TypeT::UInt: {
                // TypeInt* ty = reinterpret_cast<TypeInt*>(val);
                // return llvm::ConstantInt::get(*llvm_context, llvm::APInt(ty->get_size_bytes()*8, ty->val_min, false));
            }
            case TypeT::Bool: {
                panic; // TODO
                // TypeInt* ty = reinterpret_cast<TypeInt*>(val);
                // return llvm::ConstantInt::get(*llvm_context, llvm::APInt(ty->get_size_bytes()*8, val, false));
            }
            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(val);
                return llvm::ConstantFP::get(*llvm_context, llvm::APFloat(ty->val_min));
            }
            case TypeT::Str: {
                TypeStr* ty = reinterpret_cast<TypeStr*>(val);
                // return llvm::ConstantInt::get(*llvm_context, llvm::APInt(val->get_size_bytes()*8, val, val->is_signed));
                panic; // TODO
            }
        }
        panic;
    }
    void debug_print() {
        type::debug_print(val);
    }
};

// If `lhs` is nullptr, this is a unary operator
// unary operators can be promoted to binary operators
struct NodeOp : Node {
    op::OpType op;
    Node* lhs; // nullable
    Node* rhs;
    bool is_unary() { return op::is_unary(op); }

    llvm::Value* codegen() { panic; }
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
    llvm::Value* codegen() { panic; }
    void debug_print() {
        std::cout << "(var " << token.val << ")";
    }
};

// Callee name stored in the `token`
struct NodeCall : Node {
    Vec<Node*> args;

    llvm::Value* codegen() { panic; }
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
    Token ret_type;

    llvm::Value* codegen() { panic; }
    void debug_print() {
        std::cout << "fn (";
        for(P<Token,Token> n : args) { std::cout << "param " << n.a.val << " type " << n.b.val << ","; }
        std::cout << ")->(type " << ret_type.val << ")";
    }
};

// `token` is "fn"
struct NodeFnDef : Node {
    Node* proto;
    Node* body;

    llvm::Value* codegen() { panic; }
    void debug_print() {
        std::cout << "function {" << std::endl;
        proto->debug_print(); std::cout << std::endl;
        body->debug_print(); std::cout << std::endl;
        std::cout << "} function" << std::endl;
    }
};

struct NodeBlock : Node {
    Vec<Node*> list;

    llvm::Value* codegen() { panic; }
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

    llvm::Value* codegen() { panic; }
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

struct NodeWhile : Node {
    Node* condition;
    Node* body;

    llvm::Value* codegen() { panic; }
    void debug_print() {
        std::cout << "while ";
        condition->debug_print();
        std::cout << " {" << std::endl;
        body->debug_print(); std::cout << std::endl;
        std::cout << "} while" << std::endl;
    }
};

// `token` will be "let"
struct NodeVarDecl : Node {
    Token name;
    Token declared_type; // TODO
    Node* init; // nullable

    llvm::Value* codegen() { panic; }
    void debug_print() {
        assert(init != nullptr); // TODO
        std::cout << "vardecl " << name.val << " type " << declared_type.val << " = {" << std::endl;
        init->debug_print(); std::cout << std::endl;
        std::cout << "} vardecl" << std::endl;
    }
};

struct NodeRet : Node {
    Node* val;

    llvm::Value* codegen() { panic; }
    void debug_print() {
        std::cout << "return {" << std::endl;
        val->debug_print(); std::cout << std::endl;
        std::cout << "} return" << std::endl;
    }
};