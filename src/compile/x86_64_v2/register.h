#pragma once

#include "../../core/prelude.h"
#include "../../core/str.h"

// I think: rax, rcx, rdx, rsi, rdi, r8–r11 are caller saved, rbx, rbp, r12–r15 are callee saved, rdi, rsi, rdx, rcx, r8, r9 function args, rax function return value
// None of this matters for me rn, since I don't even have functions

enum Reg : u16 {
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
    u16 regs;
};

RegMask RMASK = RegMask { U16_MAX }; // can read from any register
RegMask WMASK = RegMask { u16(RMASK.regs ^ bRSP) }; // cannot write to stack pointer









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
        return (mod << 6) | ((reg & 0b111) << 3) | rm & 0b111;
    }

    u8 sib(u8 scale, u8 index, u8 base) {
        return (scale << 6) | ((index & 0b111) << 3) | base & 0b111;
    }
};