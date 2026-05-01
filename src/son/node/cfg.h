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

    bool is_load(Node* n) {
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

    // just about the worst solution, but hey, I don't have time and patience for this.
    // in my mind, this project is already a failure that needs to be redesigned from ground up.
    // i've got a lot of ideas, just 2 months too little time
    // but for now, when a node is "a load", this will return the info about it
    // i honestly now see the appeal of the very OOP approach that Simple uses
    // although i think there should be a way to do it that's not as OOP and still good
    // anyway, for now I'll be doing this garbage
    Node* mem_of_load(Node* n) {
        switch(n->nt) {
            case NodeType::Load: {
                NodeLoad* node = (NodeLoad*) n;
                return node->mem();
            }

            case NodeType::x86Load: {
                todo;
            }
            
            case NodeType::x86AddM:
            case NodeType::x86SubM:
            case NodeType::x86MulM:
            case NodeType::x86DivM:
            case NodeType::x86CmpM: {
                x86NodeOpM* node = (x86NodeOpM*) n;
                return node->mem();
            }

            case NodeType::x86Pop: {
                todo;
            }
            case NodeType::x86Lea: {
                todo;
            }

            default: panic;
        }
    }

    u32 mem_alias_of_load(Node* n) {
        switch(n->nt) {
            case NodeType::Load: {
                NodeLoad* node = (NodeLoad*) n;
                return node->mem_alias;
            }

            case NodeType::x86Load: {
                todo;
            }
            
            case NodeType::x86AddM:
            case NodeType::x86SubM:
            case NodeType::x86MulM:
            case NodeType::x86DivM:
            case NodeType::x86CmpM: {
                x86NodeOpM* node = (x86NodeOpM*) n;
                todo;
            }

            case NodeType::x86Pop: {
                todo;
            }
            case NodeType::x86Lea: {
                todo;
            }

            default: panic;
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