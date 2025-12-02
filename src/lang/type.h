#pragma once

#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/slice.h"
#include "../core/map.h"
#include "../core/str.h"

// type information level
enum class TypeI {
    Top,    // usually, valid but nothing concrete is known
    Known,  // usually, exact value is known
    Bottom, // usually, impossible
};

enum class TypeT {
    // Special
    // Pure, // Type Top (such as `let mystery;` with assignment later down the line)
    // Ambiguous, // Type Bottom (Likely a compile time error)
    Pure, // Type (either `let mystery;` with assignment later down the line OR meet of incompatible types)

    // Specialized
    Bool, UInt, Int, Enum, // TypeInt
    Float, // TypeFloat
    Tuple, // TypeTuple
    Ptr, // TypePtr (raw pointer)
    Array, // TypeArray (sized array)
    Slice, // TypeSlice (unsized array)
    Struct, // TypeStruct
    Named, // TypeNamed (usage of a named type; concrete type not resolved yet)
};

struct Type {
    TypeI tinfo;
    TypeT ttype; // basically a tag for what (sub)class it is (with a little bit extra semantics, such as `Bool` and `Int` being different)
};

/* Specific types */

struct TypeInt : Type {
    // TODO look at "known bits"
    bool is_signed;
    u64 val_min;
    u64 val_max;

    // return the size, in bytes, of this type
    u8 get_size_bytes() {}
};

struct TypeFloat : Type {
    f64 val_min;
    f64 val_max;
};

struct TypeTuple : Type {
    Slice<Type*> val;
};

struct TypePtr : Type {
    Type* val;
};

struct TypeArray : Type {
    Type* val;
    u64 size;
};

struct TypeSlice : Type {
    Type* val;
    // u64 size; // TODO ???
};

struct TypeStr : Type {
    Str val;
};

/* Type Pool */

// You "ask" for a Type
// struct TypePool {
//     mem::Arena* type_pool;

//     Type* ask(TypeI i, TypeT t) {

//     }
//     TypeInt* ask_int_const(u64 val) {

//     }
//     TypeInt* ask_uint_const(u64 val) {
        
//     }
//     Type* request(TypeI i, TypeT t) {

//     }
//     Type* request(TypeI i, TypeT t) {

//     }
// };