#pragma once

#include "node.h"

namespace node {
    bool pinned(Node* n) {
        assert(n != nullptr);
        switch(n->nt) {
            // ctrl defines what to "pin" to
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::CtrlProj:
                return true;
            
            // cannot be moved freely
            case NodeType::Proj:
            case NodeType::Load:
            case NodeType::Store:
            case NodeType::AllocA:
            case NodeType::Phi:
                return true;

            // can move freely
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