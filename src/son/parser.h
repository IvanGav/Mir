#pragma once

#include "prelude.h"

#include "../token/tokenizer.h"
#include "../lang/util.h"

#include "type.h"
#include "node.h"

#define nonull(expr) { if((expr) == nullptr) return nullptr; }
// use __TOKEN__ for value of read token
#define read_token_or_err(expect_token, err_msg) { if(!this->read_token(expect_token)) { error = err_msg; return nullptr; }}

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
            this->read_token(TokenType::LeftBracket);
            Node* index = this->next_primary_expr(); 
            if(index == nullptr) return nullptr;
            if(!this->read_token(TokenType::RightBracket)) { error = "Expected ]"_s; return nullptr; }
            Node* mem = SCOPE_NODE->find("$1"_s); // TODO hardcoded
            Node* offset = NodeBinOp::create(Op::Mul, index, NodeConst::create(8)); // TODO hardcoded
            Node* load_node = NodeLoad::create(1, mem, node, offset); // TODO hardcoded
            return load_node;
        }
        return node;
    }

    Node* next_symbol() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::IntLiteral: {
                return NodeConst::create(type::pool.int_const(str::to_int<u64>(token.val)));
            }

            // Unary operator; read the next term (operand)
            case TokenType::Special: {
                Node* operand_expr = this->next_term();
                if(operand_expr == nullptr) return nullptr;
                return NodeUnOp::create(op::unary(token.val), operand_expr);
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
                error = str::concat(errlist, 2);
                return nullptr;
            }
        }
        unreachable;
    }

    // Top level expression can be one of:
    // - `return <expr>;`
    // - `let <name>: <type> = <expr>;`
    // - `<name> = <expr>;`
    // - `{ ... };`
    // - `if(...) {...} ... ;`
    // - `while(...) {...};`
    // - `break;` when inside of a loop
    // - `continue;` when inside of a loop
    // `nullptr` means that source has been fully parsed or an error occurred
    // does consume the tailing `;`
    Node* next_top_level_expr() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::Return: {
                Node* ret_expr = this->next_primary_expr();
                if(ret_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                Node* node_ret = NodeRet::create(SCOPE_NODE->ctrl(), ret_expr);
                STOP_NODE->push_input(node_ret); // register the return with the stop node
                return node_ret;
            }
            
            case TokenType::VarDecl: {
                Token var_name = t.next_token();
                if(!this->read_token(":"_s)) { error = "Expected type when declaring a variable"_s; return nullptr; }
                Type* declared_type = this->next_type();
                if(declared_type == nullptr) return nullptr;
                Node* initializer_expr;
                if(t.peek_non_white() == '=') {
                    this->read_token("="_s); // will succeed
                    initializer_expr = this->next_primary_expr();
                    if(initializer_expr == nullptr) return nullptr;
                } else {
                    initializer_expr = NodeConst::create(type::default_val(declared_type));
                }
                SCOPE_NODE->define(var_name.val, initializer_expr); // Defining var here
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return initializer_expr;
            }
            
            // Variable assignment
            case TokenType::Identifier: {
                if(t.peek_non_white() == '=') {
                    this->read_token("="_s);
                    Node* new_expr = this->next_primary_expr();
                    if(new_expr == nullptr) return nullptr;
                    SCOPE_NODE->update(token.val, new_expr); // Updating var here
                    if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                    return new_expr;
                } else if(t.peek_non_white() == '[') {
                    this->read_token(TokenType::LeftBracket);
                    Node* index = this->next_primary_expr(); 
                    if(index == nullptr) return nullptr;
                    if(!this->read_token(TokenType::RightBracket)) { error = "Expected ]"_s; return nullptr; }
                    if(!this->read_token("="_s)) {
                        error = "Top level expression starting with a identifier has to be assignment"_s;
                        return nullptr;
                    }
                    Node* expr = this->next_primary_expr();
                    if(expr == nullptr) return nullptr;
                    expr->keep();
                    if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                    Node* mem = SCOPE_NODE->find("$1"_s); // TODO hardcoded
                    Node* ptr = SCOPE_NODE->find(token.val);
                    Node* offset = NodeBinOp::create(Op::Mul, index, NodeConst::create(8)); // TODO hardcoded
                    expr->unkeep();
                    Node* store_node = NodeStore::create(1, mem, ptr, offset, expr); // TODO hardcoded
                    SCOPE_NODE->update("$1"_s, store_node); // TODO hardcoded
                    return expr;
                }
                error = "Top level expression starting with a identifier has to be assignment"_s;
                return nullptr;
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

            case TokenType::While: {
                Node* while_expr = this->next_while(token);
                if(while_expr == nullptr) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return while_expr;
            }

            case TokenType::Break: {
                this->apply_break();
                if(this->err()) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return (Node*) BREAK_SCOPE_NODE;
            }

            case TokenType::Continue: {
                this->apply_continue();
                if(this->err()) return nullptr;
                if(!this->read_token(TokenType::EndOfLine)) { error = "Expected ;"_s; return nullptr; }
                return (Node*) CONTINUE_SCOPE_NODE;
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
        mem::Arena local = mem::Arena::create(32 KB);
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
    // Consume the trailing `}`
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

    // Assume that `if` has already been read and accept it as argument `token`
    Node* next_if(Token token) {
        if(!this->read_token(TokenType::LeftParenthese)) { error = "expected '(' after 'if'"_s; return nullptr; }
        Node* condition = this->next_primary_expr();
        if(condition == nullptr) return nullptr;
        if(!this->read_token(TokenType::RightParenthese)) { error = "condition has to end with ')'"_s; return nullptr; }

        Node* if_node = NodeIf::create(SCOPE_NODE->ctrl(), condition);

        // Set up projection nodes

        Node* proj_true = NodeProj::create(0, if_node, true);
        Node* proj_false = NodeProj::create(1, if_node, true);
        // In if true branch, the ifT proj node becomes the ctrl
        // But first clone the scope and set it as current
        NodeScope* scope_false = SCOPE_NODE->duplicate();

        // Parse the true side

        SCOPE_NODE->update_ctrl(proj_true);
        if(!this->read_token(TokenType::LeftCurly)) { error = "expected '{' after 'if' condition"_s; return nullptr; }
        Node* true_branch = this->next_block_expr();
        if(true_branch == nullptr) return nullptr;
        // assert(true_branch->type->tinfo == TypeI::Bottom);
        NodeScope* scope_true = SCOPE_NODE;

        // Parse the false side

        SCOPE_NODE = scope_false;
        SCOPE_NODE->update_ctrl(proj_false);
        if(t.peek_non_white() != ';') {
            // there's an else clause
            if(!this->read_token(TokenType::Else)) { error = "expected 'else' clause"_s; return nullptr; }
            if(!this->read_token(TokenType::LeftCurly)) { error = "expected '{' after 'if'"_s; return nullptr; }
            Node* false_branch = this->next_block_expr();
            if(false_branch == nullptr) return nullptr;
            scope_false = SCOPE_NODE;
        }

        // assert(scope_true->self.input.size == scope_false->self.input.size); // TODO

        // Merge results
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

    // assume that `while` has been read and passed as argument `while_token`
    Node* next_while(Token while_token) {
        NodeScope* save_break_scope = BREAK_SCOPE_NODE;
        NodeScope* save_continue_scope = CONTINUE_SCOPE_NODE;

        // note that loop_node->input[1] is nullptr until the loop is fully parsed
        SCOPE_NODE->update_ctrl(NodeRegion::create_incomplete(SCOPE_NODE->ctrl()));
        
        // Save the current scope as the loop head; will be the sentinel for the body loops
        NodeScope* head = SCOPE_NODE;
        head->self.keep();
        // Make SCOPE_NODE the loop body scope
        SCOPE_NODE = head->duplicate_with_sentinel();

        if(!this->read_token(TokenType::LeftParenthese)) { error = "Expected '(' after 'while'"_s; return nullptr; }
        Node* condition = this->next_primary_expr();
        if(condition == nullptr) { return nullptr; }
        if(!this->read_token(TokenType::RightParenthese)) { error = "Expected ')' after 'while' condition"_s; return nullptr; }
        Node* loop_cond_node = NodeIf::create(SCOPE_NODE->ctrl(), condition);
        loop_cond_node->keep();
        Node* proj_t = NodeProj::create(0, loop_cond_node, true);
        loop_cond_node->unkeep();
        Node* proj_f = NodeProj::create(1, loop_cond_node, true);

        // Break scope has the false projection -> when while condition is false
        // By default has same variables as before entering the loop body -> just duplicate the current scope
        BREAK_SCOPE_NODE = SCOPE_NODE->duplicate();
        BREAK_SCOPE_NODE->update_ctrl(proj_f);
        CONTINUE_SCOPE_NODE = nullptr;

        // Parse the true side = loop body
        SCOPE_NODE->update_ctrl(proj_t);
        if(!this->read_token(TokenType::LeftCurly)) { error = "Expected a block as 'while' body"_s; return nullptr; }
        Node* block_ret = this->next_block_expr();
        if(block_ret == nullptr) return nullptr;

        // Merge the loop bottom into other continue statements
        if (CONTINUE_SCOPE_NODE != nullptr) {
            SCOPE_NODE->merge(CONTINUE_SCOPE_NODE); // TODO should be sufficient, right?
            CONTINUE_SCOPE_NODE = nullptr; // no references to dead nodes
        }
        assert(SCOPE_NODE->self.input.size != 0);

        // The true branch loops back, so whatever is current _scope.ctrl gets
        // added to head loop as input.  endLoop() updates the head scope, and
        // goes through all the phis that were created earlier.  For each phi,
        // it sets the second input to the corresponding input from the back
        // edge.  If the phi is redundant, it is replaced by its sole input.
        NodeScope* exit_scope = BREAK_SCOPE_NODE;

        // connect the back edge
        assert(!head->self.is_dead());
        head->end_loop(SCOPE_NODE, exit_scope);
        assert(!head->self.is_dead());
        head->self.unkeep();
        head->self.kill();

        // restore
        CONTINUE_SCOPE_NODE = save_continue_scope;
        BREAK_SCOPE_NODE = save_break_scope;

        // At exit the false control is the current control, and
        // the scope is the exit scope after the exit test.
        SCOPE_NODE = exit_scope;
        return (Node*) SCOPE_NODE;
    }

    Type* next_type() {
        Token base_type_t = t.next_type(); // will not include tailing '*' and '[num]'
        if(base_type_t.val != "i64"_s) { error = "The only supported primitive type is i64"_s; return nullptr; }
        Type* base_type = type::pool.int_sized(8);
        if(t.peek_non_white() == '*') { error = "Pointers not supported yet"_s; return nullptr; }
        if(t.peek_non_white() == '[') {
            t.at++;
            Str arr_size_str = t.parse_number_literal();
            i64 arr_size = str::to_int<i64>(arr_size_str);
            if(t.peek_non_white() != ']') { error = "Expected ] after number in the type"_s; return nullptr; }
            t.at++;
            return type::pool.ptr_to(base_type, arr_size);
        }
        return base_type;
    }

    // SCOPE_NODE becomes xctrl
    void apply_break() {
        if(!this->is_loop_active()) {
            error = "Cannot 'break' without an active loop"_s;
            return;
        }
        BREAK_SCOPE_NODE->merge(SCOPE_NODE);
        SCOPE_NODE = NodeScope::create_xctrl();
    }
    // SCOPE_NODE becomes xctrl
    void apply_continue() {
        if(!this->is_loop_active()) {
            error = "Cannot 'continue' without an active loop"_s;
            return;
        }
        if(CONTINUE_SCOPE_NODE == nullptr) {
            CONTINUE_SCOPE_NODE = SCOPE_NODE;
        } else {
            CONTINUE_SCOPE_NODE->merge(SCOPE_NODE);
            SCOPE_NODE = NodeScope::create_xctrl();
        }
    }

    bool is_loop_active() {
        return BREAK_SCOPE_NODE != nullptr;
    }
};