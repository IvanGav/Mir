#pragma once

#include "../../core/prelude.h"
#include "../../son/node.h"
#include "register.h"

namespace x86 {
    Node* try_make_lea(Node*, Node*, Node*, i32);

    Node* make_add(Node* add) {
        NodeBinOp* n = (NodeBinOp*) add;
        assert(add->nt == NodeType::BinOp && n->op == Op::Add);
        Node* lhs = n->lhs();
        Node* rhs = n->rhs();

        if(lhs->nt == NodeType::Load && lhs->output.size == 1) { // TODO why the lhs->output.size == 1 ???
            todo;
        }
        if(rhs->nt == NodeType::Load) { todo; }

        if(rhs->nt == NodeType::Const && ((NodeConst*)(rhs))->val->ttype == TypeT::Int) {
            i64 imm = ((TypeInt*)(((NodeConst*)(rhs))->val))->val();
            if(sizeofival(imm) == sizeof(i64)) {
                // if can't fit in 32 bits - have to Mov and then Add 2 registers
                return x86NodeOpR::create(add);
            }
            // TODO maybe add these 2 later
            // imm can fit in 32 bits
            // if(lhs->nt == NodeType::BinOp && lhs->op() == Op::Add) {
            //     // (base + (idx << scale)) + off
            //     NodeBinOp* ladd = (NodeBinOp*)lhs;
            //     return try_make_lea(add, ladd->lhs(), ladd->rhs(), (i32)imm);
            // }
            // TODO add the shift operators
            // if(lhs->nt == NodeType::BinOp && lhs->op() == Op::Shl) {
            //     // (idx << scale) + off
            //     NodeBinOp* lshl = (NodeBinOp*)lhs;
            //     return try_make_lea(add, nullptr, lshl->rhs(), (i32)imm);
            // }

            return x86NodeOpI::create(add);
        }
        return try_make_lea(add, lhs, rhs, 0);
    }
};