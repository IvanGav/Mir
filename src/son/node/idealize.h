#pragma once

#include "node.h"

bool should_swap(Node* left, Node* right) {
    if(right->nt == NodeType::Const) return false;
    if(left->nt == NodeType::Const) return true;
    return left->uid > right->uid;
}

namespace node {
    Node* peephole(Node* n); // FORWARD DECL
    // Nullable; When nullptr is returned, no progress/change has been made
    Node* idealize(Node* n) {
        switch(n->nt) {
            case NodeType::Scope:
            case NodeType::Start:
            case NodeType::Ret:
            case NodeType::Proj:
            case NodeType::Const:
                return nullptr;
            
            case NodeType::Add: {
                NodeBinOp* node = (NodeBinOp*) n;
                Node* lhs = node->lhs();
                Node* rhs = node->rhs();
                Type* lt = lhs->type;
                Type* rt = rhs->type;
                assert(!(type::constant(lt) && type::constant(rt))); // node::peephole would've replaced with constant node

                // Add of 0. If (0+x), will be canonicalized to (x+0).
                if(rt == (Type*) type::pool.int_const(0)) return lhs;

                // Add of same is a multiply by 2
                if(lhs == rhs) {
                    Node* multiplier = node::peephole((Node*) NodeConst::generated((Type*) type::pool.int_const(2)).create(START_NODE));
                    return (Node*) NodeBinOp::generated(Op::Mul).create(lhs, multiplier);
                }

                // Move ops such that: adds are on the left, consts are on the right

                // Move non-adds to RHS
                if(lhs->nt != NodeType::Add && rhs->nt == NodeType::Add) {
                    node->swap_lhs_rhs();
                    return n;
                }

                // Note: for the following notation (add add non) since they've been rotated, it's assumed to be ((add add) non) which is implicitly ((add + add) + non)
                // Now we might see (add add non) or (add non non) or (add add add) but never (add non add)

                // Do we have  x + (y + z) ?
                // Swap to    (x + y) + z
                // Rotate (add add add) to remove the add on RHS
                if(rhs->nt == NodeType::Add) {
                    NodeBinOp* rhs_add = (NodeBinOp*) rhs;
                    Node* new_lhs = node::peephole((Node*) NodeBinOp::generated(Op::Add).create(lhs, rhs_add->lhs()));
                    return (Node*) NodeBinOp::generated(Op::Add).create(new_lhs, rhs_add->rhs());
                }

                // Now we might see (add add non) or (add non non) but never (add non add) nor (add add add)
                if(lhs->nt != NodeType::Add) {
                    if(should_swap(node->lhs(), node->rhs())) {
                        node->swap_lhs_rhs();
                        return n;
                    }
                    return nullptr;
                }

                // Now we only see (add add non)

                // Do we have (x + con1) + con2?
                // Replace with (x + (con1+con2) which then fold the constants
                NodeBinOp* lhs_add = (NodeBinOp*) lhs;
                if(lhs_add->rhs()->nt == NodeType::Const && rhs->nt == NodeType::Const) {
                    Node* new_lhs = lhs_add->lhs();
                    Node* new_rhs = node::peephole((Node*) NodeBinOp::generated(Op::Add).create(lhs_add->rhs(), node->rhs()));
                    return (Node*) NodeBinOp::generated(Op::Add).create(new_lhs, new_rhs);
                }

                // Now we sort along the spline via rotates, to gather similar things together.

                // Do we rotate (x + y) + z
                // into         (x + z) + y ?
                if(should_swap(lhs_add->rhs(), node->rhs())) {
                    Node* new_lhs = node::peephole((Node*) NodeBinOp::generated(Op::Add).create(lhs_add->lhs(), node->rhs()));
                    Node* new_rhs = lhs_add->rhs();
                    return (Node*) NodeBinOp::generated(Op::Add).create(new_lhs, new_rhs);
                }

                return nullptr;
            }

            case NodeType::Sub: {
                NodeBinOp* node = (NodeBinOp*) n;
                Node* lhs = node->lhs();
                Node* rhs = node->rhs();
                Type* lt = lhs->type;
                Type* rt = rhs->type;
                assert(!(type::constant(lt) && type::constant(rt))); // node::peephole would've replaced with constant node

                // Subtract 0 identity
                if(rt == (Type*) type::pool.int_const(0)) return lhs;

                // Negation identity
                if(lt == (Type*) type::pool.int_const(0)) return (Node*) NodeUnOp::generated(Op::Neg).create(rhs);

                return nullptr;
            }

            case NodeType::Mul: {
                NodeBinOp* node = (NodeBinOp*) n;
                Node* lhs = node->lhs();
                Node* rhs = node->rhs();
                Type* lt = lhs->type;
                Type* rt = rhs->type;
                assert(!(type::constant(lt) && type::constant(rt))); // node::peephole would've replaced with constant node

                // Multiply by 1 identity
                if(rt == (Type*) type::pool.int_const(1)) return lhs;

                // Canonicalize to constants being on the right
                if(lhs->nt == NodeType::Const && rhs->nt != NodeType::Const) {
                    node->swap_lhs_rhs();
                    return n;
                }

                return nullptr;
            }

            case NodeType::Div: {
                NodeBinOp* node = (NodeBinOp*) n;
                Node* lhs = node->lhs();
                Node* rhs = node->rhs();
                Type* lt = lhs->type;
                Type* rt = rhs->type;
                assert(!(type::constant(lt) && type::constant(rt))); // node::peephole would've replaced with constant node

                // Divide by 1 identity
                if(rt == (Type*) type::pool.int_const(1)) return lhs;

                return nullptr;
            }
            
            case NodeType::Mod: {
                NodeBinOp* node = (NodeBinOp*) n;
                Node* lhs = node->lhs();
                Node* rhs = node->rhs();
                Type* lt = lhs->type;
                Type* rt = rhs->type;
                assert(!(type::constant(lt) && type::constant(rt))); // node::peephole would've replaced with constant node

                // Modulo 1 identity
                if(rt == (Type*) type::pool.int_const(1)) return (Node*) NodeConst::generated((Type*) type::pool.int_const(0)).create(START_NODE);

                return nullptr;
            }

            case NodeType::Neg:
                return nullptr;
            
            case NodeType::Undefined:
                printe("call idealize on undefined node", n);
                panic;
        }
        unreachable;
    }
}