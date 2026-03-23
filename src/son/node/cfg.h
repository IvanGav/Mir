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
                return true;
            
            case NodeType::Scope:
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
            
            case NodeType::Proj: {
                NodeProj* node = (NodeProj*)n;
                return node->index == 0 || node->ctrl()->nt == NodeType::If; // TODO make sure there is no better way of doing this
            }
            
            case NodeType::Undefined:
                printe("call cfg on undefined node", n);
                panic;
        }
        unreachable;
    }
}