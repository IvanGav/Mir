#pragma once

#include "node.h"
#include "compute.h"
#include "idealize.h"

namespace node {
    Node* peephole(Node* n) {
        assert(n != nullptr);
        n->type = node::compute(n); // compute n's best known type

        switch(n->nt) {
            case NodeType::Scope:
            case NodeType::Start:
            case NodeType::Ret:
                return n;
            
            case NodeType::Const:
                return n;
            
            case NodeType::Proj: // TODO make sure that's right
            case NodeType::If: // TODO make sure that's right
            case NodeType::Region: // TODO make sure that's right
            case NodeType::Phi: // TODO make sure that's right
            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Mul:
            case NodeType::Div:
            case NodeType::Mod:
            case NodeType::Neg: {
                if(type::constant(n->type)) {
                    Node* replacement = NodeConst::create(n->type, START_NODE);
                    n->kill();
                    return replacement;
                }
                // I have up to 3 nodes and all but 1 need to die
                // n     <- original  ->    this
                // ideal <- idealized ->    n
                // best  <- peepholed ->    m (inside of deadCodeElim)
                Node* ideal = node::idealize(n);
                if(ideal != nullptr) {
                    Node* best = node::peephole(ideal); // TODO WHAT'S THE POINT OF ***NOT*** PEEPHOLING INSIDE OF IDEALIZE WTH???
                    if(n != best && n->unused()) {
                        // TODO do i need to best.keep() and best.unkeep()???
                        n->kill();
                    }
                    return best;
                }
                return n;
            }
            
            case NodeType::Undefined:
                printe("call peephole on undefined node", n);
                panic;
        }
        unreachable;
    }
}