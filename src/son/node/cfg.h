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
            
            case NodeType::x86Jump: // equivalent to `if`
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

            case NodeType::x86Jump: // equivalent to `if`
                return false;
            
            default: unreachable;
        }
        unreachable;
    }

    bool is_ideal(Node* n) {
        todo;
        switch(n->nt) {
            default: return false;
        }
    }

    bool is_store(Node* n) {
        switch(n->nt) {
            case NodeType::Load: return true;

            case NodeType::x86Load:
            case NodeType::x86AddM:
            case NodeType::x86SubM:
            case NodeType::x86MulM:
            case NodeType::x86DivM:
            case NodeType::x86CmpM:
            case NodeType::x86Pop:
            case NodeType::x86Lea:
                return true;

            default: return false;
        }
    }

    Op op(Node* n) {
        switch(n->nt) {
            case NodeType::BinOp: {
                NodeBinOp* node = (NodeBinOp*) n;
                return node->op;
            }
            case NodeType::UnOp: {
                NodeUnOp* node = (NodeUnOp*) n;
                return node->op;
            }
            default: panic;
        }
    }

}