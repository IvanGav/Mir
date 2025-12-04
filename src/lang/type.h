#pragma once

#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/slice.h"
#include "../core/set.h"
#include "../core/str.h"

#include "../llvm/static.h"

#define impl_type_hash(concrete_type) u64 hash(){return type::hash(reinterpret_cast<Type*>(this));}

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

    // Specialized
    Bool, UInt, Int, // TypeInt
    Float, // TypeFloat
    Tuple, // TypeTuple
    Ptr, // TypePtr (raw pointer)
    Array, // TypeArray (sized array)
    Slice, // TypeSlice (unsized array)
    Str, // TypeStr (uhhhh)

    // Unimplemented yet
    Struct, // TypeStruct
    Named, // TypeNamed (usage of a named type; concrete type not resolved yet)
};

struct Type {
    TypeI tinfo;
    TypeT ttype; // basically a tag for what (sub)class it is (with a little bit extra semantics, such as `Bool` and `Int` being different)
    
    impl_type_hash(Type);
    impl_eq(Type);
};

/* Specific types */

struct TypeInt {
    // TODO look at "known bits"
    Type self;
    u64 val_min;
    u64 val_max;

    // return the size, in bytes, of this type
    u8 get_size_bytes() {
        return 8; // TODO temporary
        switch(self.ttype) {
            case TypeT::Bool: return 1;
            case TypeT::UInt: {
                if(val_max <= U8_MAX)
                    return 1;
                else if(val_max <= U16_MAX)
                    return 2;
                else if(val_max <= U32_MAX)
                    return 4;
                else if(val_max <= U64_MAX)
                    return 8;
                panic;
            }
            case TypeT::Int: {
                if(i64(val_min) >= I8_MAX && i64(val_max) <= I8_MAX)
                    return 1;
                if(i64(val_min) >= I16_MAX && i64(val_max) <= I16_MAX)
                    return 2;
                if(i64(val_min) >= I32_MAX && i64(val_max) <= I32_MAX)
                    return 4;
                if(i64(val_min) >= I64_MAX && i64(val_max) <= I64_MAX)
                    return 8;
                panic;
            }
        }
        unreachable;
    }

    impl_type_hash(TypeInt);
    impl_eq(TypeInt);
};

struct TypeFloat {
    Type self;
    f64 val_min;
    f64 val_max;

    impl_type_hash(TypeFloat);
    impl_eq(TypeFloat);
};

struct TypeTuple {
    Type self;
    Slice<Type*> val;
};

struct TypePtr {
    Type self;
    Type* val;
};

struct TypeArray {
    Type self;
    Type* val;
    u64 size;
};

struct TypeSlice {
    Type self;
    Type* val;
    // u64 size; // TODO ???
};

struct TypeStr {
    Type self;
    Str val;

    impl_type_hash(TypeStr);
    auto operator==(const TypeStr& rhs) const { return (self == rhs.self && val == rhs.val); }
};

/* Type Pool */

// You "ask" for a Type
struct TypePool {
    mem::Arena* type_arena;

    HSet<Type> s_type;
    HSet<TypeInt> s_type_int;
    HSet<TypeFloat> s_type_float;
    HSet<TypeStr> s_type_str;

    Type* ask(Type t) {
        s_type.add(t);
        return s_type.get(t);
    }
    
    // bool
    TypeInt* ask_bool_any() {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 0, .val_max = 1 });
    }
    TypeInt* ask_bool_const(bool val) {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = (u64) val, .val_max = (u64) val });
    }
    // unsigned int
    TypeInt* ask_uint_sized(u8 bytes) {
        u64 max;
        switch(bytes) {
            case 1: max = U8_MAX; break;
            case 2: max = U16_MAX; break;
            case 4: max = U32_MAX; break;
            case 8: max = U64_MAX; break;
            default: panic;
        }
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::UInt}, .val_min = 0, .val_max = max });
    }
    TypeInt* ask_uint_const(u64 val) {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::UInt}, .val_min = val, .val_max = val });
    }
    // signed int
    TypeInt* ask_int_sized(u8 bytes) {
        i64 max, min;
        switch(bytes) {
            case 1: max = I8_MAX; min = I8_MIN; break;
            case 2: max = I16_MAX; min = I16_MIN; break;
            case 4: max = I32_MAX; min = I32_MIN; break;
            case 8: max = I64_MAX; min = I64_MIN; break;
            default: panic;
        }
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = std::bit_cast<u64>(min), .val_max = std::bit_cast<u64>(max)});
    }
    TypeInt* ask_int_const(i64 val) {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = std::bit_cast<u64>(val), .val_max = std::bit_cast<u64>(val) });
    }
    // other int
    TypeInt* ask_int(TypeInt t) {
        s_type_int.add(t);
        return s_type_int.get(t);
    }

    // float
    TypeFloat* ask_float(TypeFloat t) {
        s_type_float.add(t);
        return s_type_float.get(t);
    }

    // str
    TypeStr* ask_str(TypeStr t) {
        s_type_str.add(t);
        return s_type_str.get(t);
    }
};

