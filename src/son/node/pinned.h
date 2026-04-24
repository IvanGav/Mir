#pragma once

#include "node.h"

namespace node {
    bool pinned(Node* n) {
        assert(n != nullptr);
        switch(n->nt) {
            // ctrl define "pinned"
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::Loop:
            case NodeType::CtrlProj:
                panic;
            
            case NodeType::Proj: // projects onto a specific ctrl node
            case NodeType::Phi: // merges variables of a speicifc region node
                return true;

            // can move freely
            case NodeType::Load:
            case NodeType::Store:
            case NodeType::AllocA:
            case NodeType::Const:
            case NodeType::BinOp:
            case NodeType::UnOp:
                return false;
            
            case NodeType::Scope:
                printe("call pinned on scope node", n);
                panic;

            case NodeType::Undefined:
                printe("call pinned on undefined node", n);
                panic;
        }
        unreachable;
    }
}