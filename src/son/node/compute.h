#pragma once

#include "node.h"
#include "../type/const.h"

namespace node {
    // TODO why not assign to `n->type` directly?
    Type* compute(Node* n) {
        assert(n != nullptr);
        switch(n->nt) {
            case NodeType::Scope:
            case NodeType::Stop:
            case NodeType::Ret:
                return type::pool.bottom;

            case NodeType::Start: {
                NodeStart* node = (NodeStart*)(n);
                assert(node->args != nullptr);
                return (Type*) node->args;
            }
            
            case NodeType::If: {
                Type* arr[2] = {type::pool.ctrl, type::pool.ctrl};
                Slice<Type*> val = Slice<Type*>::from_ptr(arr, 2);
                return (Type*) type::pool.get_tuple(TypeTuple { .self = Type { .tinfo = TypeI::Known, .ttype = TypeT::Tuple }, .val = val });
            }

            case NodeType::Region: {
                return type::pool.ctrl; // TODO
            }
            
            case NodeType::CtrlProj:
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
                NodePhi* node = (NodePhi*)(n);
                if(node->region()->nt != NodeType::Region || ((NodeRegion*) (node->region()))->is_incomplete()) {
                    // return node::glb(node->data(0)->type);
                    Type* t = node->data(0)->type;
                    if(t->ttype == TypeT::Mem) {
                        TypePtr* memtype = (TypePtr*) type::pool.get_bottom(t->ttype);
                        memtype->ptr = type::pool.get_bottom(((TypePtr*) t)->ptr->ttype);
                    }
                    return type::pool.get_bottom(t->ttype);
                }
                Type* t = type::pool.top;
                for(u32 i = 0; i < node->data_size(); i++)
                    t = type::meet(t, node->data(i)->type);
                assert(t != type::pool.top); // should refine to something more specific
                return t;
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
            case NodeType::Mod:
            case NodeType::Eq:
            case NodeType::Neq:
            case NodeType::Less:
            case NodeType::Greater:
            case NodeType::LessEq:
            case NodeType::GreaterEq: {
                NodeBinOp* node = (NodeBinOp*)(n);
                Type* lt = node->lhs()->type; Type* rt = node->rhs()->type;
                assert(lt != nullptr); assert(rt != nullptr);
                if(lt->ttype != rt->ttype) return type::pool.bottom;
                if(lt->tinfo == TypeI::Bottom || rt->tinfo == TypeI::Top) return lt;
                if(lt->tinfo == TypeI::Top || rt->tinfo == TypeI::Bottom) return rt;
                if(lt->ttype == TypeT::Int) {
                    TypeInt* lti = (TypeInt*) lt; TypeInt* rti = (TypeInt*) rt;
                    return type::pool.int_range(
                        op::apply(node->op, lti->val_min, rti->val_min),
                        op::apply(node->op, lti->val_max, rti->val_max)
                    );
                } else {
                    printd(lt->ttype);
                    printd(lt->tinfo);
                    printd(rt->ttype);
                    printd(lt->tinfo);
                    todo;
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

            case NodeType::Load: {
                // if know what memory was stored last, 
                NodeLoad* node = (NodeLoad*)(n);
                TypePtr* mem = (TypePtr*) node->mem()->type; // unsafe without type check before use
                if(mem->self.ttype == TypeT::Mem && node->decl_type != mem->ptr) {
                    // assert(mem->mem_alias == node->mem_alias);
                    // return type::meet(node->decl_type, mem->ptr); // TODO should be join?
                    return mem->ptr;
                }
                assert(node->decl_type != nullptr);
                return node->decl_type;
            }

            case NodeType::Store: {
                NodeStore* node = (NodeStore*)(n);
                Type* val = node->val()->type;
                TypePtr* mem = (TypePtr*) node->mem()->type; // Invariant; unsafe unless checked
                assert(mem->self.ttype == TypeT::Mem);
                // TODO do a alias check here mem->alias == node->alias
                Type* t = type::meet(val, mem->ptr);
                return type::pool.mem(t);
            }

            case NodeType::AllocA: {
                NodeAllocA* node = (NodeAllocA*)(n);
                Type* types[3];
                types[0] = type::pool.ctrl;
                types[1] = node->ptr;
                types[2] = type::pool.mem(node->mem()->type);
                return type::pool.from_slice(Slice<Type*>::from_ptr(types, 3));
            }
            
            case NodeType::Undefined:
                printe("call compute on undefined node", n);
                panic;
        }
        unreachable;
    }
}