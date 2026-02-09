#pragma once

#include "node.h"
#include "../type/const.h"

namespace node {
    // TODO why not assign to `n->type` directly?
    Type* compute(Node* n) {
        assert(n != nullptr);
        switch(n->nt) {
            case NodeType::Scope:
            case NodeType::Ret:
                return type::pool.bottom();
            
            case NodeType::If: {
                Type* arr[2] = {type::pool.ctrl(), type::pool.ctrl()};
                Slice<Type*> val = Slice<Type*>::from_ptr(arr, 2);
                return (Type*) type::pool.get_tuple(TypeTuple { .self = Type { .tinfo = TypeI::Known, .ttype = TypeT::Tuple }, .val = val });
            }

            case NodeType::Region: {
                return type::pool.ctrl();
            }
            
            case NodeType::Proj: {
                NodeProj* node = (NodeProj*)(n);
                Type* t = node->ctrl()->type;
                if(t->ttype == TypeT::Tuple) {
                    return ((TypeTuple*)t)->val[node->index];
                } else {
                    panic;
                }
            }

            case NodeType::Phi: {
                return type::pool.bottom(); // TODO why?
            }

            case NodeType::Start: {
                NodeStart* node = (NodeStart*)(n);
                assert(node->args != nullptr);
                return (Type*) node->args;
            }
            
            case NodeType::Const: {
                NodeConst* node = (NodeConst*)(n);
                assert(node->val != nullptr);
                return node->val;
            }

            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Div:
            case NodeType::Mul:
            case NodeType::Mod: {
                NodeBinOp* node = (NodeBinOp*)(n);
                Type* lt = node->lhs()->type; Type* rt = node->rhs()->type;
                assert(lt != nullptr); assert(rt != nullptr);
                if(type::constant(lt) && type::constant(rt)) {
                    if(lt->ttype == TypeT::Int && rt->ttype == TypeT::Int) {
                        i64 left = ((TypeInt*) lt)->val(); i64 right = ((TypeInt*) rt)->val();
                        return (Type*) type::pool.int_const(op::apply(node->op, left, right));
                    } else {
                        todo;
                    }
                } else {
                    return type::meet(lt, rt);
                }
            }

            case NodeType::Neg: {
                NodeUnOp* node = (NodeUnOp*)(n);
                Type* rt = node->rhs()->type;
                if(type::constant(rt)) {
                    if(rt->ttype == TypeT::Int) {
                        i64 right = ((TypeInt*) rt)->val();
                        return (Type*) type::pool.int_const(-right);
                    } else {
                        todo;
                    }
                } else {
                    return rt;
                }
            }
            
            case NodeType::Undefined:
                printe("call compute on undefined node", n);
                panic;
        }
        unreachable;
    }
}