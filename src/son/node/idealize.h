#pragma once

#include "node.h"

bool should_swap(Node* left, Node* right) {
    if(right->nt == NodeType::Const) return false;
    if(left->nt == NodeType::Const) return true;
    return left->uid > right->uid;
}

namespace node {
    // Nullable; When nullptr is returned, no progress/change has been made
    Node* replace_with_const(Node* n) {
        switch(n->nt) {
            case NodeType::Const:
            case NodeType::Start:
            // case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::Scope:
                return nullptr;
            
            default:
                break;
        }

        if(type::constant(n->type)) {
            return NodeConst::create(n->type, START_NODE);
        } else {
            return nullptr;
        }
    }

    // Nullable; When nullptr is returned, no progress/change has been made
    Node* idealize(Node* n) {
        Node* const_replace = node::replace_with_const(n);
        if(const_replace != nullptr) return const_replace;
        
        switch(n->nt) {
            case NodeType::Scope:
            case NodeType::Start:
            case NodeType::If:
            case NodeType::Ret:
            case NodeType::Proj:
            case NodeType::Const:
            return nullptr;

            case NodeType::Region: {
                return nullptr;
            }


            case NodeType::Phi: {
                NodePhi* node = (NodePhi*) n;
                if(node->incomplete() || 
                    (node->region()->nt == NodeType::Region && ((NodeRegion*)node->region())->incomplete())
                ) { return nullptr; } // self of region is incomplete; don't optimize yet

                Node* maybe_single = node->single_unique_input();
                if(maybe_single != nullptr) { return maybe_single; }

                // Pull "down" a common data op. One less op in the world. One more Phi, but Phis do not make code.
                // `Phi(op(A,B),op(Q,R),op(X,Y))` becomes `op(Phi(A,Q,X), Phi(B,R,Y))`
                if(node->data(0)->is_binop() && node->all_same()) {
                    mem::Arena arena = mem::Arena::create(5 MB);
                    Vec<Node*> lhs_data = Vec<Node*>::create(arena);
                    Vec<Node*> rhs_data = Vec<Node*>::create(arena);
                    for(u32 i = 0; i < node->data_size(); i++) {
                        lhs_data.push(ref(((NodeBinOp*)node->data(i))->lhs()));
                        rhs_data.push(ref(((NodeBinOp*)node->data(i))->rhs()));
                    }
                    Node* phi_lhs = NodePhi::create(node->debug_var_name, node->region(), lhs_data.full_slice());
                    Node* phi_rhs = NodePhi::create(node->debug_var_name, node->region(), rhs_data.full_slice());
                    return NodeBinOp::create(((NodeBinOp*) node->data(0))->op, phi_lhs, phi_rhs);
                }

                return nullptr;
            }
            
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
                    Node* multiplier = NodeConst::create((Type*) type::pool.int_const(2), START_NODE);
                    return NodeBinOp::create(Op::Mul, lhs, multiplier);
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
                    Node* new_lhs = NodeBinOp::create(Op::Add, lhs, rhs_add->lhs());
                    return NodeBinOp::create(Op::Add, new_lhs, rhs_add->rhs());
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
                    Node* new_rhs = NodeBinOp::create(Op::Add, lhs_add->rhs(), node->rhs());
                    return NodeBinOp::create(Op::Add, new_lhs, new_rhs);
                }

                // Now we sort along the spline via rotates, to gather similar things together.

                // Do we rotate (x + y) + z
                // into         (x + z) + y ?
                if(should_swap(lhs_add->rhs(), node->rhs())) {
                    Node* new_lhs = NodeBinOp::create(Op::Add, lhs_add->lhs(), node->rhs());
                    Node* new_rhs = lhs_add->rhs();
                    return NodeBinOp::create(Op::Add, new_lhs, new_rhs);
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
                if(lt == (Type*) type::pool.int_const(0)) return NodeUnOp::create(Op::Neg, rhs);

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

                // TODO maybe not? Overflow issues?
                // convert (arg * 2) * 3 into arg * 6
                if(lhs->nt == NodeType::Mul && (
                    rhs->nt == NodeType::Const &&
                    ((NodeBinOp*)lhs)->rhs()->nt == NodeType::Const
                )) {
                    Node* i1 = rhs;
                    Node* i2 = ((NodeBinOp*)lhs)->rhs();
                    Node* new_lhs = ((NodeBinOp*)lhs)->lhs();
                    Node* mul = NodeBinOp::create(Op::Mul, i1, i2);
                    Node* self = NodeBinOp::create(Op::Mul, new_lhs, mul);
                    return self;
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
                if(rt == (Type*) type::pool.int_const(1)) return NodeConst::create((Type*) type::pool.int_const(0), START_NODE);

                return nullptr;
            }

            case NodeType::Eq:
            case NodeType::Neq:
            case NodeType::Less:
            case NodeType::Greater:
            case NodeType::LessEq:
            case NodeType::GreaterEq:
                return nullptr;

            case NodeType::Neg:
                return nullptr;
            
            case NodeType::Undefined:
                printe("call idealize on undefined node", n);
                panic;
        }
        unreachable;
    }
}