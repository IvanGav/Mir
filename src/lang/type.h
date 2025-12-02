#pragma once

#include "../core/prelude.h"
#include "../core/slice.h"

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
    u64 val_min;
    u64 val_max;

    // return the size, in bytes, of this type
    u8 get_size_bytes() {}
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

/* Type Pool */

struct TypePool {
    Type* request(TypeI i, TypeT t) {

    }
};