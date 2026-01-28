#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

struct Type;
namespace type {
    u64 hash(Type* t);
};

// type information level
enum class TypeI {
    Top,    // usually, valid but nothing concrete is known
    Known,  // usually, exact value is known
    Bottom, // usually, impossible
};

enum class TypeT {
    // Generic
    Pure, // Type (either `let mystery;` with assignment later down the line OR meet of incompatible types)
    Ctrl, // Type

    // Specialized
    Bool, Int, // TypeInt
    Float, // TypeFloat
    Tuple, // TypeTuple
};

struct Type {
    TypeI tinfo;
    TypeT ttype; // basically a tag for what (sub)class it is (with a little bit extra semantics, such as `Bool` and `Int` being different)
    bool operator==(const Type&) const = default;
    u64 hash() { return type::hash(this); }

    static Type top(TypeT tt) { return Type { .tinfo = TypeI::Top, .ttype = tt }; }
    static Type known(TypeT tt) { return Type { .tinfo = TypeI::Known, .ttype = tt }; }
    static Type bottom(TypeT tt) { return Type { .tinfo = TypeI::Bottom, .ttype = tt }; }
};

/* Specific types */

struct TypeInt {
    // TODO look at "known bits"
    Type self;
    i64 val_min;
    i64 val_max;
    bool operator==(const TypeInt&) const = default;
    u64 hash() { return type::hash((Type*)this); }

    i64 val() { assert(val_min == val_max); return val_min; }
};

struct TypeFloat {
    Type self;
    f64 val_min;
    f64 val_max;
    bool operator==(const TypeFloat&) const = default;
    u64 hash() { return type::hash((Type*)this); }

    f64 val() { assert(val_min == val_max); return val_min; }
};

struct TypeTuple {
    Type self;
    Slice<Type*> val;
    bool operator==(const TypeTuple&) const = default;
    u64 hash() { return type::hash((Type*)this); }
};