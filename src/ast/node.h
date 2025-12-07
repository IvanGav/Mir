#pragma once

#include "../core/prelude.h"
#include "../core/vec.h"
#include "../core/pair.h"

#include "../lang/op.h"
#include "../lang/type.h"

#include "../token/tokenizer.h"

#include "../llvm/static.h"
#include <llvm/Support/type_traits.h>

// IMPORTANT:
// Any time you see `P<Token,Token>`, that's representing a `var_name: var_type` pair

struct Node {
    Token token;
    virtual llvm::Value* codegen() = 0;
    virtual Type* compute_type() = 0;
    virtual void debug_print() = 0;
};

/* specialized nodes */

struct NodeConst : Node {
    Type* val;

    llvm::Value* codegen() {
        assert(type::is_const(val));
        switch(val->ttype) {
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(val);
                return llvm::ConstantInt::get(*llvm_context, llvm::APInt(ty->get_size_bytes()*8, ty->val_min, true)); // TODO signed or negative?
            }
            case TypeT::UInt: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(val);
                return llvm::ConstantInt::get(*llvm_context, llvm::APInt(ty->get_size_bytes()*8, ty->val_min, false));
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
    Type* compute_type() { return val; }
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
    llvm::Value* codegen_binary() {
        Type* lt = lhs->compute_type();
        Type* rt = rhs->compute_type();
        llvm::Value* lv = lhs->codegen();
        llvm::Value* rv = rhs->codegen();
        if (!lv || !rv) panic;
        // TODO remove pures
        if(lt->ttype != TypeT::Pure && rt->ttype != TypeT::Pure && lt->ttype != rt->ttype) panic;
        // TODO temp
        assert(lt->ttype == TypeT::UInt || lt->ttype == TypeT::Pure);

        // switch (op) {
        //     case op::OpType::Add:
        //         return llvm_builder->CreateFAdd(lv, rv, "addtmp");
        //     case op::OpType::Sub:
        //         return llvm_builder->CreateFSub(lv, rv, "subtmp");
        //     case op::OpType::Mul:
        //         return llvm_builder->CreateFMul(lv, rv, "multmp");
        //     case op::OpType::Less:
        //         lv = llvm_builder->CreateFCmpULT(lv, rv, "cmptmp");
        //         return llvm_builder->CreateUIToFP(lv, llvm::Type::getDoubleTy(*llvm_context), "booltmp");
        //     default:
        //         panic;
        // }
        switch (op) {
            case op::OpType::Add:
                return llvm_builder->CreateAdd(lv, rv, "addtmp");
            case op::OpType::Sub:
                return llvm_builder->CreateSub(lv, rv, "subtmp");
            case op::OpType::Mul:
                return llvm_builder->CreateMul(lv, rv, "multmp");
            case op::OpType::Less:
                return llvm_builder->CreateICmpULT(lv, rv, "cmplttmp");
            case op::OpType::Greater:
                return llvm_builder->CreateICmpUGT(lv, rv, "cmpgttmp");
            case op::OpType::LessEq:
                return llvm_builder->CreateICmpULE(lv, rv, "cmpletmp");
            case op::OpType::GreaterEq:
                return llvm_builder->CreateICmpUGE(lv, rv, "cmpgetmp");
            case op::OpType::Eq:
                return llvm_builder->CreateICmpEQ(lv, rv, "cmpeqtmp");
        }
        panic;
    }
    llvm::Value* codegen_unary() {
        Type* rt = rhs->compute_type();
        llvm::Value* rv = rhs->codegen();
        if (!rv) panic;
        // TODO temp
        assert(rt->ttype == TypeT::UInt || rt->ttype == TypeT::Pure);

        switch (op) {
            case op::OpType::Neg:
                printd("negation");
                return llvm_builder->CreateNeg(rv, "negtmp");
            case op::OpType::LogiNot:
                panic;
                return llvm_builder->CreateNot(rv, "loginottmp"); // TODO check
            case op::OpType::BitNot:
                panic;
                return llvm_builder->CreateNot(rv, "bitnottmp"); // TODO check
        }
        panic;
    }

    llvm::Value* codegen() {
        if(lhs == nullptr) {
            return this->codegen_unary();
        } else {
            return this->codegen_binary();
        }
    }
    // TODO incorrect but unfixable
    // will have to serve as a primitive type checker
    Type* compute_type() {
        if(op::is_logi(op)) return reinterpret_cast<Type*>(type::pool.ask_bool_any()); // TODO if both sides are const and same, return concrete
        // if unary
        if(lhs == nullptr) {
            return rhs->compute_type();
        }
        Type* lt = lhs->compute_type();
        Type* rt = rhs->compute_type();
        return type::meet(lt, rt);
    }
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
    llvm::Value* codegen() {
        if(!named_values.exists(token.val)) panic;
        return named_values[token.val];
    }
    // TODO returns garbage "any" type
    Type* compute_type() {
        if(!named_values.exists(token.val)) panic;
        return type::pool.ask( Type { .tinfo=TypeI::Top, .ttype=TypeT::Pure } );
    }
    void debug_print() {
        std::cout << "(var " << token.val << ")";
    }
};

// Callee name stored in the `token`
struct NodeCall : Node {
    Vec<Node*> args;

