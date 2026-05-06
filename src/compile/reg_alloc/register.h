#pragma once

#include "../../son/prelude.h"
#include "../../son/node.h"

// Registers for x86 64 v2. No float support in my version for now.

// I think: rax, rcx, rdx, rsi, rdi, r8–r11 are caller saved, rbx, rbp, r12–r15 are callee saved, rdi, rsi, rdx, rcx, r8, r9 function args, rax function return value
// None of this matters for me rn, since I don't even have functions

enum Reg : u16 {
    UNDEFINED = U16_MAX,
    KILL = U16_MAX - 1,
    RAX = 0, // A = accumulator
    RBX = 1, // B = base
    RCX = 2, // C = counter
    RDX = 3, // D = data
    RSI = 4, // SI = source index
    RDI = 5, // DI = destination index
    RSP = 6, // SP = stack pointer
    RBP = 7, // BP = base pointer
    R08 = 8,
    R09 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,

    bRAX = 1<<RAX,
    bRBX = 1<<RBX,
    bRCX = 1<<RCX,
    bRDX = 1<<RDX,
    bRSI = 1<<RSI,
    bRDI = 1<<RDI,
    bRSP = 1<<RSP,
    bRBP = 1<<RBP,
    bR08 = 1<<R08,
    bR09 = 1<<R09,
    bR10 = 1<<R10,
    bR11 = 1<<R11,
    bR12 = 1<<R12,
    bR13 = 1<<R13,
    bR14 = 1<<R14,
    bR15 = 1<<R15
};

Str regname[16] {"RAX"_s,"RBX"_s,"RCX"_s,"RDX"_s,"RSI"_s,"RDI"_s,"RSP"_s,"RBP"_s,"R08"_s,"R09"_s,"R10"_s,"R11"_s,"R12"_s,"R13"_s,"R14"_s,"R15"_s};

struct RegMask {
    u64 regs; // additional 48 bits are for spills and stuff

    // set bit count
    u8 size() const {
        return (u8) __builtin_popcountll(regs); // TODO is.. that rightt???
    }
    bool is_size_1() { return (regs & -regs) == regs; }
    bool is_empty() { return regs == 0; }
    RegMask operator&(RegMask const& other) const {
        return RegMask { .regs = u64(this->regs & other.regs) };
    }
    RegMask operator-(RegMask const& other) const {
        return RegMask { .regs = u64(this->regs & ~other.regs) };
    }
    RegMask operator-(u8 other) const {
        return RegMask { .regs = u64(this->regs & ~(1 << other)) };
    }
    auto operator<=>(RegMask const&) const = default;
    bool overlaps(RegMask const& other) const {
        return (regs & other.regs) > 0;
    }
    Reg first_reg() const {
        if(regs == 0) { warn; return Reg::UNDEFINED; } // no register available
        u8 count = __builtin_ctzll(regs); // Count Tailing Zeros Long Long
        assert(count < 15); // temporary; when dealing with spills might be wrong, buuuttt
        return (Reg)count;
    }
    bool test(u8 reg) {
        assert(reg < 64);
        return ((regs >> reg) & 1) != 0;
    }
};

RegMask RMASK = RegMask { U16_MAX }; // can read from any register
RegMask WMASK = RegMask { u64(RMASK.regs ^ bRSP) }; // cannot write to stack pointer

