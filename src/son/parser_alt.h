#pragma once

#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/vec.h"

#include "../token/tokenizer.h"
#include "../lang/util.h"

#include "type.h"
#include "node.h"

struct Parser {
    Tokenizer t;
    Str error;

    static Parser create(Str src) {
        return Parser { .t = Tokenizer::create(src), .error = ""_s };
    }

    bool done() {
        return t.eof();
    }

    bool err() {
        return error != ""_s;
    }

    Node* next_term() {
        Node* node = this->next_symbol();
        if(node == nullptr) return nullptr;
        if(t.peek_non_white() == '.') {
            // TODO member access here
            todo;
        } else if(t.peek_non_white() == '[') {
            // TODO index operation here
            todo;
        }
        return node;
    }

    Node* next_symbol() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::IntLiteral: {
                return node::peephole((Node*)
                    NodeConst::from_token((Type*) type::pool.int_const(str::to_int<u64>(token.val)), token)
                    .create(SCOPE_NODE->ctrl())
                );
            }

            // Unary operator; read the next term (operand)
            case TokenType::Special: {
                Node* operand_expr = this->next_term();
                if(operand_expr == nullptr) return nullptr;
                return node::peephole((Node*)
                    NodeUnOp::from_token(token, op::unary(token.val))
                    .create(operand_expr)
                );
            }

