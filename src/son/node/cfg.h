#pragma once

#include "node.h"

namespace node {
    bool cfg(Node* n) {
        assert(n != nullptr);
        switch(n->nt) {
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::CtrlProj:
                return true;
            
            default: return false;
        }
        unreachable;
    }

    // get the loop depth of `n`
    u32 loop_depth(Node* n) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::CtrlProj:
            case NodeType::Region:
                todo;

            default: panic;
        }
        unreachable;
    }
}