    llvm::Value* codegen() { panic; }
    Type* compute_type() { panic; }
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

    llvm::Value* codegen() {
        // llvm::Type::getDoubleTy
        // TODO change this to actual datatypes; assuming all 64 bit integers
        std::vector<llvm::Type*> type_arr(args.size, llvm::Type::getInt64Ty(*llvm_context));
        llvm::FunctionType *ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(*llvm_context), type_arr, false);

        llvm::Function *fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, str::to_cppstr(name.val), llvm_module);

        // Set names for all arguments.
        u32 i = 0;
        for (auto &arg : fn->args())
            arg.setName(str::to_cppstr(args[i++].a.val));

        return fn;
    }
    Type* compute_type() { panic; }
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

    llvm::Value* codegen() {
        llvm::Function* fn = llvm_module->getFunction(str::to_cppstr(dynamic_cast<NodeFnProto*>(proto)->name.val));

        if (!fn) fn = reinterpret_cast<llvm::Function*>(proto->codegen());
        if (!fn) panic;
        if (!fn->empty()) { printd("redefinition"); panic; }
        
        // Create a new basic block to start insertion into.
        llvm::BasicBlock *bb = llvm::BasicBlock::Create(*llvm_context, "entry", fn);
        llvm_builder->SetInsertPoint(bb);

        // Record the function arguments in the NamedValues map.
        named_values.clear();
        // TODO specify arena
        for (auto& arg : fn->args()) named_values.add(str::clone_cstr(std::string(arg.getName()).data()), &arg);

        if (llvm::Value* ret = body->codegen()) {
            // Finish off the function.
            llvm_builder->CreateRet(ret);

            // Validate the generated code, checking for consistency.
            llvm::verifyFunction(*fn);

            // Optimize the function.
            llvm_FPM.run(*fn, llvm_FAM);

            return fn;
        }
        // Error reading body, remove function.
        fn->eraseFromParent();
        panic;
    }
    Type* compute_type() { panic; }
    void debug_print() {
        std::cout << "function {" << std::endl;
        proto->debug_print(); std::cout << std::endl;
        body->debug_print(); std::cout << std::endl;
        std::cout << "} function" << std::endl;
    }
};

struct NodeBlock : Node {
    Vec<Node*> list;

    // TODO rework; this is temporary garbage
    llvm::Value* codegen() {
        return list[0]->codegen();
    }
    Type* compute_type() { panic; }
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
    Type* compute_type() { panic; }
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
    Type* compute_type() { panic; }
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
    Type* compute_type() { panic; }
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
    Type* compute_type() { panic; }
    void debug_print() {
        std::cout << "return {" << std::endl;
        val->debug_print(); std::cout << std::endl;
        std::cout << "} return" << std::endl;
    }
};