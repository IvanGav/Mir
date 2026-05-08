#pragma once

#include "reg_alloc/reg_alloc.h"

namespace x86 {
    void emit_label(std::ofstream&, CFGNode*);
    void emit_cfg(std::ofstream&, RegAlloc*, CFGNode*);
    void emit_node(std::ofstream&, RegAlloc*, Node*);

    Str reg(RegAlloc* alloc, Node* n) {
        u16 r = alloc->regnum(n);
        assert(r < 16);
        return regname[r]; // names of registers, in `register.h`
    }

    // vvv TODO this is a hack to get it looking like it works for the design expo.. delete later and actually fix. The real solution would be to insert splits in the if branches for constants... or something like that i think.
    bool const_used_only_by_phis(Node* n) {
        if(n->nt != NodeType::Const) return false;
        if(n->output.size == 0) return false;

        for(Node* use : n->output) {
            if(use->nt != NodeType::Phi) return false;
        }

        return true;
    }

    void emit_phi_edge_moves(std::ofstream& of, RegAlloc* alloc, CFGNode* pred, CFGNode* succ) {
        if(!(succ->nt == NodeType::Region || succ->nt == NodeType::Loop)) return;

        u32 pred_idx = succ->input.index_of(pred);
        assert(pred_idx != succ->input.size);

        for(Node* out : succ->output) {
            if(out->nt != NodeType::Phi) continue;

            NodePhi* phi = (NodePhi*)out;

            if(out->type->ttype == TypeT::Mem) continue;

            Node* src = phi->data(pred_idx);
            Node* dst = out;

            if(src->nt == NodeType::Const) {
                NodeConst* c = (NodeConst*)src;
                assert(c->val->ttype == TypeT::Int);
                i64 val = ((TypeInt*)c->val)->val();

                // Important: write the constant into the PHI register,
                // not the Const node's scheduled register.
                of << "  mov " << reg(alloc, dst) << ", " << val << "\n";
            } else {
                of << "  mov " << reg(alloc, dst) << ", " << reg(alloc, src) << "\n";
            }
        }
    }
    // ^^^ also at the same time remove anything with TODO HACK

    void emit_header(std::ofstream& of) {
        of << 
"# arg is in rdi, return in rax\n"
".intel_syntax noprefix\n"
".global _start\n"
".section .text\n"
"_start:\n"
"  mov rbx, [rsp]        # argc\n"
"  cmp rbx, 2\n"
"  jl no_arg             # need at least 1 argument\n"
"  mov rsi, [rsp+16]     # argv[1] (skip argc + argv[0])\n"
"  xor rdi, rdi          # clear rax\n"
"parse_loop:\n"
"  mov bl, byte ptr [rsi]\n"
"  cmp bl, 0\n"
"  je done\n"
"  sub bl, '0'           # convert ASCII to digit\n"
"  imul rdi, rdi, 10\n"
"  add rdi, rbx\n"
"  inc rsi\n"
"  jmp parse_loop\n"
"done:\n"
"# start of mir code here"
        << std::endl;
    }

    void emit_footer(std::ofstream& of) {
        of <<
"# end of mir code here\n"
"  no_arg:\n"
"  mov rdi, 0\n"
"  mov rax, 60\n"
"  syscall\n"
"return:\n"
"  mov rdi, rax\n"
"  mov rax, 60\n"
"  syscall\n"
        << std::endl;
    }

    void emit_jump(std::ofstream& of, CFGNode* cfg) {
        of << "  jmp .L" << cfg->cfgid << "\n";
    }

    bool is_terminal_in_block(Node* n) {
        if(n == nullptr) return true; // this is a weird edge case where, if there's nothing to output to, there's a "terminal", it's uhhh
        return n->nt == NodeType::If || n->nt == NodeType::Ret;
    }

    CFGNode* first_block_successor(CFGNode* bb) {
        for(Node* out : bb->output) {
            if(!out->cfg()) continue;

            // These are real successor block heads.
            if(node::is_block_head(out)) {
                return out;
            }
        }
        return nullptr;
    }

    void emit(const char* output_file_path, RegAlloc* alloc) {
        std::ofstream of(output_file_path);
        emit_header(of);
        for(CFGNode* bb : node::cfgrp) {
            if(!node::is_block_head(bb)) continue;
            emit_cfg(of, alloc, bb);
            for(Node* n : bb->output) {
                if(const_used_only_by_phis(n)) continue; // TODO HACK
                emit_node(of, alloc, n);
            }
            // at the end of the branch, jump to uniting region
            CFGNode* cfg_out = first_block_successor(bb);
            if(!is_terminal_in_block(cfg_out)) {
                emit_phi_edge_moves(of, alloc, bb, cfg_out); // TODO HACK
                emit_jump(of, cfg_out);
            }
        }
        emit_footer(of);
    }

