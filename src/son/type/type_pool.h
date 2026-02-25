#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"

struct TypePool {
    mem::Arena* type_arena;

    HSet<Type> s_type;
    HSet<TypeInt> s_type_int;
    HSet<TypeFloat> s_type_float;
    HSet<TypeTuple> s_type_tuple;

    Type* request(Type t) {
        s_type.add(t);
        assert(s_type.get(t) != nullptr);
        return s_type.get(t);
    }
    Type* bottom() {
        return this->request(Type { .tinfo=TypeI::Bottom, .ttype=TypeT::Pure });
    }
    Type* top() {
        return this->request(Type { .tinfo=TypeI::Top, .ttype=TypeT::Pure });
    }
    Type* ctrl() {
        return this->request(Type { .tinfo=TypeI::Bottom, .ttype=TypeT::Ctrl });
    }

    Type* bottom(TypeT tt) {
        switch(tt) {
            case TypeT::Pure: case TypeT::Ctrl: return this->request(Type { .tinfo=TypeI::Bottom, .ttype=tt });
            case TypeT::Bool: case TypeT::Int:  return (Type*) this->get_int(TypeInt { .self = Type { .tinfo=TypeI::Bottom, .ttype=tt } });
            case TypeT::Float:                  return (Type*) this->get_float(TypeFloat { .self = Type { .tinfo=TypeI::Bottom, .ttype=tt } });
            case TypeT::Tuple:                  return (Type*) this->get_tuple(TypeTuple { .self = Type { .tinfo=TypeI::Bottom, .ttype=tt } });
        }
        unreachable;
    }
    Type* top(TypeT tt) {
        switch(tt) {
            case TypeT::Pure: case TypeT::Ctrl: return this->request(Type { .tinfo=TypeI::Top, .ttype=tt });
            case TypeT::Bool: case TypeT::Int:  return (Type*) this->get_int(TypeInt { .self = Type { .tinfo=TypeI::Top, .ttype=tt } });
            case TypeT::Float:                  return (Type*) this->get_float(TypeFloat { .self = Type { .tinfo=TypeI::Top, .ttype=tt } });
            case TypeT::Tuple:                  return (Type*) this->get_tuple(TypeTuple { .self = Type { .tinfo=TypeI::Top, .ttype=tt } });
        }
        unreachable;
    }
    
    // bool
    Type* bool_any() {
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 0, .val_max = 1 });
    }
    Type* bool_false() {
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 0, .val_max = 0 });
    }
    Type* bool_true() {
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 1, .val_max = 1 });
    }
    // int
    Type* int_sized(u8 bytes) {
        i64 max, min;
        switch(bytes) {
            case 1: max = I8_MAX; min = I8_MIN; break;
            case 2: max = I16_MAX; min = I16_MIN; break;
            case 4: max = I32_MAX; min = I32_MIN; break;
            case 8: max = I64_MAX; min = I64_MIN; break;
            default: panic;
        }
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = std::bit_cast<i64>(min), .val_max = std::bit_cast<i64>(max)});
    }
    Type* int_const(i64 val) {
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = std::bit_cast<i64>(val), .val_max = std::bit_cast<i64>(val) });
    }
    Type* get_int(TypeInt t) {
        s_type_int.add(t);
        assert(s_type_int.get(t) != nullptr);
        return (Type*) s_type_int.get(t);
    }

    // float
    Type* get_float(TypeFloat t) {
        s_type_float.add(t);
        assert(s_type_float.get(t) != nullptr);
        return (Type*) s_type_float.get(t);
    }

    // tuple
    Type* get_tuple(TypeTuple t) {
        // Be a bit careful here
        // TypeTuple contains a slice, which may not be valid forever
        // if doesn't exist, deep clone; retrieve otherwise
        // hash and operator== on slices are deep, so no need to worry there
        if(!s_type_tuple.exists(t)) {
            Slice<Type*> slice_deep_clone = Vec<Type*>::clone_slice(t.val, *type_arena).full_slice();
            TypeTuple deep_clone = TypeTuple { .self = t.self, .val = slice_deep_clone};
            s_type_tuple.add(deep_clone);
            // std::cout << "Deep: " << deep_clone.self.hash() << std::endl;
            // std::cout << "Given:" << t.self.hash() << std::endl;
            // s_type_tuple.get(deep_clone);
            // s_type_tuple.get(t);
            // std::cout << "__________" << std::endl;
        }
        assert(s_type_tuple.get(t) != nullptr);
        return (Type*) s_type_tuple.get(t);
    }
    
    Type* from_slice(Slice<Type*> args) {
        return (Type*) this->get_tuple(TypeTuple { .self = Type::known(TypeT::Tuple), .val = args });
    }
};

namespace type {
    static TypePool pool;
};