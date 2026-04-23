#pragma once

#include "../../core/set.h"
#include "node.h"

std::ostream& operator<<(std::ostream& os, NodeType nt) {
    switch (nt) {
        case NodeType::Undefined:   return os << "Undefined";
        case NodeType::Scope:       return os << "Scope";
        case NodeType::Start:       return os << "Start";
        case NodeType::Stop:        return os << "Stop";
        case NodeType::Ret:         return os << "Ret";
        case NodeType::Proj:        return os << "Proj";
        case NodeType::CtrlProj:    return os << "CtrlProj";
        case NodeType::If:          return os << "If";
        case NodeType::Region:      return os << "Region";
        case NodeType::Loop:        return os << "Loop";
        case NodeType::Phi:         return os << "Phi";
        case NodeType::Const:       return os << "Const";
        case NodeType::BinOp:       todo;
        case NodeType::UnOp:        todo;
        case NodeType::Load:        return os << "Load";
        case NodeType::Store:       return os << "Store";
        case NodeType::AllocA:      return os << "AllocA";
    }
    unreachable;
}

std::ostream& operator<<(std::ostream& os, Node* n) {
    assert(n != nullptr);
    os << n->uid << ": " << n->nt << "\n";
    switch(n->nt) {
        case NodeType::Start: {
            return os;
        }

        case NodeType::Stop: {
            todo;
        }

        case NodeType::Ret: {
            NodeRet* node = (NodeRet*) n;
            os << "\tctrl = " << node->ctrl()->uid << "\n";
            os << "\texpr = " << node->expr()->uid << "\n";
            return os;
        }

        case NodeType::If: {
            NodeIf* node = (NodeIf*) n;
            os << "\tctrl = " << node->ctrl()->uid << "\n";
            os << "\tcondition = " << node->condition()->uid << "\n";
            return os;
        }

        case NodeType::Loop:
        case NodeType::Region: {
            NodeRegion* node = (NodeRegion*) n;
            for(u32 i = 0; i < node->self.input.size; i++) {
                os << "\tctrl" << i << " = ";
                if(node->ctrl(i) == nullptr) {
                    os << "incomplete" << "\n";
                } else {
                    os << node->ctrl(i)->uid << "\n";
                }
            }
            return os;
        }

        case NodeType::Phi: {
            NodePhi* node = (NodePhi*) n;
            os << "\tregion = " << node->region()->uid << "\n";
            for(u32 i = 0; i < node->data_size(); i++) {
                os << "\tdata" << i << " = ";
                if(node->data(i) == nullptr) {
                    os << "incomplete" << "\n";
                } else {
                    os << node->data(i)->uid << "\n";
                }
            }
            return os;
        }

        case NodeType::CtrlProj:
        case NodeType::Proj: {
            NodeProj* node = (NodeProj*) n;
            os << "\tindex = " << node->index << "\n";
            os << "\tctrl = " << node->ctrl()->uid << "\n";
            return os;
        }
        
        case NodeType::Const: {
            NodeConst* node = (NodeConst*) n;
            os << "\tval = " << node->val << "\n";
            os << "\tctrl = " << node->ctrl()->uid << "\n";
            return os;
        }

        case NodeType::BinOp: {
            NodeBinOp* node = (NodeBinOp*) n;
            os << "\tlhs = " << node->lhs()->uid << "\n";
            os << "\trhs = " << node->rhs()->uid << "\n";
            return os;
        }
        
        case NodeType::UnOp: {
            NodeUnOp* node = (NodeUnOp*) n;
            os << "\trhs = " << node->rhs()->uid << "\n";
            return os;
        }

        case NodeType::Load: {
            NodeLoad* node = (NodeLoad*) n;
            os << "\talias = " << node->mem_alias << "\n";
            os << "\tmem = " << node->mem()->uid << "\n";
            os << "\tptr = " << node->ptr()->uid << "\n";
            os << "\toff = " << node->off()->uid << "\n";
            return os;
        }

        case NodeType::Store: {
            NodeStore* node = (NodeStore*) n;
            os << "\talias = " << node->mem_alias << "\n";
            os << "\tmem = " << node->mem()->uid << "\n";
            os << "\tptr = " << node->ptr()->uid << "\n";
            os << "\toff = " << node->off()->uid << "\n";
            os << "\tval = " << node->val()->uid << "\n";
            return os;
        }

        case NodeType::AllocA:
            todo;
        
        case NodeType::Scope: {
            // NodeScope* node = (NodeScope*) n;
            os << "\tNOTHING TO PRINT\n";
            return os;
        }
        
        case NodeType::Undefined: {
            os << "\tinput.size = " << n->input.size << "\n";
            return os;
        }
    }
    unreachable;
}

namespace node {
    void print_tree(Node* root, bool* printed) {
        if(printed[root->uid] == 1) return;
        printed[root->uid] = 1;
        std::cout << root << std::endl;
        for(usize i = 0; i < root->output.size; i++) {
            node::print_tree(root->output[i], printed);
        }
    }
    void print_tree(Node* root) {
        mem::Arena scratch = mem::Arena::create(1 MB);
        u32 size = Node::uid_counter + 1;
        bool* arr = scratch.alloc<bool>(size);
        std::fill(arr, arr + size, false);
        node::print_tree(root, arr);
    }
}