    void emit_node(std::ofstream& of, RegAlloc* alloc, Node* n) {
        switch(n->nt) {
            case NodeType::Const: {
                // mov dst, imm
                NodeConst* c = (NodeConst*) n;
                assert(c->val->ttype == TypeT::Int);
                i64 val = ((TypeInt*)c->val)->val();
                of << "  mov " << reg(alloc, n) << ", " << val << "\n";
                break;
            }
            case NodeType::BinOp: {
                // Two-address: input[1] shares output register (must already be same after reg_alloc)
                // op dst/lhs, rhs
                NodeBinOp* b = (NodeBinOp*) n;
                Str dst = reg(alloc, n);       // == reg of input[1] after 2-addr coalescing
                Str rhs = reg(alloc, b->rhs());
                switch(b->op) {
                    case Op::Add: of << "  add " << dst << ", " << rhs << "\n"; break;
                    case Op::Sub: of << "  sub " << dst << ", " << rhs << "\n"; break;
                    case Op::Mul: of << "  imul " << dst << ", " << rhs << "\n"; break;
                    // TODO HACK comparisons are printed in ifs.. yes that *does* mean that I cannot do `i = 1 == 2` but for now.. whatever
                    case Op::Less: case Op::LessEq: case Op::Greater: case Op::GreaterEq: case Op::Eq: case Op::Neq:
                        break;
                    default: panic; // unhandled ops are bad
                }
                break;
            }
            case NodeType::Load: {
                NodeLoad* l = (NodeLoad*) n;
                Str dst = reg(alloc, n);
                Str base = reg(alloc, l->ptr());
                Str off  = reg(alloc, l->off());
                of << "  mov " << dst << ", [" << base << " + " << off << "]\n";
                break;
            }
            case NodeType::Store: {
                NodeStore* s = (NodeStore*) n;
                Str base = reg(alloc, s->ptr());
                Str off  = reg(alloc, s->off());
                Str val  = reg(alloc, s->val());
                of << "  mov [" << base << " + " << off << "], " << val << "\n";
                break;
            }
            case NodeType::Split: {
                // A split that survived post_color is a real register-to-register move
                NodeSplit* s = (NodeSplit*) n;
                of << "  mov " << reg(alloc, n) << ", " << reg(alloc, s->val()) << "\n";
                break;
            }
            case NodeType::Ret: {
                // return value already in RAX (enforced by regmap)
                of << "  jmp return\n"; // THIS IS HOW YOU RETURN IN MIR FROM NOW ON I GUESS
                break;
            }
            case NodeType::If: {
                // The condition was computed into some register by a BinOp comparison.
                // You need a `test`/`cmp` + conditional jump.
                // The true proj (index 0) is the fall-through or taken branch.
                NodeIf* ifc = (NodeIf*) n;
                // find the proj targets
                CFGNode* true_bb  = nullptr;
                CFGNode* false_bb = nullptr;
                for(Node* out : n->output) {
                    if(out->nt == NodeType::CtrlProj) {
                        NodeProj* p = (NodeProj*) out;
                        if(p->index == 0) true_bb  = out; // taken
                        else              false_bb = out; // not taken
                    }
                }
                Node* cond_node = ifc->condition();
                if(cond_node->nt == NodeType::BinOp) {
                    NodeBinOp* cmp = (NodeBinOp*) cond_node;
                    of << "  cmp " << reg(alloc, cmp->lhs()) << ", " << reg(alloc, cmp->rhs()) << "\n";
                    Str jcc;
                    switch(cmp->op) {
                        case Op::Less:      jcc = "jl"_s;  break;
                        case Op::LessEq:    jcc = "jle"_s; break;
                        case Op::Greater:   jcc = "jg"_s;  break;
                        case Op::GreaterEq: jcc = "jge"_s; break;
                        case Op::Eq:        jcc = "je"_s;  break;
                        case Op::Neq:       jcc = "jne"_s; break;
                        default: panic;
                    }
                    of << "  " << jcc << " .L" << true_bb->cfgid << "\n";
                    emit_jump(of, false_bb);
                } else {
                    warn;
                    // condition register
                    Str cond = reg(alloc, ifc->condition());
                    of << "  test " << cond << ", " << cond << "\n";
                    of << "  jnz .L" << true_bb->cfgid << "\n";
                    emit_jump(of, false_bb);
                }
                break;
            }
            default: break;
        }
    }

    void emit_label(std::ofstream& of, CFGNode* bb) {
        of << ".L" << bb->cfgid << ":\n";
    }

    void emit_cfg(std::ofstream& of, RegAlloc* alloc, CFGNode* bb) {
        switch(bb->nt) {
            case NodeType::Start: // already printing _start, so doesn't matter
                // fall through from predecessor, or back-edge jump already emitted
                break;
            default: {
                emit_label(of, bb);
                break;
            }
        }
    }
};