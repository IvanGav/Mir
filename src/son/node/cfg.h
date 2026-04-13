#pragma once

#include "node.h"

namespace node {
    bool cfg(Node* n) {
        assert(n != nullptr);
        switch(n->nt) {
            case NodeType::Start:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::CtrlProj:
                return true;
            
            case NodeType::Scope:
            case NodeType::Proj:
            case NodeType::Const:
            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Mul:
            case NodeType::Div:
            case NodeType::Mod:
            case NodeType::Eq:
            case NodeType::Neq:
            case NodeType::Less:
            case NodeType::Greater:
            case NodeType::LessEq:
            case NodeType::GreaterEq:
            case NodeType::Neg:
            case NodeType::Phi:
                return false;
            
            case NodeType::Load:
            case NodeType::Store:
            case NodeType::AllocA:
                return false;
            
            case NodeType::Undefined:
                printe("call cfg on undefined node", n);
                panic;
        }
        unreachable;
    }

    // get the loop depth of `n`
    u32 loop_depth(Node* n) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::CtrlProj:
            case NodeType::Region:
                todo;

            default: panic;
        }
        unreachable;
    }

    // get depth of `n` in the dominator tree
    // `dom_depth(NodeStart) = 1`
    // does not cache, so may be linear to the size of the program
    u32 dom_depth(Node* n) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start: return 1;

            case NodeType::Ret: {
                NodeRet* node = (NodeRet*)n;
                return 1 + node::dom_depth(node->ctrl()); // only has 1 ctrl input; must be the dominator
            }

            case NodeType::If: {
                NodeIf* node = (NodeIf*)n;
                return 1 + node::dom_depth(node->ctrl()); // only has 1 ctrl input; must be the dominator
            }
            
            case NodeType::CtrlProj: {
                NodeProj* node = (NodeProj*)n;
                return 1 + node::dom_depth(node->ctrl()); // only has 1 ctrl input; must be the dominator
            }
            
            case NodeType::Region: {
                NodeRegion* node = (NodeRegion*)n;
                assert(!node->is_incomplete());
                u32 high_depth = node::dom_depth(node->ctrl(0));
                for(u32 i = 1; i < node->ctrl_size(); i++) {
                    high_depth = max(high_depth, node::dom_depth(node->ctrl(1)));
                }
                return high_depth;
            }
            default: panic;
        }
        unreachable;
    }

    Node* idom(Node* n1, Node* n2); // forward def

    // get immidiate dominator of `n`
    // return nullptr if called on `NodeStart`
    // does not cache, so may be linear to the size of the program
    Node* idom(Node* n) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start: return nullptr;
            
            case NodeType::Ret: {
                NodeRet* node = (NodeRet*)n;
                return node->ctrl(); // only has 1 ctrl input; must be the dominator
            }

            case NodeType::If: {
                NodeIf* node = (NodeIf*)n;
                return node->ctrl(); // only has 1 ctrl input; must be the dominator
            }
            
            case NodeType::CtrlProj: {
                NodeProj* node = (NodeProj*)n;
                return node->ctrl(); // only has 1 ctrl input; must be the dominator
            }
            
            case NodeType::Region: {
                NodeRegion* node = (NodeRegion*)n;
                assert(!node->is_incomplete());
                Node* common_idom = node->ctrl(0);
                for(u32 i = 1; i < node->ctrl_size(); i++) {
                    common_idom = node::idom(common_idom, node->ctrl(1));
                }
                return common_idom;
            }
            
            default: panic;
        }
        unreachable;
    }

    // get first immidiate dominator of `n1` and `n2`
    // does not cache, so may be linear to the size of the program
    Node* idom(Node* n1, Node* n2) {
        // the algorithm is to go up the idom tree until we meet 
        assert(node::cfg(n1)); assert(node::cfg(n2));
        while (n1 != n2) {
            todo;
            while (n1 < n2) // not literal pointer comparison
                n1 = node::idom(n1);
            while (n1 > n2)  // not literal pointer comparison
                n2 = node::idom(n2);
        }
        return n1;
    }
}