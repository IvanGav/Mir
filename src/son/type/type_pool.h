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
    TypeInt* bool_any() {
        return this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 0, .val_max = 1 });
    }
    TypeInt* bool_false() {
        return this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 0, .val_max = 0 });
    }
    TypeInt* bool_true() {
        return this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Bool}, .val_min = 1, .val_max = 1 });
    }
    // int
    TypeInt* int_sized(u8 bytes) {
        i64 max, min;
        switch(bytes) {
            case 1: max = I8_MAX; min = I8_MIN; break;
            case 2: max = I16_MAX; min = I16_MIN; break;
            case 4: max = I32_MAX; min = I32_MIN; break;
            case 8: max = I64_MAX; min = I64_MIN; break;
            default: panic;
        }
        return this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = std::bit_cast<i64>(min), .val_max = std::bit_cast<i64>(max)});
    }
    TypeInt* int_const(i64 val) {
        return this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = std::bit_cast<i64>(val), .val_max = std::bit_cast<i64>(val) });
    }
    TypeInt* get_int(TypeInt t) {
        s_type_int.add(t);
        return s_type_int.get(t);
    }

    // float
    TypeFloat* get_float(TypeFloat t) {
        s_type_float.add(t);
        return s_type_float.get(t);
    }

    // tuple
    TypeTuple* get_tuple(TypeTuple t) {
        // Be a bit careful here
        // TypeTuple contains a slice, which may not be valid forever
        // if doesn't exist, deep clone; retrieve otherwise
        // hash and operator== on slices are deep, so no need to worry there
        if(!s_type_tuple.exists(t)) {
            Slice<Type*> slice_deep_clone = Vec<Type*>::clone_slice(t.val, *type_arena).full_slice();
            TypeTuple deep_clone = TypeTuple { .self = t.self, .val = slice_deep_clone};
            s_type_tuple.add(deep_clone);
        }
        return s_type_tuple.get(t);
    }
};

namespace type {
    static TypePool pool;
};