#pragma once

#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/slice.h"
#include "../core/set.h"
#include "../core/str.h"
#include "../core/maybe.h"

#include "../llvm/static.h"

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
    
    u64 hash() { return hash::from(tinfo) ^ (hash::from(ttype) << 2); }
    int operator==(const Type& rhs) const { return tinfo == rhs.tinfo && ttype == rhs.ttype; }
};

/* Specific types */

struct TypeInt {
    Type self;

    u8 bytes;
    Maybe<u64> val;

    // return the size, in bytes, of this type
    u8 get_size_bytes() {
        return bytes;
    }

    u64 hash() { u64 newval = self.hash() ^ ((val.here ? hash::from(val.val) : 1) << bytes); if(self.ttype == TypeT::Int) { return ~newval; } else { return newval; } }
    int operator==(const TypeInt& rhs) const { return self == rhs.self && bytes == rhs.bytes && val == rhs.val; }
};

struct TypeFloat {
    Type self;
    
    u8 bytes;
    Maybe<f64> val;

    u64 hash() { u64 newval = self.hash() ^ ((val.here ? hash::from(val.val) : 1) << bytes); return newval; }
    int operator==(const TypeFloat& rhs) const { return self == rhs.self && bytes == rhs.bytes && val == rhs.val; }
};

struct TypeStr {
    Type self;
    Str val;

    u64 hash() { return self.hash() ^ (hash::from(val)); }
    int operator==(const TypeStr& rhs) const { return self == rhs.self && val == rhs.val; }
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
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .bytes=1, .val=Maybe<u64>::none() });
    }
    TypeInt* ask_bool_const(bool val) {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .bytes=1, .val=Maybe<u64>::some((u64)val) });
    }
    // unsigned int
    TypeInt* ask_uint_sized(u8 bytes) {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::UInt}, .bytes=bytes, .val=Maybe<u64>::none() });
    }
    TypeInt* ask_uint_const(u64 val) {
        // TODO hardcoded to 8 bytes
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::UInt}, .bytes=8, .val=Maybe<u64>::some(val) });
    }
    // signed int
    TypeInt* ask_int_sized(u8 bytes) {
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .bytes=bytes, .val=Maybe<u64>::none() });
    }
    TypeInt* ask_int_const(u64 val) {
        // TODO hardcoded to 8 bytes
        return this->ask_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .bytes=8, .val=Maybe<u64>::some(val) });
    }
    // other int
    TypeInt* ask_int(TypeInt t) {
        s_type_int.add(t);
        return s_type_int.get(t);
    }

    // float
    TypeFloat* ask_float_sized(u8 bytes) {
        return this->ask_float(TypeFloat { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Float}, .bytes=bytes, .val=Maybe<f64>::none() });
    }
    TypeFloat* ask_int_const(f64 val) {
        // TODO hardcoded to 8 bytes
        return this->ask_float(TypeFloat { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Float}, .bytes=8, .val=Maybe<f64>::some(val) });
    }
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

    Type* from_str(Str name) {
        if(name == "u8"_s) return reinterpret_cast<Type*>(type::pool.ask_uint_sized(1));
        if(name == "i8"_s) return reinterpret_cast<Type*>(type::pool.ask_int_sized(1));
        if(name == "u16"_s) return reinterpret_cast<Type*>(type::pool.ask_uint_sized(2));
        if(name == "i16"_s) return reinterpret_cast<Type*>(type::pool.ask_int_sized(2));
        if(name == "u32"_s) return reinterpret_cast<Type*>(type::pool.ask_uint_sized(4));
        if(name == "i32"_s) return reinterpret_cast<Type*>(type::pool.ask_int_sized(4));
        if(name == "u64"_s) return reinterpret_cast<Type*>(type::pool.ask_uint_sized(8));
        if(name == "i64"_s) return reinterpret_cast<Type*>(type::pool.ask_int_sized(8));
        
        if(name == "f32"_s) return reinterpret_cast<Type*>(type::pool.ask_float_sized(4));
        if(name == "f64"_s) return reinterpret_cast<Type*>(type::pool.ask_float_sized(8));

        if(name == "bool"_s) return reinterpret_cast<Type*>(type::pool.ask_bool_any());
        if(name == "Str"_s) return reinterpret_cast<Type*>(type::pool.ask_str(TypeStr {.self=Type {.tinfo=TypeI::Top, .ttype=TypeT::Str}}));
        panic;
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
                else if(ty->val.here && ty->val.val == 0)
                    std::cout << "false";
                else if(ty->val.here && ty->val.val == 1)
                    std::cout << "true";
                else
                    std::cout << "<Bool:any>";
                return;
            }
            case TypeT::UInt: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<UInt:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<UInt:Bottom>";
                else if(ty->val.here)
                    std::cout << ty->val.val << "u";
                else
                    std::cout << "<UInt:" << (u64) ty->bytes << " bytes>";
                return;
            }
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<Int:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<Int:Bottom>";
                else if(ty->val.here)
                    std::cout << std::bit_cast<i64>(ty->val.val);
                else
                    std::cout << "<Int:" << (u64) ty->bytes << " bytes>";
                return;
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    std::cout << "<Float:Top>";
                else if(ty->self.tinfo == TypeI::Bottom) 
                    std::cout << "<Float:Bottom>";
                else if(ty->val.here)
                    std::cout << ty->val.val;
                else
                    std::cout << "<Float:" << (u64) ty->bytes << " bytes>";
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
                return ty->self.tinfo == TypeI::Known && ty->val.here;
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                return ty->self.tinfo == TypeI::Known && ty->val.here;
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

    Type* meet(Type* t1, Type* t2) {
        if(*t1 == *t2) return t1;
        return type::pool.ask( Type {.tinfo=TypeI::Bottom, .ttype=TypeT::Pure} );
    }
}