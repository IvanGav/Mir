#pragma once

#include "node.h"

namespace node {
    bool cfg(CFGNode* n) {
        assert(n != nullptr);
        switch(n->nt) {
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::Loop:
            case NodeType::CtrlProj:
                return true;
            
            default:
                return false;
        }
        unreachable;
    }

    bool is_block_head(CFGNode* n) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start: // begins the program
            case NodeType::CtrlProj: // begins an `if` branch
            case NodeType::Region: // begins the block that merges an `if`
            case NodeType::Loop: // technically opens the loop; really same as `Region`
                return true;

            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
                return false;
            
            default: unreachable;
        }
        unreachable;
    }
}