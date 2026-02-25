#pragma once

#include "prelude.h"

#include "../token/tokenizer.h"
#include "../lang/util.h"

#include "type.h"
#include "node.h"

struct Parser {
    Tokenizer t;
    Str error;

    static Parser create(Str src) {
        return Parser { .t = Tokenizer::create(src), .error = PARSER_NO_ERROR };
    }

    bool done() {
        return t.eof();
    }

    bool err() {
        return error != PARSER_NO_ERROR;
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
                return NodeConst::create(type::pool.int_const(str::to_int<u64>(token.val)), START_NODE, token);
            }

            // Unary operator; read the next term (operand)
            case TokenType::Special: {
                Node* operand_expr = this->next_term();
                if(operand_expr == nullptr) return nullptr;
                return NodeUnOp::create(op::unary(token.val), operand_expr, token);
            }

            case TokenType::LeftParenthese: {
                Node* expr = this->next_primary_expr();
                if(expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::RightParenthese)) {
                    error = "expected ')' after reading an expression that starts with '('"_s;
                    return nullptr;
                }
                return expr;
            }

            
            // Read variable name or function call (including all args and both parentheses)
            case TokenType::Identifier: {
                // only function calls have `(` after the identifier
                if(t.peek_non_white() == '(') {
                    todo;
                } else {
                    Node* value = SCOPE_NODE->find(token.val);
                    if(value == nullptr) {
                        Str errlist[3] = { "variable "_s, token.val, " is not defined"_s};
                        // error = str::from_slice_of_str(ref(Slice<Str>::from_ptr(errlist, 3)));
                        error = str::concat(errlist, 3);
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

            case TokenType::If: {
                todo;
                return this->next_if(token);
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
                // error = str::from_slice_of_str(ref(Slice<Str>::from_ptr(errlist, 2)));
                error = str::concat(errlist, 2);
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
    // does consume the tailing `;`
    Node* next_top_level_expr() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::Return: {
                Node* ret_expr = this->next_primary_expr();
                if(ret_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return NodeRet::create(SCOPE_NODE->ctrl(), ret_expr, token);
            }
            
            case TokenType::VarDecl: {
                Token var_name = t.next_token();
                if(!this->read_token(":"_s)) { error = "Expected type when declaring a variable"_s; return nullptr; }
                Token declared_type = t.next_type(); // TODO
                if(!this->read_token("="_s)) { error = "Variable declaration without initialization"_s; return nullptr; }
                Node* initializer_expr = this->next_primary_expr();
                if(initializer_expr == nullptr) return nullptr;
                SCOPE_NODE->define(var_name.val, initializer_expr); // Defining var here
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return initializer_expr;
            }
            
            // Variable assignment
            case TokenType::Identifier: {
                if(!this->read_token("="_s)) { error = "Top level expression starting with a identifier has to be assignment"_s; return nullptr; }
                Node* new_expr = this->next_primary_expr();
                if(new_expr == nullptr) return nullptr;
                SCOPE_NODE->update(token.val, new_expr); // Updating var here
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return new_expr;
            }

            case TokenType::LeftCurly: {
                Node* block_expr = this->next_block_expr();
                if(block_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return block_expr;
            }

            case TokenType::If: {
                Node* if_expr = this->next_if(token);
                if(if_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return if_expr;
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
        Vec<Token> op_stack = Vec<Token>::create(local);
        Vec<Node*> val_stack = Vec<Node*>::create(local);

        // parse until find a terminal symbol (in the body)
        while(true) {
            // read the next operand
            val_stack.push(ref(this->next_term()));
            if(val_stack.back() == nullptr) return nullptr;

            // expect operator or terminal symbol (`)`, `]`, `}`, `;`, `,` or eof)
            if(ch::terminal(t.peek_non_white())) {
                // apply the remaining ops, in order
                while(op_stack.size > 0) {
                    assert(val_stack.size >= 2);
                    Token apply_op = op_stack.pop();
                    Node* rhs = val_stack.pop();
                    Node* lhs = val_stack.pop();
                    assert(rhs != nullptr);
                    assert(lhs != nullptr);
                    // apply operands to the op
                    Node* applied = NodeBinOp::from_token(lhs, rhs, apply_op);
                    // push it back
                    val_stack.push(applied);
                }
                assert(val_stack.size == 1);
                return val_stack[0];
            }

            // assume an operator
            Token op_node = this->read_binop();
            if(this->err()) { return nullptr; }

            // Assignment is special
            if(op::from_str(op_node.val) == Op::Assignment) {
                printe("cannot parse = in build_primary_expr", "bad syntax");
                panic;
            }

            // apply if conditions are met (a modified version of the shunting yard algorithm)
            while(op_stack.size > 0 && op::has_precedence(
                op::from_str(op_stack.back().val),
                op::from_str(op_node.val)
            )) {
                assert(val_stack.size >= 2);
                Token apply_op = op_stack.pop();
                Node* rhs = val_stack.pop();
                Node* lhs = val_stack.pop();
                assert(rhs != nullptr);
                assert(lhs != nullptr);
                // apply operands to the op
                Node* applied = NodeBinOp::from_token(lhs, rhs, apply_op);
                // push it back
                val_stack.push(applied);
            }

            // push the new operator onto the stack
            op_stack.push(op_node);
        }
    }

    // Assume that the leading `{` has already been read
    // return the last expr value
    Node* next_block_expr() {
        SCOPE_NODE->push();
        Node* expr = nullptr;
        while(t.peek_non_white() != '}') {
            expr = this->next_top_level_expr();
            if(expr == nullptr) return nullptr;
        }
        SCOPE_NODE->pop();
        if(expr == nullptr) { error = "Empty block is not allowed"_s; return nullptr; }
        if(!this->read_token(TokenType::RightCurly)) { error = "Expected }"_s; return nullptr; }
        // return NodeConst::create(type::pool.bottom(), START_NODE);
        return expr;
    }

    Node* next_if(Token token) {
        if(!this->read_token(TokenType::LeftParenthese)) { error = "expected '(' after 'if'"_s; return nullptr; }
        Node* condition = this->next_primary_expr();
        if(!this->read_token(TokenType::RightParenthese)) { error = "condition has to end with ')'"_s; return nullptr; }

        Node* if_node = NodeIf::create(SCOPE_NODE->ctrl(), condition, token);

        // Set up projection nodes

        Node* proj_true = NodeProj::create(0, if_node);
        Node* proj_false = NodeProj::create(1, if_node);
        // In if true branch, the ifT proj node becomes the ctrl
        // But first clone the scope and set it as current
        NodeScope* scope_false = SCOPE_NODE->duplicate();

        // Parse the true side

        SCOPE_NODE->update(CTRL_STR, proj_true);
        if(!this->read_token(TokenType::LeftCurly)) { error = "expected '{' after 'if' condition"_s; return nullptr; }
        Node* true_branch = this->next_block_expr();
        if(true_branch == nullptr) return nullptr;
        assert(true_branch->type->tinfo == TypeI::Bottom);
        NodeScope* scope_true = SCOPE_NODE;

        // Parse the false side

        SCOPE_NODE = scope_false;
        SCOPE_NODE->update(CTRL_STR, proj_false);
        if(t.peek_non_white() != ';') {
            // there's an else clause
            if(!this->read_token(TokenType::Else)) { error = "expected 'else' clause"_s; return nullptr; }
            if(!this->read_token(TokenType::LeftCurly)) { error = "expected '{' after 'else'"_s; return nullptr; }
            Node* false_branch = this->next_block_expr();
            if(false_branch == nullptr) return nullptr;
            assert(false_branch->type->tinfo == TypeI::Bottom);
            scope_false = SCOPE_NODE;
        }

        assert(scope_true->self.input.size == scope_false->self.input.size);

        // Merge results
        // SCOPE_NODE->update(CTRL_STR, scope_true->merge(scope_false)); // TODO erm, it's already getting updated in `NodeScope::merge`
        scope_true->merge(scope_false);
        SCOPE_NODE = scope_true;

        return SCOPE_NODE->ctrl();
    }

    Token read_binop() {
        Token token = t.next_binary_op();
        if(token.tt == TokenType::Special) {
            return token;
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