// TODO_AI note that most of these were done by Claude
/*
For comparisons could do:

On NodeBinOp:
cmp lhs, rhs
setl al          ; or setg, sete, etc.
movzx rax, al   ; zero-extend byte to 64-bit

On NodeIf:
test rax, rax
jnz true_label
jmp false_label
*/
namespace x86 {
    // TODO shift amount is `rcx` only; div/mod are `rax/rdx` only
    RegMask regmap(Node* n, u32 i) {
        if(i == 0) return RegMask{0}; // ctrl input, never a register
        switch(n->nt) {
            case NodeType::BinOp: {
                // inputs 1 and 2 are both GPR values
                // input 1 is two-address (shares output), but mask is still RMASK
                return RMASK;
            }
            case NodeType::UnOp:
                return RMASK; // input 1 is the operand, destructive
            case NodeType::Load: {
                // input 1 = mem (no register), input 2 = ptr (GPR), input 3 = offset (GPR)
                if(i == 1) return RegMask{0}; // memory edge
                return RMASK; // ptr and offset need registers
            }
            case NodeType::Store: {
                // input 1 = mem (no register), input 2 = ptr (GPR),
                // input 3 = offset (GPR),      input 4 = val (GPR)
                if(i == 1) return RegMask{0}; // memory edge
                return RMASK;
            }
            case NodeType::AllocA: {
                // input 1 = size (GPR), input 2 = mem (no register)
                if(i == 2) return RegMask{0}; // memory edge
                return RMASK;
            }
            case NodeType::Ret: {
                // input 0 = ctrl (handled above), input 1 = return value
                // On x86-64, return value goes in RAX
                if(i == 1) return RegMask{bRAX};
                return RegMask{0};
            }
            case NodeType::Phi:
                // memory phis: no register for any input
                if(n->type->ttype == TypeT::Mem) return RegMask{0};
                return RMASK;
            case NodeType::Split:
                return RMASK; // input 1 is the value being copied
            default:
                return RegMask{0};
        }
    }
    RegMask outregmap(Node* n) {
        switch(n->nt) {
            case NodeType::BinOp:
            case NodeType::UnOp:
            case NodeType::Const:
            case NodeType::Load:
            case NodeType::AllocA: // produces a pointer
            case NodeType::Split:
                return WMASK;
            
            case NodeType::Phi:
                // memory phis don't need a register
                return n->type->ttype == TypeT::Mem ? RegMask{0} : WMASK;

            case NodeType::Proj:
                // memory projections don't need a register
                return n->type->ttype == TypeT::Mem ? RegMask{0} : WMASK;

            default:
                return RegMask{0};
        }
    }
    // is clone = cheaper to recreate than to spill
    bool is_clone(Node* n) {
        return n->nt == NodeType::Const;
    }
    u32 two_address(Node* n) {
        if(n->nt == NodeType::BinOp) return 1; // lhs must share output reg
        if(n->nt == NodeType::Store) return 0; // no output register at all
        if(n->nt == NodeType::UnOp)  return 1; // unary ops are also destructive on x86
        return 0;
    }
    // are "symmetric"
    bool commutes(Node* n) {
        if(n->nt != NodeType::BinOp) return false;
        NodeBinOp* b = (NodeBinOp*) n;
        switch(b->op) {
            case Op::Add:
            case Op::Mul:
            case Op::BitAnd:
            case Op::BitOr:
            case Op::BitXor:
            case Op::Eq:
            case Op::Neq:
                return true;
            default:
                return false;
        }
    }
    bool is_mach(Node* n) {
        switch(n->nt) {
            case NodeType::BinOp:
            case NodeType::UnOp:
            case NodeType::Const:
            case NodeType::Load:
            case NodeType::Store:
            case NodeType::AllocA:
            case NodeType::Proj:
            case NodeType::Phi:
            case NodeType::Split:
            case NodeType::Ret: // takes input -> makes it a "machine" node
                return true;
            default:
                return false;
        }
    }
    Node* copy_node(Node* n) {
        assert(n->nt == NodeType::Const); // only "is_clone" allowed, which is just Const for now
        NodeConst* node = (NodeConst*)n;
        return NodeConst::create(node->val);
    }
    RegMask killmap(Node* n) {
        assert(x86::is_mach(n));
        return RegMask{0}; // no caller-save kills; no function calls; maybe needed for divide/modulo
    }
};





// Let's forget about this for the time being =)
namespace encoding {
    // rex = register extension prefix byte (https://en.wikipedia.org/wiki/REX_prefix)
    // modrm = mod-reg-r/m field
    //   mod (2 bits)   = generally, b11 indicates register-direct addressing mode; register-indirect addressing mode otherwise (AddrMode)
    //   reg (3/4 bits) = opcode extension OR reg reference (instr src or dst, depending on instr)
    //   rm  (3/4 bits) = register operand
    // sib = scaled index byte
    //   scale (2 bits)   = scaling factor of sib.index -> s = 2**scale
    //   index (3/4 bits) = index register to use
    //   base  (3/4 bits) = base register to use

    enum Rex : u8 {
        Base = 0b01000000,
        W = 0b00001000, // extends the operands to be 64 bit
        R = 0b00000100, // extends the modrm.reg
        X = 0b00000010, // extends the sib.index
        B = 0b00000001, // extends the modrm.r/m or sib.base
    };

    enum AddrMode : u8 {
        Indirect = 0, // [mem]
        Indirect8 = 1, // [mem + 0x12]
        Indirect32 = 2, // [mem + 0x12345678]
        Direct = 3, // mem
    };

    u8 rex(u8 reg, u8 ptr, u8 index, bool wide = true) {
        assert(-1 <= reg && reg < 16);
        assert(-1 <= ptr && ptr < 16);
        assert(-1 <= index && index < 16);
        u8 rex = Rex::Base;
        if(wide)       rex |= Rex::W;
        if(8 <= reg)   rex |= Rex::R;
        if(8 <= ptr)   rex |= Rex::B;
        if(8 <= index) rex |= Rex::X;
        return rex;
    }

    u8 modrm(AddrMode mod, u8 reg, u8 rm) {
        return (mod << 6) | ((reg & 0b111) << 3) | (rm & 0b111);
    }

    u8 sib(u8 scale, u8 index, u8 base) {
        return (scale << 6) | ((index & 0b111) << 3) | (base & 0b111);
    }
};