/* debug */

std::ostream& operator<<(std::ostream& os, TypeI ti) {
    switch (ti) {
        case TypeI::Top:    return os << "Top";
        case TypeI::Known:  return os << "Known";
        case TypeI::Bottom: return os << "Bottom";
    }
    unreachable;
}

std::ostream& operator<<(std::ostream& os, TypeT tt) {
    switch (tt) {
        case TypeT::Pure:   return os << "Pure";

        case TypeT::Bool:   return os << "Bool";
        case TypeT::UInt:   return os << "UInt";
        case TypeT::Int:    return os << "Int";

        case TypeT::Float:  return os << "Float";
        case TypeT::Tuple:  return os << "Tuple";
        case TypeT::Ptr:    return os << "Ptr";
        case TypeT::Array:  return os << "Array";
        case TypeT::Slice:  return os << "Slice";
        case TypeT::Str:    return os << "Str";

        case TypeT::Struct: return os << "Struct";
        case TypeT::Named:  return os << "Named";
    }
    unreachable;
}

/* functions defined on `Type*` */

namespace type {
    static TypePool pool;

    // hash of every type
    u64 hash(Type* t) {
        switch (t->ttype) {
            case TypeT::Pure: {
                Type* ty = static_cast<Type*>(t);
                return hash::from(ty->tinfo) ^ (hash::from(ty->ttype) << 2);
            }

            case TypeT::Bool:
            case TypeT::UInt:
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                u64 val = hash::from(ty->self.tinfo) ^ (hash::from(ty->self.ttype) << 2) ^ 
                    (hash::from(ty->val_max) ^ (hash::from(ty->val_max - ty->val_min) << 4));
                if(ty->self.ttype == TypeT::Int) return ~val;
                return val;
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                u64 val_min_cast = std::bit_cast<u64>(ty->val_min);
                u64 val_max_cast = std::bit_cast<u64>(ty->val_max);
                return hash::from(ty->self.tinfo) ^ (hash::from(ty->self.ttype) << 2) ^ 
                    (hash::from(val_max_cast) ^ (hash::from(val_max_cast - val_min_cast)));
            }

            case TypeT::Str: {
                TypeStr* ty = reinterpret_cast<TypeStr*>(t);
                return hash::from(ty->self.tinfo) ^ (hash::from(ty->self.ttype) << 2) ^ 
                    (hash::from(ty->val));
            }

            case TypeT::Tuple: {
                TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                panic;
            }

            case TypeT::Ptr: {
                TypePtr* ty = reinterpret_cast<TypePtr*>(t);
                panic;
            }

            case TypeT::Array: {
                TypeArray* ty = reinterpret_cast<TypeArray*>(t);
                panic;
            }

            case TypeT::Slice: {
                TypeSlice* ty = reinterpret_cast<TypeSlice*>(t);
                panic;
            }
        }

