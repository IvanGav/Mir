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
    bool error;

    static Parser create(Str src) {
        return Parser { .t = Tokenizer::create(src), .error = false };
    }

    bool done() {
        return t.eof();
    }

    // get next node, lazily
    // that means, read only as much as it makes sense to
    // do not call this function without context, it expects to see only certain token types
    // 
    // example: if reading `1 + 2`, `next_node` will only read `1`, because `1` by itself makes sense
    // example: if reading `myfn(10, -5, 1 + 2) + 10`, the entire call `myfn(10, -5, 1 + 2)` will be read, as it doesn't make sense to separate function name from arguments
    // note: cannot read binary operators. Use `read_binop` instead
    // 
    // `nullptr` means that source has been fully parsed or error was encountered
    Node* next_node() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::IntLiteral: {
                return node::peephole((Node*)
                    NodeConst::from_token((Type*) type::pool.int_const(str::to_int<u64>(token.val)), token)
                    .create(START_NODE)
                );
            }

            // Unary operator; read the next term (operand)
            case TokenType::Special: {
                Node* operand_expr = this->next_term();
                return node::peephole((Node*)
                    NodeUnOp::from_token(token, op::unary(token.val))
                    .create(operand_expr)
                );
            }

            case TokenType::Return: {
                Node* ret_expr = this->next_expr();
                return node::peephole((Node*)
                    NodeRet::from_token(token)
                    .create(START_NODE, ret_expr)
                );
            }

            case TokenType::LeftParenthese: {
                Node* expr = this->next_expr();
                if(!this->read_token(TokenType::RightParenthese)) {
                    error = true;
                    return nullptr;
                }
                return expr;
            }
            
            case TokenType::VarDecl: {
                Token var_name = t.next_token();
                if(!this->read_token(":"_s)) { error = true; return nullptr; }
                Token declared_type = t.next_type();
                if(!this->read_token("="_s)) { error = true; return nullptr; }
                Node* initializer_expr = this->next_expr();
                SCOPE_NODE->define(var_name.val, initializer_expr);
                return initializer_expr;
            }

            
            // Read variable name or function call (including all args and both parentheses)
            case TokenType::Identifier: {
                todo;
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
                    todo;
                    // NodeVar* var = node_arena->alloc<NodeVar>(1);
                    // new (var) NodeVar;
                    // var->token = token;
                    // return var;
                }
            }

            case TokenType::FloatLiteral: {
                todo;
            }

            case TokenType::StringLiteral: {
                todo;
                // NodeConst* n = node_arena->alloc<NodeConst>(1);
                // new (n) NodeConst;
                // n->token = token;
                // n->val = reinterpret_cast<Type*>(type::pool.ask_str(TypeStr { 
                //     .self = Type { .tinfo = TypeI::Known, .ttype = TypeT::Str },
                //     .val = Str { .data = token.val.data+1, .size = token.val.size-2 } // we are guaranteed the token to be `"..."` where `...` is our desired string value
                // }));
                // return n;
            }

            case TokenType::If: {
                todo;
                // NodeIf* if_node = node_arena->alloc<NodeIf>(1);
                // new (if_node) NodeIf;
                // if_node->token = token;
                // if_node->condition.arena = node_arena;
                // if_node->body.arena = node_arena;

                // while(true) {
                //     assert(t.peek_non_white() == '(');
                //     if_node->condition.push(this->next_node());
                //     assert(t.peek_non_white() == '{');
                //     if_node->body.push(this->next_node());

                //     // at this point either an `else [if]` clause or `;`
                //     // that's because if statements have `;` after them
                //     // and if part of expression, must have an else clause
                //     if(t.peek_non_white() == ';') {
                //         return if_node;
                //     }

                //     // assume we have an else clause
                //     Token _else_token = t.next_token();
                //     assert(_else_token.tt == TokenType::Else);

                //     if(t.peek_non_white() == '{') {
                //         if_node->else_clause = this->next_node();
                //         return if_node;
                //     }

                //     // assume we have an `else if` clause
                //     Token _if_token = t.next_token();
                //     assert(_if_token.tt == TokenType::If);
                // }
            }

            case TokenType::While: {
                todo;
                // NodeWhile* while_node = node_arena->alloc<NodeWhile>(1);
                // new (while_node) NodeWhile;
                // while_node->token = token;

                // assert(t.peek_non_white() == '(');
                // while_node->condition = this->next_node();
                // assert(t.peek_non_white() == '{');
                // while_node->body = this->next_node();

                // return while_node;
            }

            case TokenType::FunctionDecl: {
                todo;
                // NodeFnDef* fn = node_arena->alloc<NodeFnDef>(1);
                // new (fn) NodeFnDef;
                // fn->token = token;

                // fn->proto = this->build_fn_prototype(token);
                // assert(t.peek_non_white() == '{');
                // fn->body = this->next_node();

                // return fn;
            }

            case TokenType::LeftBracket: {
                todo; // array literal
            }

            case TokenType::LeftCurly: {
                todo;
                // NodeBlock* block = node_arena->alloc<NodeBlock>(1);
                // new (block) NodeBlock;
                // block->token = token; // dummy token; put in the `{` as its token
                // block->list.arena = node_arena;

                // // a crude way of going until reaching the next `}`
                // // TODO if `nullptr`, exit
                // while(t.peek_non_white() != '}') {
                //     Node* expr = this->next_expr();
                //     block->list.push(expr);
                //     this->read_semicol(); // TODO what if `if (true) { 10 } else { 1 };`
                // }
                // Token _end_brace = t.next_token(); // consume the trailing `}`
                // assert(_end_brace.tt == TokenType::RightCurly);
                // return block;
            }

            case TokenType::EndOfLine:
                return this->next_term(); // TODO rework?

            case TokenType::EndOfFile:
                return nullptr;

            /*
                Unexpected syntax
            */

            case TokenType::RightParenthese:
            case TokenType::RightBracket:
            case TokenType::RightCurly:
            case TokenType::Undefined:
            case TokenType::DataType:
            case TokenType::Comma:
            case TokenType::Else:
                error = true;
                return nullptr;
        }
        unreachable;
    }

    // read the next "term"
    // in this context, I mean either:
    // - `(...)` group, `{...}` group, var, if..., literal (int, float, string, array)
    // - one of the above along with index operation (aka `arr[...]`)
    // - one of the above along with member access (aka `obj.val` or `obj.fun(...)`)
    // - any combination of the above until they end
    // `nullptr` means that source has been fully parsed
    Node* next_term() {
        Node* node = this->next_node();
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

    // read next expression (excluding the terminating token)
    // expressions end with:
    // - `)`, `]`, `}` - anything inside of a braces block
    // - `,` - comma separated list has a list of expressions
    // - `;` - it's a statement
    // `nullptr` means that source has been fully parsed or an error occurred
    Node* next_expr() {
        Node* node = this->next_term();
        if(node == nullptr) return nullptr; // eof or error

        switch(node->nt) {
            case NodeType::Neg:
            case NodeType::Const:
                return this->build_primary_expr(node);
                
            case NodeType::Ret:
                return node;

            // Expression cannot start with a BinOp
            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Mul:
            case NodeType::Div:
            case NodeType::Mod:
            // Not associated with any syntax
            case NodeType::Start:
                error = true;
                return nullptr;
        }
        unreachable;
    }

    NodeBinOp read_binop() {
        Token token = t.next_binary_op();
        if(token.tt == TokenType::Special) {
            NodeBinOp n = NodeBinOp::from_token(token, op::binary(token.val));
            return n;
        }
        printe("expected a binary op", token);
        panic;
    }

    // given some node that is the beginning of a primary expression,
    // parse the entire primary expression with correct operator precidence
    // Note that since the starting node is given, the tokenizer should be at a binary operator or terminal symbol
    Node* build_primary_expr(Node* start) {
        mem::Arena local = mem::Arena::create(2 KB);
        Vec<NodeBinOp> op_stack = Vec<NodeBinOp>::create(local);
        Vec<Node*> val_stack = Vec<Node*>::create(local);
        val_stack.push(start);

        // parse until find a terminal symbol (in the body)
        while(true) {
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
            // TODO error check

            // apply if conditions are met (a modified version of the shunting yard algorithm)
            // when reading an operator:
            // "If the operator's precedence is lower than that of the operators at the top of the stack 
            // or the precedences are equal and the operator is left associative, 
            // then that operator is popped off the stack and added to the output" 
            // (https://en.wikipedia.org/wiki/Shunting_yard_algorithm)
            // TODO also make sure to account for `a < b < c` being invalid (although maybe handled with types alone?)
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

            // read the next operand
            val_stack.push(this->next_term());
        }
        unreachable;
    }

    // `fn` has been read, start reading the function prototype
    Node* build_fn_prototype(Token fn_token) {
        todo;
        // NodeFnProto* proto = node_arena->alloc<NodeFnProto>(1);
        // new (proto) NodeFnProto;
        // proto->token = fn_token;
        // proto->args.arena = node_arena;

        // proto->name = t.next_token();
        // assert(proto->name.tt == TokenType::Identifier);

        // Token next = t.next_token(); // consume `(`
        // assert(next.tt == TokenType::LeftParenthese);
        // while(next.tt != TokenType::RightParenthese) {
        //     assert(next.tt == TokenType::Comma || next.tt == TokenType::LeftParenthese); // commas delimit arguments
        //     P<Token,Token> p;
        //     p.a = t.next_token();
        //     assert(p.a.tt == TokenType::Identifier);
        //     Token _colon = t.next_token();
        //     assert(_colon.val == ":"_s);
        //     p.b = t.next_type();
        //     proto->args.push(p);
        //     next = t.next_token(); // get `,` or `)`
        // }
        // Token _returning = t.next_token();
        // assert(_returning.val == ":"_s);
        // proto->ret_type = t.next_type();
        // return proto;
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