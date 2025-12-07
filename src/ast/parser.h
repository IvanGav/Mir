#pragma once

#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/vec.h"

#include "../token/tokenizer.h"
#include "../token/util.h"

#include "../lang/type.h"

#include "node.h"

struct Parser {
    Tokenizer t;
    mem::Arena* node_arena;

    static Parser create(Str src, mem::Arena* node_arena) {
        return Parser { .t = Tokenizer::create(src), .node_arena = node_arena };
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
    // note: operators (special symbols) will be returned by themselves, without `lhs` or `rhs` assigned or read
    // 
    // `nullptr` means that source has been fully parsed
    Node* next_node() {
        Token token = t.next_token();

        switch(token.tt) {
            case TokenType::IntLiteral: {
                NodeConst* n = node_arena->alloc<NodeConst>(1);
                new (n) NodeConst;
                n->token = token;
                u64 int_val = str::to_int<u64>(token.val);
                n->val = reinterpret_cast<Type*>(type::pool.ask_uint_const(int_val));
                return n;
            }

            case TokenType::FloatLiteral: {
                // TODO
                panic;
            }

            case TokenType::StringLiteral: {
                NodeConst* n = node_arena->alloc<NodeConst>(1);
                new (n) NodeConst;
                n->token = token;
                n->val = reinterpret_cast<Type*>(type::pool.ask_str(TypeStr { 
                    .self = Type { .tinfo = TypeI::Known, .ttype = TypeT::Str },
                    .val = Str { .data = token.val.data+1, .size = token.val.size-2 } // we are guaranteed the token to be `"..."` where `...` is our desired string value
                }));
                return n;
            }

            // Read variable name or function call (including all args and both parentheses)
            case TokenType::Identifier: {
                // a crude way of telling whether or not this is a variable name or function call
                // only function calls have `(` after the identifier
                if(t.peek_non_white() == '(') {
                    NodeCall* call = node_arena->alloc<NodeCall>(1);
                    new (call) NodeCall;
                    call->token = token;
                    call->args.arena = node_arena;

                    Token next = t.next_token(); // consume `(`
                    while(next.tt != TokenType::RightParenthese) {
                        assert(next.tt == TokenType::Comma || next.tt == TokenType::LeftParenthese); // commas delimit arguments
                        call->args.push(this->next_expr());
                        next = t.next_token(); // get `,` or `)`
                    }
                    return call;
                } else {
                    NodeVar* var = node_arena->alloc<NodeVar>(1);
                    new (var) NodeVar;
                    var->token = token;
                    return var;
                }
            }

            // Operator; assume unary if both exist for a given symbol
            case TokenType::Special: {
                NodeOp* op = node_arena->alloc<NodeOp>(1);
                new (op) NodeOp;
                op->token = token;
                
                op->op = op::from_str(token.val);
                assert(op->op != op::OpType::Undefined);

                // don't read other nodes; the caller of this function must determine the precidence and assign accordingly
                return op;
            }

            case TokenType::If: {
                NodeIf* if_node = node_arena->alloc<NodeIf>(1);
                new (if_node) NodeIf;
                if_node->token = token;
                if_node->condition.arena = node_arena;
                if_node->body.arena = node_arena;

                while(true) {
                    assert(t.peek_non_white() == '(');
                    if_node->condition.push(this->next_node());
                    assert(t.peek_non_white() == '{');
                    if_node->body.push(this->next_node());

                    // at this point either an `else [if]` clause or `;`
                    // that's because if statements have `;` after them
                    // and if part of expression, must have an else clause
                    if(t.peek_non_white() == ';') {
                        return if_node;
                    }

                    // assume we have an else clause
                    Token _else_token = t.next_token();
                    assert(_else_token.tt == TokenType::Else);

                    if(t.peek_non_white() == '{') {
                        if_node->else_clause = this->next_node();
                        return if_node;
                    }

                    // assume we have an `else if` clause
                    Token _if_token = t.next_token();
                    assert(_if_token.tt == TokenType::If);
                }
            }

            case TokenType::While: {
                NodeWhile* while_node = node_arena->alloc<NodeWhile>(1);
                new (while_node) NodeWhile;
                while_node->token = token;

                assert(t.peek_non_white() == '(');
                while_node->condition = this->next_node();
                assert(t.peek_non_white() == '{');
                while_node->body = this->next_node();

                return while_node;
            }

            // TODO rework
            case TokenType::VarDecl: {
                NodeVarDecl* var_decl = node_arena->alloc<NodeVarDecl>(1);
                new (var_decl) NodeVarDecl;
                var_decl->token = token;

                var_decl->name = t.next_token();
                
                Token _colon_token = t.next_token();
                assert(_colon_token.val == ":"_s);
                
                var_decl->declared_type = type::from_str(t.next_type().val);

                Token _assignment_token = t.next_token();
                assert(_assignment_token.val == "="_s);

                var_decl->init = this->next_expr();

                return var_decl;
            }

            // TODO rework to support pure function declarations
            case TokenType::FunctionDecl: {
                NodeFnDef* fn = node_arena->alloc<NodeFnDef>(1);
                new (fn) NodeFnDef;
                fn->token = token;

                fn->proto = this->build_fn_prototype(token);
                assert(t.peek_non_white() == '{');
                fn->body = this->next_node();

                return fn;
            }

            case TokenType::Return: {
                NodeRet* r = node_arena->alloc<NodeRet>(1);
                new (r) NodeRet;
                r->token = token;
                r->val = this->next_expr();
                return r;
            }

            case TokenType::LeftParenthese: {
                Node* expr = this->next_expr();
                Token _right_parenthese = t.next_token();
                assert(_right_parenthese.val == ")"_s);
                return expr;
            }

            case TokenType::LeftBracket: {
                // TODO array literal
                panic;
            }

            case TokenType::LeftCurly: {
                NodeBlock* block = node_arena->alloc<NodeBlock>(1);
                new (block) NodeBlock;
                block->token = token; // dummy token; put in the `{` as its token
                block->list.arena = node_arena;

                // a crude way of going until reaching the next `}`
                // TODO if `nullptr`, exit
                while(t.peek_non_white() != '}') {
                    Node* expr = this->next_expr();
                    block->list.push(expr);
                    this->read_semicol(); // TODO what if `if (true) { 10 } else { 1 };`
                }
                Token _end_brace = t.next_token(); // consume the trailing `}`
                assert(_end_brace.tt == TokenType::RightCurly);
                return block;
            }

            case TokenType::EndOfLine:
                return this->next_term();

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
                panic;
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
        if(t.peek_non_white() == '.') {
            // TODO member access here
            panic;
        } else if(t.peek_non_white() == '[') {
            // TODO index operation here
            panic;
        }
        return node;
    }

    // read next expression (excluding the terminating token)
    // expressions end with:
    // - `)`, `]`, `}` - anything inside of a braces block
    // - `,` - comma separated list has a list of expressions
    // - `;` - it's a statement
    // `nullptr` means that source has been fully parsed
    Node* next_expr() {
        Node* node = this->next_term();
        if(node == nullptr) return nullptr; // eof

        switch(node->token.tt) {
            /*
                Primary expression
            */

            case TokenType::LeftParenthese: // grouping
            case TokenType::LeftCurly: // basic block; TODO make sure it does return a value
            case TokenType::Identifier: // variable access or function call
            case TokenType::Special: // unary operator
            case TokenType::If: // can be an expression aka return a value; TODO make sure it does return a value and has `else` clause
            case TokenType::IntLiteral:
            case TokenType::FloatLiteral:
            case TokenType::StringLiteral: {
                Node* expr = this->build_primary_expr(node);
                return expr;
            }

            /*
                Special language constructs; don't require extra processing as `next_term` will read them in their entirety
                Doesn't include `if` only because it may be a part of a primary expression, while these are standalone
            */

            case TokenType::While:
            case TokenType::VarDecl:
            case TokenType::FunctionDecl:
            case TokenType::Return: {
                return node;
            }
            
            // Array declaration; if index, it would've been read as part of simple term expression
            case TokenType::LeftBracket: {
                panic;
            }

            // empty line or something
            // TODO may be a problem for `for` statements?
            case TokenType::EndOfLine: {
                return this->next_expr();
            }

            // TODO change?
            case TokenType::EndOfFile: {
                return nullptr;
            }

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
                panic;
        }
        unreachable;
    }

    // given some node that is the beginning of a primary expression,
    // parse the entire primary expression with correct operator precidence
    Node* build_primary_expr(Node* start) {
        // at this point, assume that all `TokenType::Special` are operators
        mem::Arena local = mem::Arena::create(2 KB);
        Vec<Node*> op_stack = Vec<Node*>::empty(local);
        Vec<Node*> val_stack = Vec<Node*>::empty(local);

        Node* next = start;

        // parse until find a terminal symbol (in the body)
        while(true) {
            // `next` contains the next value node or a unary op

            // if unary operator, get its operand and apply
            // note that it is possible to have a fully applied op here (`next` has parsed `(1+1)`)
            // can be `!!!!true`, for all we know, so read unary until can't anymore
            {
                Node* cur = next;
                while(cur->token.tt == TokenType::Special && dynamic_cast<NodeOp*>(cur)->rhs == nullptr) {
                    Node* operand = this->next_term();
                    dynamic_cast<NodeOp*>(cur)->rhs = operand;
                    cur = operand; // go on to check whether the newly read node is a value or unary op
                }
            }

            // even if it was a unary op, it was now applied to a value; it's now a value
            val_stack.push(next);
            
            // expect operator or terminal symbol (`)`, `]`, `}`, `;`, `,` or eof)
            if(ch::terminal(t.peek_non_white())) {
                // apply the remaining ops, in order
                while(op_stack.size > 0) {
                    assert(val_stack.size >= 2);
                    Node* apply_op = op_stack.pop();
                    Node* rhs = val_stack.pop();
                    Node* lhs = val_stack.pop();
                    // apply operands to the op
                    dynamic_cast<NodeOp*>(apply_op)->lhs = lhs;
                    dynamic_cast<NodeOp*>(apply_op)->rhs = rhs;
                    // push it back
                    val_stack.push(apply_op);
                }
                assert(val_stack.size == 1);
                return val_stack[0];
            }

            // assume an operator
            Node* op_node = this->next_node();
            assert(op_node->token.tt == TokenType::Special); // is an operator (binary)

            // convert from unary to binary, if needed
            if(dynamic_cast<NodeOp*>(op_node)->op == op::OpType::Neg) {
                dynamic_cast<NodeOp*>(op_node)->op = op::OpType::Sub;
            }

            // apply if conditions are met (a modified version of the shunting yard algorithm)
            // when reading an operator:
            // "If the operator's precedence is lower than that of the operators at the top of the stack 
            // or the precedences are equal and the operator is left associative, 
            // then that operator is popped off the stack and added to the output" 
            // (https://en.wikipedia.org/wiki/Shunting_yard_algorithm)
            // TODO also make sure to account for `a < b < c` being invalid (although maybe handled with types alone?)
            while(op_stack.size > 0 && op::has_precedence(
                dynamic_cast<NodeOp*>(op_stack.back())->op,
                dynamic_cast<NodeOp*>(op_node)->op
            )) {
                assert(val_stack.size >= 2);
                Node* apply_op = op_stack.pop();
                Node* rhs = val_stack.pop();
                Node* lhs = val_stack.pop();
                // apply operands to the op
                dynamic_cast<NodeOp*>(apply_op)->lhs = lhs;
                dynamic_cast<NodeOp*>(apply_op)->rhs = rhs;
                // push it back
                val_stack.push(apply_op);
            }

            // push the new operator onto the stack
            op_stack.push(op_node);

            // read the next term, presumably not an op
            next = this->next_term();
        }
        unreachable;
    }

    // `fn` has been read, start reading the function prototype
    Node* build_fn_prototype(Token fn_token) {
        NodeFnProto* proto = node_arena->alloc<NodeFnProto>(1);
        new (proto) NodeFnProto;
        proto->token = fn_token;
        proto->args.arena = node_arena;

        proto->name = t.next_token();
        assert(proto->name.tt == TokenType::Identifier);

        Token next = t.next_token(); // consume `(`
        assert(next.tt == TokenType::LeftParenthese);
        while(next.tt != TokenType::RightParenthese) {
            assert(next.tt == TokenType::Comma || next.tt == TokenType::LeftParenthese); // commas delimit arguments
            P<Token,Token> p;
            p.a = t.next_token();
            assert(p.a.tt == TokenType::Identifier);
            Token _colon = t.next_token();
            assert(_colon.val == ":"_s);
            p.b = t.next_type();
            proto->args.push(p);
            next = t.next_token(); // get `,` or `)`
        }
        Token _returning = t.next_token();
        assert(_returning.val == ":"_s);
        proto->ret_type = t.next_type();
        return proto;
    }

    void read_semicol() {
        Token _semicol = t.next_token();
        assert(_semicol.tt == TokenType::EndOfLine);
    }
};