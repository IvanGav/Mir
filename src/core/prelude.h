#pragma once

#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <cstdarg>
#include <stdalign.h>
#include <bit>
#include <type_traits>

/* llvm */

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef uintptr_t usize;
typedef float f32;
typedef double f64;

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFFULL
#define I8_MAX 0x7F
#define I16_MAX 0x7FFF
#define I32_MAX 0x7FFFFFFF
#define I64_MAX 0x7FFFFFFFFFFFFFFFULL
#define I8_MIN i8(0x80)
#define I16_MIN i16(0x8000)
#define I32_MIN i32(0x80000000)
#define I64_MIN i64(0x8000000000000000LL)
#define F32_SMALL (__builtin_bit_cast(f32, 0x00800000u))
#define F32_LARGE (__builtin_bit_cast(f32, 0x7F7FFFFFu))
#define F32_INF (__builtin_bit_cast(f32, 0x7F800000u))
#define F32_QNAN (__builtin_bit_cast(f32, 0x7FFFFFFFu))
#define F32_SNAN (__builtin_bit_cast(f32, 0x7FBFFFFFu))
#define F64_SMALL (__builtin_bit_cast(f64, 0x0010000000000000ull))
#define F64_LARGE (__builtin_bit_cast(f64, 0x7FEFFFFFFFFFFFFFull))
#define F64_INF (__builtin_bit_cast(f64, 0x7FF0000000000000ull))
#define F64_QNAN (__builtin_bit_cast(f64, 0x7FFFFFFFFFFFFFFFull))
#define F64_SNAN (__builtin_bit_cast(f64, 0x7FF7FFFFFFFFFFFFull))

#define KB *1024
#define MB *1024*1024

#define unreachable { assert(false); std::abort(); }

#define panic { printf("BAD SYNTAX"); assert(false); std::abort(); }

#define min(a, b) a > b ? b : a
#define max(a, b) a > b ? a : b

#define printd(expr) std::cout << "--DEBUG " #expr ": " << (expr) << std::endl;

usize next_power_of_two(usize n) {
    assert(sizeof(usize) == sizeof(unsigned long long));
    i32 leading_zeros = __builtin_clzll(n);
    usize closest_pow_2 = 1 << (sizeof(usize)*8 - leading_zeros - 1);
    if(n != closest_pow_2) closest_pow_2 <<= 1;
    return closest_pow_2;
}

static const usize prime_sizes[] = {
    // definitely primes:
    53, 97, 193, 389, 769,
    1543, 3079, 6151, 12289, 24593,
    49157, 98317, 196613, 393241,
    // not tested for being primes:
    786433, 1572869, 3145739,
    6291469, 12582917, 25165843,
    50331653, 100663319, 201326611,
    402653189, 805306457, 1610612741
};

usize next_prime_size(usize n) {
    for(usize i = 0; i < 26; i++) {
        if(prime_sizes[i] > n) return prime_sizes[i];
    }
    unreachable;
}

u8 sizeofival(i64 val) {
    if(val >= I8_MIN && val <= I8_MAX) {
        return sizeof(i8);
    } else if(val >= I16_MIN && val <= I16_MAX) {
        return sizeof(i16);
    } else if(val >= I32_MIN && val <= I32_MAX) {
        return sizeof(i32);
    } else if(val >= I64_MIN && val <= I64_MAX) {
        return sizeof(i64);
    }
    unreachable;
}

u8 sizeofuval(u64 val) {
    if(val <= U8_MAX) {
        return sizeof(u8);
    } else if(val <= U16_MAX) {
        return sizeof(u16);
    } else if(val <= U32_MAX) {
        return sizeof(u32);
    } else if(val <= U64_MAX) {
        return sizeof(u64);
    }
    unreachable;
}

#define impl_eq(concrete_type)  auto operator<=>(const concrete_type&) const = default; \
                                auto operator==(const concrete_type& rhs) const { return (*this <=> rhs) == 0; } \
                                auto operator!=(const concrete_type& rhs) const { return (*this <=> rhs) != 0; }
