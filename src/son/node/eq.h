#pragma once

#include "node.h"

namespace node {
    bool eq(Node* left, Node* right) {
        assert(left != nullptr && right != nullptr);
        if(left->nt != right->nt) return false; // if not same type, not same
        switch(left->nt) {
            case NodeType::Start:
            case NodeType::Stop:
            case NodeType::Scope:
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::Region:
            case NodeType::Loop:
            case NodeType::Phi:
            case NodeType::Load:
            case NodeType::Store:
            case NodeType::AllocA:
            case NodeType::BinOp:
            case NodeType::UnOp: {
                return left->input == right->input;
            }
            
            case NodeType::CtrlProj:
            case NodeType::Proj: {
                NodeProj* ln = (NodeProj*)(left);
                NodeProj* rn = (NodeProj*)(right);
                return left->input == right->input && ln->index == rn->index;
            }

            case NodeType::Const: {
                NodeConst* ln = (NodeConst*)(left);
                NodeConst* rn = (NodeConst*)(right);
                return ln->val == rn->val;
            }
            
            case NodeType::Undefined:
                printe("call eq on undefined nodes", left);
                panic;
        }
        unreachable;
    }
}