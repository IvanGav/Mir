#pragma once

#include "../son/node.h"

namespace compile {
    void dump_node(Node* n, Vec<u8>& str) {
        str.push_slice(node::to_str(n->nt));
        str.push('\t');
        str.push_slice(str::from_int(n->uid));
        str.push_slice("\t["_s);
        for(u32 i = 0; i < n->input.size; i++) {
            if(n->input[i] == nullptr)  str.push_slice("null"_s);
            else                        str.push_slice(str::from_int(n->input[i]->uid));
            str.push_slice(", "_s);
        }
        str.push_slice("]\t"_s);
        switch(n->nt) {
            case NodeType::Const: str.push_slice(type::to_str(n->type)); break;
            case NodeType::CtrlProj: str.push_slice(str::from_int(((NodeProj*)n)->index)); break;
            case NodeType::BinOp: str.push_slice(op::symbol(((NodeBinOp*)n)->op)); break;
            case NodeType::UnOp: str.push_slice(op::symbol(((NodeUnOp*)n)->op)); break;
            default: break;
        }
        str.push('\n');
    }

    Str dump(Node* start) {
        assert(node::cfg_size > 0); // require idom
        Vec<u8> str{};
        for(u32 i = 0; i < node::cfg_size; i++) {
            CFGNode* cfg = node::cfgrp[i];
            compile::dump_node(cfg, str);
            for(u32 i = 0; i < cfg->output.size; i++) {
                if(cfg->output[i]->cfg()) continue;
                str.push('\t');
                compile::dump_node(cfg->output[i], str);
            }
            str.push('\n');
        }
        return str.full_slice();
    }
};