        unreachable;
    }

    void debug_print(Type* t) {
        switch (t->ttype) {
            case TypeT::Pure: {
                std::cout << "<Pure:" << t->tinfo << ">";
                return;
            }

            case TypeT::Bool: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<Bool:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<Bool:Bottom>";
                else if(ty->val_min == 0 && ty->val_max == 0)
                    std::cout << "false";
                else if(ty->val_min == 1 && ty->val_max == 1)
                    std::cout << "true";
                else if(ty->val_min == 1 && ty->val_max == 1)
                    std::cout << "<Bool:[false..true]>";
                return;
            }
            case TypeT::UInt: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<UInt:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<UInt:Bottom>";
                else if(ty->val_min == ty->val_max)
                    std::cout << ty->val_min;
                else
                    std::cout << "<UInt:[" << ty->val_min << ".." << ty->val_max << "]>";
                // TODO expand to display u8, u16, u32, u64
                return;
            }
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                panic; // TODO
                return;
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<Float:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<Float:Bottom>";
                else if(ty->val_min == ty->val_max)
                    std::cout << ty->val_min;
                else
                    std::cout << "<Float:[" << ty->val_min << ".." << ty->val_max << "]>";
                return;
            }

            case TypeT::Str: {
                TypeStr* ty = reinterpret_cast<TypeStr*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<Str:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<Str:Bottom>";
                else
                    std::cout << "\"" << ty->val << "\"";
                return;
            }

            case TypeT::Tuple: {
                TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                panic;
            }

            case TypeT::Ptr: {
                TypePtr* ty = reinterpret_cast<TypePtr*>(t);
                panic;
            }

            case TypeT::Array: {
                TypeArray* ty = reinterpret_cast<TypeArray*>(t);
                panic;
            }

            case TypeT::Slice: {
                TypeSlice* ty = reinterpret_cast<TypeSlice*>(t);
                panic;
            }
        }

        unreachable;
    }

    bool is_const(Type* t) {
        switch (t->ttype) {
            case TypeT::Pure: return false;

            case TypeT::Bool:
            case TypeT::UInt:
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                return ty->self.tinfo == TypeI::Known && ty->val_max == ty->val_min;
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                return ty->self.tinfo == TypeI::Known && ty->val_max == ty->val_min;
            }

            case TypeT::Str: {
                TypeStr* ty = reinterpret_cast<TypeStr*>(t);
                return ty->self.tinfo == TypeI::Known;
            }

            case TypeT::Tuple: {
                TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                panic;
            }

            case TypeT::Ptr: {
                TypePtr* ty = reinterpret_cast<TypePtr*>(t);
                panic;
            }

            case TypeT::Array: {
                TypeArray* ty = reinterpret_cast<TypeArray*>(t);
                panic;
            }

            case TypeT::Slice: {
                TypeSlice* ty = reinterpret_cast<TypeSlice*>(t);
                panic;
            }
        }
        unreachable;
    }

    // "Intersection"
    Type* meet(Type* t1, Type* t2) {
        // meet of two unrelated types is always Pure:Bottom
        if(t1->ttype != t2->ttype) return pool.ask(Type { .tinfo = TypeI::Bottom, .ttype = TypeT::Pure });

        // for all types, Top meet Something = Something
        if(t1->tinfo == TypeI::Top) return t2;
        if(t2->tinfo == TypeI::Top) return t1;
        // for all types, Something meet Bottom = Bottom
        if(t1->tinfo == TypeI::Bottom) return t1;
        if(t2->tinfo == TypeI::Bottom) return t2;
        
        switch (t1->ttype) {
            case TypeT::Pure: return t1;

            case TypeT::Bool:
            case TypeT::UInt:
            case TypeT::Int: {
                TypeInt* ti1 = reinterpret_cast<TypeInt*>(t1);
                TypeInt* ti2 = reinterpret_cast<TypeInt*>(t2);
                u64 min = max(ti1->val_min, ti2->val_min);
                u64 max = min(ti1->val_max, ti2->val_max);
                if(min > max)
                    return reinterpret_cast<Type*>(pool.ask_int( TypeInt { .self = Type { .tinfo = TypeI::Bottom, .ttype = ti1->self.ttype } } ));
                return reinterpret_cast<Type*>(pool.ask_int( TypeInt { .self = Type { .tinfo = TypeI::Known, .ttype = ti1->self.ttype }, .val_min = min, .val_max = max } ));
            }

            case TypeT::Float: {
                TypeFloat* tf1 = reinterpret_cast<TypeFloat*>(t1);
                TypeFloat* tf2 = reinterpret_cast<TypeFloat*>(t2);
                f64 min = max(tf1->val_min, tf2->val_min);
                f64 max = min(tf1->val_max, tf2->val_max);
                if(min > max)
                    return reinterpret_cast<Type*>(pool.ask_float( TypeFloat { .self = Type { .tinfo = TypeI::Bottom, .ttype = tf1->self.ttype } } ));
                return reinterpret_cast<Type*>(pool.ask_float( TypeFloat { .self = Type { .tinfo = TypeI::Known, .ttype = tf1->self.ttype }, .val_min = min, .val_max = max } ));
            }

            case TypeT::Str: {
                TypeStr* ts1 = reinterpret_cast<TypeStr*>(t1);
                TypeStr* ts2 = reinterpret_cast<TypeStr*>(t2);
                if(ts1->val != ts2->val)
                    return reinterpret_cast<Type*>(pool.ask_str( TypeStr { .self = Type { .tinfo = TypeI::Bottom, .ttype = ts1->self.ttype } } ));
                return reinterpret_cast<Type*>(ts1);
            }

            case TypeT::Tuple: {
                panic;
            }

            case TypeT::Ptr: {
                panic;
            }

            case TypeT::Array: {
                panic;
            }

            case TypeT::Slice: {
                panic;
            }
        }
        unreachable;
    }

    // "Union"
    Type* join(Type* t1, Type* t2) {
        panic;
    }
}