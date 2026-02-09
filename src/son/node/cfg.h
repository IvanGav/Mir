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
            case NodeType::Neg:
            case NodeType::Phi:
                return false;
            
            case NodeType::Proj:
                return ((NodeProj*)n)->index == 0; // TODO make sure there is no better way of doing this
            
            case NodeType::Undefined:
                printe("call cfg on undefined node", n);
                panic;
        }
        unreachable;
    }
}