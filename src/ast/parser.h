#pragma once

#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/vec.h"

#include "../token/tokenizer.h"

#include "node.h"

struct Parser {
    Tokenizer t;

    mem::Arena* node_arena;

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
                n->token = token;
                n->val = -1; // TODO
                return n;
            }

            case TokenType::FloatLiteral: {
                NodeConst* n = node_arena->alloc<NodeConst>(1);
                n->token = token;
                // TODO
                panic;
            }

            case TokenType::StringLiteral: {
                // TODO
                panic;
            }

            // Read variable name or function call (including all args and both parentheses)
            case TokenType::Identifier: {
                // a crude way of telling whether or not this is a variable name or function call
                // only function calls have `(` after the identifier
                u8 next_char = t.peek_non_white();
                if(next_char == '(') {
                    NodeCall* call = node_arena->alloc<NodeCall>(1);
                    call->token = token;

                    Token next = t.next_token(); // consume `(`
                    while(next.tt != TokenType::RightParenthese) {
                        // TODO error checking for `eof`, `;`, `]` and the likes of werid nonsensical args; or later maybe
                        if(next.tt == TokenType::Comma) { continue; } // commas delimit arguments
                        call->args.push(this->next_expr());
                        next = t.next_token(); // get `,` or `)`
                    }
                    return call;
                } else {
                    NodeVar* var = node_arena->alloc<NodeVar>(1);
                    var->token = token;
                    return var;
                }
            }

            // Operator; assume unary if both exist for a given symbol
            case TokenType::Special: {
                NodeOp* op = node_arena->alloc<NodeOp>(1);
                op->token = token;
                
                if (token.val == "-"_s) op->op = OpType::Neg;
                else if (token.val == "!"_s) op->op = OpType::LogiNot;
                else if (token.val == "~"_s) op->op = OpType::BitNot;

                else if (token.val == "+"_s) op->op = OpType::Add;
                // else if (token.val == "-"_s) op->op = OpType::Sub;
                else if (token.val == "*"_s) op->op = OpType::Mul;
                else if (token.val == "/"_s) op->op = OpType::Div;
                else if (token.val == "%"_s) op->op = OpType::Mod;
                else if (token.val == "||"_s) op->op = OpType::LogiOr;
                else if (token.val == "&&"_s) op->op = OpType::LogiAnd;
                else if (token.val == "|"_s) op->op = OpType::BitOr;
                else if (token.val == "&"_s) op->op = OpType::BitAnd;
                else if (token.val == "^"_s) op->op = OpType::BitXor;
                else panic;

                // don't read other nodes; the caller of this function must determine the precidence and assign accordingly
                return op;
            }

            case TokenType::If:
            case TokenType::While:
            case TokenType::VarDecl:
            case TokenType::FunctionDecl:
                panic;

            case TokenType::Return: {
                NodeRet* r = node_arena->alloc<NodeRet>(1);
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
                // TODO: array indexing or declaration
                panic;
            }

            case TokenType::LeftCurly: {
                NodeBlock* block = node_arena->alloc<NodeBlock>(1);
                Token next = t.next_token();
                while(next.tt == TokenType::EndOfLine) {
                    Node* expr = this->next_expr();
                    block->list.push(expr);
                    next = t.next_token();
                }
                return block;
            }

            case TokenType::EndOfFile:
                return nullptr;

            /*
                Unexpected syntax
            */

            case TokenType::EndOfLine: // Should not read a node on `;`
            case TokenType::RightParenthese:
            case TokenType::RightBracket:
            case TokenType::RightCurly:
            case TokenType::DataType:
            case TokenType::Comma: 
                panic;
        }
        unreachable;
    }

    // read next expression
    // expressions end with:
    // - `)`, `]`, `}` - anything inside of a braces block
    // - `,` - comma separated list has a list of expressions
    // - `;` - it's a statement
    // `nullptr` means that source has been fully parsed
    Node* next_expr() {
        Node* node = this->next_node();

        switch(node->token.tt) {
            /*
                Primary expression
            */

            case TokenType::LeftParenthese: // grouping
            case TokenType::LeftCurly: // basic block
            case TokenType::Identifier: // variable access or function call
            case TokenType::Special: // unary operator
            case TokenType::If: // can be an expression aka return a value
            case TokenType::IntLiteral:
            case TokenType::FloatLiteral:
            case TokenType::StringLiteral: {
                Node* primary = this->next_node();
                panic;
            }

            /*
                Special language constructs; don't require extra processing as `next_node` will read them in their entirety
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

            case TokenType::EndOfFile: {
                return nullptr;
            }

            /*
                Unexpected syntax
            */

            case TokenType::RightParenthese:
            case TokenType::RightBracket:
            case TokenType::RightCurly:
            case TokenType::DataType:
            case TokenType::Comma: 
                panic;
        }
        unreachable;
    }
};