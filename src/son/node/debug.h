#pragma once

#include "../../core/set.h"
#include "node.h"

std::ostream& operator<<(std::ostream& os, NodeType nt) {
    switch (nt) {
        case NodeType::Undefined:   return os << "Undefined";
        case NodeType::Scope:       return os << "Scope";
        case NodeType::Start:       return os << "Start";
        case NodeType::Ret:         return os << "Ret";
        case NodeType::Proj:        return os << "Proj";
        case NodeType::Const:       return os << "Const";
        case NodeType::Add:         return os << "Add";
        case NodeType::Sub:         return os << "Sub";
        case NodeType::Mul:         return os << "Mul";
        case NodeType::Div:         return os << "Div";
        case NodeType::Mod:         return os << "Mod";
        case NodeType::Neg:         return os << "Neg";
    }
    unreachable;
}

std::ostream& operator<<(std::ostream& os, Node* n) {
    assert(n != nullptr);
    os << n->uid << ": " << n->nt << "(" << n->token << ")\n";
    switch(n->nt) {
        case NodeType::Start: {
            return os;
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

        case NodeType::Region: {
            NodeRegion* node = (NodeRegion*) n;
            for(u32 i = 0; i < node->self.input.size; i++) {
                os << "\tctrl" << i << " = " << node->ctrl(i)->uid << "\n";
            }
            return os;
        }

        case NodeType::Phi: {
            NodePhi* node = (NodePhi*) n;
            os << "\tregion = " << node->region()->uid << "\n";
            for(u32 i = 0; i < node->self.input.size; i++) {
                os << "\tdata" << i << " = " << node->data(i)->uid << "\n";
            }
            return os;
        }

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

        case NodeType::Add:
        case NodeType::Sub:
        case NodeType::Mul:
        case NodeType::Div:
        case NodeType::Mod: {
            NodeBinOp* node = (NodeBinOp*) n;
            os << "\tlhs = " << node->lhs()->uid << "\n";
            os << "\trhs = " << node->rhs()->uid << "\n";
            return os;
        }
        
        case NodeType::Neg: {
            NodeUnOp* node = (NodeUnOp*) n;
            os << "\trhs = " << node->rhs()->uid << "\n";
            return os;
        }
        
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