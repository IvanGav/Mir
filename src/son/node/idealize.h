#pragma once

#include "node.h"

namespace node {
    Node* idealize_add(NodeBinOp* node);
    Node* idealize_sub(NodeBinOp* node);
    Node* idealize_mul(NodeBinOp* node);
    Node* idealize_div(NodeBinOp* node);
    Node* idealize_mod(NodeBinOp* node);

    // in NodeBinOp, we want constants on the right and anything else on the left
    bool should_swap(Node* left, Node* right) {
        if(right->nt == NodeType::Const) return false;
        if(left->nt == NodeType::Const) return true;
        return left->uid > right->uid;
    }

    // Nullable; When nullptr is returned, no progress/change has been made
    Node* replace_with_const(Node* n) {
        if(n->cfg()) return nullptr;
        if(n->nt == NodeType::Const) return nullptr; // already a const
        if(type::constant(n->type)) {
            return NodeConst::create(n->type);
        } else {
            return nullptr;
        }
    }

    // Nullable; When nullptr is returned, no progress/change has been made
    Node* idealize(Node* n) {
        Node* const_replace = node::replace_with_const(n);
        if(const_replace != nullptr) return const_replace;
        
        switch(n->nt) {
            case NodeType::Scope: // shouldn't even be idealized
            case NodeType::Const: // nothing to do
                return nullptr;
            
            // all of these are cfg - for now don't do anything with them
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::Loop:
            case NodeType::CtrlProj:
                return nullptr;

            case NodeType::Proj:
                return nullptr;

            case NodeType::Phi: {
                NodePhi* node = (NodePhi*) n;
                if(node->is_incomplete() || 
                    (node->region()->nt == NodeType::Region && ((NodeRegion*)node->region())->is_incomplete())
                ) { return nullptr; } // self of region is incomplete; don't optimize yet

                Node* maybe_single = node->single_unique_input();
                if(maybe_single != nullptr) { return maybe_single; }

                // Pull "down" a common data op. One less op in the world. One more Phi, but Phis do not make code.
                // `Phi(op(A,B),op(Q,R),op(X,Y))` becomes `op(Phi(A,Q,X), Phi(B,R,Y))`
                if(node->data(0)->nt == NodeType::BinOp && node->all_same()) {
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
            
            case NodeType::BinOp: {
                NodeBinOp* node = (NodeBinOp*) n;
                assert(!(type::constant(node->lhs()->type) && type::constant(node->rhs()->type))); // node::peephole would've replaced with constant node

                switch(node->op) {
                    case Op::Add: return idealize_add(node);
                    case Op::Sub: return idealize_sub(node);
                    case Op::Mul: return idealize_mul(node);
                    case Op::Div: return idealize_div(node);
                    case Op::Mod: return idealize_mod(node);
                    case Op::Less:
                    case Op::LessEq:
                    case Op::Greater:
                    case Op::GreaterEq:
                    case Op::Eq:
                    case Op::Neq:
                        return nullptr; // TODO Unimplemented yet

                    default: panic; // catch any non-handled cases
                }
            }

            case NodeType::UnOp: {
                // TODO
                return nullptr;
            }

            case NodeType::Load:
            case NodeType::Store:
            case NodeType::AllocA:
                return nullptr; // TODO
            
            case NodeType::Undefined:
                printe("call idealize on undefined node", n);
                panic;
        }
        unreachable;
    }

    Node* idealize_add(NodeBinOp* node) {
        Node* lhs = node->lhs();
        Node* rhs = node->rhs();
        Op lop = lhs->nt == NodeType::BinOp ? ((NodeBinOp*)lhs)->op : Op::Undefined;
        Op rop = rhs->nt == NodeType::BinOp ? ((NodeBinOp*)rhs)->op : Op::Undefined;
        // Add of 0. If (0+x), will be canonicalized to (x+0).
        if(rhs->type == type::pool.con(0)) return lhs;

        // Add of same is a multiply by 2
        if(lhs == rhs) {
            Node* multiplier = NodeConst::create(2);
            return NodeBinOp::create(Op::Mul, lhs, multiplier);
        }

        // Move ops such that: adds are on the left, consts are on the right

        // Move non-adds to RHS
        if(lop != Op::Add && rop == Op::Add) {
            node->swap_lhs_rhs();
            return (Node*)node;
        }

        // Note: for the following notation (add add non) since they've been rotated, it's assumed to be ((add add) non) which is implicitly ((add + add) + non)
        // Now we might see (add add non) or (add non non) or (add add add) but never (add non add)

        // Do we have  x + (y + z) ?
        // Swap to    (x + y) + z
        // Rotate (add add add) to remove the add on RHS
        if(rop == Op::Add) {
            NodeBinOp* rhs_add = (NodeBinOp*) rhs;
            Node* new_lhs = NodeBinOp::create(Op::Add, lhs, rhs_add->lhs());
            return NodeBinOp::create(Op::Add, new_lhs, rhs_add->rhs());
        }

        // Now we might see (add add non) or (add non non) but never (add non add) nor (add add add)
        if(lop != Op::Add) {
            if(should_swap(node->lhs(), node->rhs())) {
                node->swap_lhs_rhs();
                return (Node*)node;
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

    Node* idealize_sub(NodeBinOp* node) {
        Node* lhs = node->lhs();
        Node* rhs = node->rhs();
        // Subtract 0 identity
        if(rhs->type == type::pool.con(0)) return lhs;

        // Negation identity
        if(lhs->type == type::pool.con(0)) return NodeUnOp::create(Op::Neg, rhs);

        return nullptr;
    }

    Node* idealize_mul(NodeBinOp* node) {
        Node* lhs = node->lhs();
        Node* rhs = node->rhs();
        Op lop = lhs->nt == NodeType::BinOp ? ((NodeBinOp*)lhs)->op : Op::Undefined;
        // Op rop = rhs->nt == NodeType::BinOp ? ((NodeBinOp*)rhs)->op : Op::Undefined;
        // Multiply by 1 identity
        if(rhs->type == type::pool.con(1)) return lhs;

        // Canonicalize to constants being on the right
        if(lhs->nt == NodeType::Const && rhs->nt != NodeType::Const) {
            node->swap_lhs_rhs();
            return (Node*)node;
        }

        // convert (arg * 2) * 3 into arg * 6
        if(lop == Op::Mul && (
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

    Node* idealize_div(NodeBinOp* node) {
        Node* lhs = node->lhs();
        Node* rhs = node->rhs();
        // Divide by 1 identity
        if(rhs->type == type::pool.con(1)) return lhs;

        return nullptr;
    }

    Node* idealize_mod(NodeBinOp* node) {
        // Node* lhs = node->lhs();
        Node* rhs = node->rhs();
        // Modulo 1 identity
        if(rhs->type == type::pool.con(1)) return NodeConst::create((i64)0); // c++, 0 is not a pointer, it's a number you dum dum

        return nullptr;
    }
}