            case TokenType::LeftParenthese: {
                Node* expr = this->next_primary_expr();
                if(expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::RightParenthese)) return nullptr;
                return expr;
            }

            
            // Read variable name or function call (including all args and both parentheses)
            case TokenType::Identifier: {
                // only function calls have `(` after the identifier
                if(t.peek_non_white() == '(') {
                    todo;
                    // NodeCall* call = node_arena->alloc<NodeCall>(1);
                    // new (call) NodeCall;
                    // call->token = token;
                    // call->args.arena = node_arena;
                    // Token next = t.next_token(); // consume `(`
                    // while(next.tt != TokenType::RightParenthese) {
                    //     assert(next.tt == TokenType::Comma || next.tt == TokenType::LeftParenthese); // commas delimit arguments
                    //     call->args.push(this->next_expr());
                    //     next = t.next_token(); // get `,` or `)`
                    // }
                    // return call;
                } else {
                    Node* value = SCOPE_NODE->find(token.val);
                    if(value == nullptr) {
                        Str errlist[3] = { "variable "_s, token.val, " is not defined"_s};
                        error = str::from_slice_of_str(ref(Slice<Str>::from_ptr(errlist, 3)));
                        return nullptr;
                    }
                    return value;
                }
            }

            case TokenType::FloatLiteral: {
                todo;
            }

            case TokenType::LeftBracket: {
                todo; // array literal
            }

            case TokenType::LeftCurly: {
                todo;
            }

            case TokenType::EndOfLine: {
                error = "Expected a symbol, but ; found"_s;
                return nullptr;
            }

            case TokenType::EndOfFile:
                return nullptr;

            // Unexpected syntax
            default: {
                Str errlist[2] = {"unexpected token "_s, to_str(token.tt) };
                error = str::from_slice_of_str(ref(Slice<Str>::from_ptr(errlist, 2)));
                return nullptr;
            }
        }
        unreachable;
    }

    // Top level expression can be one of:
    // `return <expr>;`
    // `let <name>: <type> = <expr>;
    // `<name> = <expr>;
    // `{ ... };`
    // `nullptr` means that source has been fully parsed or an error occurred
    Node* next_top_level_expr() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::Return: {
                Node* ret_expr = this->next_primary_expr();
                if(ret_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return node::peephole((Node*)
                    NodeRet::from_token(token)
                    .create(SCOPE_NODE->ctrl(), ret_expr)
                );
            }
            
            case TokenType::VarDecl: {
                Token var_name = t.next_token();
                if(!this->read_token(":"_s)) { error = "Expected type when declaring a variable"_s; return nullptr; }
                Token declared_type = t.next_type();
                if(!this->read_token("="_s)) { error = "Variable declaration without initialization"_s; return nullptr; }
                Node* initializer_expr = this->next_primary_expr();
                if(initializer_expr == nullptr) return nullptr;
                SCOPE_NODE->define(var_name.val, initializer_expr);
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return initializer_expr;
            }
            
            // Variable assignment
            case TokenType::Identifier: {
                if(!this->read_token("="_s)) { error = "Top level expression starting with a identifier has to be assignment"_s; return nullptr; }
                Node* new_expr = this->next_primary_expr();
                if(new_expr == nullptr) return nullptr;
                SCOPE_NODE->update(token.val, new_expr);
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return new_expr;
            }

            case TokenType::LeftCurly: {
                Node* block_expr = this->next_block_expr();
                if(block_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::RightCurly)) { error = "Expected }"_s; return nullptr; }
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return block_expr;
            }

            // skip empty expressions
            case TokenType::EndOfLine:
                return this->next_top_level_expr();
            case TokenType::EndOfFile:
                return nullptr;
            // Unexpected syntax
            default: {
                Str errlist[2] = {"unexpected token "_s, to_str(token.tt) };
                error = str::from_slice_of_str(ref(Slice<Str>::from_ptr(errlist, 2)));
                return nullptr;
            }
        }
    }

    // parse the entire primary expression with correct operator precidence
    Node* next_primary_expr() {
        mem::Arena local = mem::Arena::create(2 KB);
        Vec<NodeBinOp> op_stack = Vec<NodeBinOp>::create(local);
        Vec<Node*> val_stack = Vec<Node*>::create(local);

        // parse until find a terminal symbol (in the body)
        while(true) {
            // read the next operand
            val_stack.push(this->next_term());
            if(val_stack.back() == nullptr) return nullptr;

            // expect operator or terminal symbol (`)`, `]`, `}`, `;`, `,` or eof)
            if(ch::terminal(t.peek_non_white())) {
                // apply the remaining ops, in order
                while(op_stack.size > 0) {
                    assert(val_stack.size >= 2);
                    NodeBinOp apply_op = op_stack.pop();
                    Node* rhs = val_stack.pop();
                    Node* lhs = val_stack.pop();
                    assert(rhs != nullptr);
                    assert(lhs != nullptr);
                    // apply operands to the op
                    Node* applied = node::peephole((Node*) apply_op.create(lhs, rhs));
                    // push it back
                    val_stack.push(applied);
                }
                assert(val_stack.size == 1);
                return val_stack[0];
            }

            // assume an operator
            NodeBinOp op_node = this->read_binop();
            if(this->err()) { return nullptr; }

            // Assignment is special
            if(op_node.op == Op::Assignment) {
                printe("cannot parse = in build_primary_expr", "bad syntax");
                panic;
            }

            // apply if conditions are met (a modified version of the shunting yard algorithm)
            while(op_stack.size > 0 && op::has_precedence(
                op_stack.back().op,
                op_node.op
            )) {
                assert(val_stack.size >= 2);
                NodeBinOp apply_op = op_stack.pop();
                Node* rhs = val_stack.pop();
                Node* lhs = val_stack.pop();
                assert(rhs != nullptr);
                assert(lhs != nullptr);
                // apply operands to the op
                Node* applied = node::peephole((Node*) apply_op.create(lhs, rhs));
                // push it back
                val_stack.push(applied);
            }

            // push the new operator onto the stack
            op_stack.push(op_node);
        }
    }

    // Assume that the leading `{` has already been read
    // Does not return a value! (returns nullptr)
    Node* next_block_expr() {
        SCOPE_NODE->push();
        Node* expr = nullptr;
        while(t.peek_non_white() != '}') {
            expr = this->next_top_level_expr();
            if(expr == nullptr) return nullptr;
            // if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; } // TODO what if `if (true) { 10 } else { 1 };`
        }
        SCOPE_NODE->pop();
        return node::peephole((Node*) 
            NodeConst::generated((Type*) type::pool.bottom())
            .create(SCOPE_NODE->ctrl())
        );
    }

    NodeBinOp read_binop() {
        Token token = t.next_binary_op();
        if(token.tt == TokenType::Special) {
            NodeBinOp n = NodeBinOp::from_token(token, op::binary(token.val));
            return n;
        }
        Str errlist[2] = { "expected an binary operator, but found "_s, token.val };
        error = str::from_slice_of_str(ref(Slice<Str>::from_ptr(errlist, 2)));
        return {}; // return garbage, since an error occurred
    }

    bool read_token(TokenType tt) {
        Token token = t.next_token();
        return token.tt == tt;
    }
    bool read_token(Str val) {
        Token token = t.next_token();
        return token.val == val;
    }
};