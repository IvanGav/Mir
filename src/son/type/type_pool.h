#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"

struct TypePool {
    mem::Arena* type_arena;

    Type* bottom; Type* top; Type* ctrl; Type* xctrl;
    HSet<TypeInt> s_type_int;
    HSet<TypeFloat> s_type_float;
    HSet<TypeTuple> s_type_tuple;
    HSet<TypePtr> s_type_ptr;

    static TypePool create(mem::Arena& type_arena) {
        return TypePool {
            .type_arena = &type_arena,
            .bottom =   type_arena.push<Type>({ .tinfo=TypeI::Bottom, .ttype=TypeT::Pure }),
            .top =      type_arena.push<Type>({ .tinfo=TypeI::Top, .ttype=TypeT::Pure }),
            .ctrl =     type_arena.push<Type>({ .tinfo=TypeI::Bottom, .ttype=TypeT::Ctrl }),
            .xctrl =    type_arena.push<Type>({ .tinfo=TypeI::Top, .ttype=TypeT::Ctrl }),
            .s_type_int =   HSet<TypeInt>::create(type_arena),
            .s_type_float = HSet<TypeFloat>::create(type_arena),
            .s_type_tuple = HSet<TypeTuple>::create(type_arena),
            .s_type_ptr =   HSet<TypePtr>::create(type_arena)
        };
    }

    Type* get_bottom(TypeT tt) {
        Type t = Type { .tinfo=TypeI::Bottom, .ttype=tt };
        switch(tt) {
            case TypeT::Pure:   return bottom;
            case TypeT::Ctrl:   return ctrl;
            case TypeT::Bool: 
            case TypeT::Int:    return (Type*) this->get_int(TypeInt { .self = t });
            case TypeT::Float:  return (Type*) this->get_float(TypeFloat { .self = t });
            case TypeT::Tuple:  return (Type*) this->get_tuple(TypeTuple { .self = t });
            case TypeT::Ptr:    return (Type*) this->get_ptr(TypePtr { .self = t, .ptr = top });
            case TypeT::Mem:    return (Type*) this->get_ptr(TypePtr { .self = t, .ptr = top });
        }
        unreachable;
    }
    Type* get_top(TypeT tt) {
        Type t = Type { .tinfo=TypeI::Top, .ttype=tt };
        switch(tt) {
            case TypeT::Pure:   return top;
            case TypeT::Ctrl:   return xctrl;
            case TypeT::Bool: 
            case TypeT::Int:    return (Type*) this->get_int(TypeInt { .self = t });
            case TypeT::Float:  return (Type*) this->get_float(TypeFloat { .self = t });
            case TypeT::Tuple:  return (Type*) this->get_tuple(TypeTuple { .self = t });
            case TypeT::Ptr:    return (Type*) this->get_ptr(TypePtr { .self = t, .ptr = bottom });
            case TypeT::Mem:    return (Type*) this->get_ptr(TypePtr { .self = t, .ptr = bottom });
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
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = val, .val_max = val });
    }
    Type* int_range(i64 min, i64 max) {
        return (Type*) this->get_int(TypeInt { .self = Type {.tinfo = TypeI::Known, .ttype = TypeT::Int}, .val_min = min, .val_max = max });
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
        if(!s_type_tuple.has(t)) {
            Slice<Type*> slice_deep_clone = Vec<Type*>::clone_slice(t.val, *type_arena).full_slice();
            TypeTuple deep_clone = TypeTuple { .self = t.self, .val = slice_deep_clone};
            s_type_tuple.add(deep_clone);
        }
        assert(s_type_tuple.get(t) != nullptr);
        return (Type*) s_type_tuple.get(t);
    }
    
    Type* from_slice(Slice<Type*> args) {
        return (Type*) this->get_tuple(TypeTuple { .self = Type::known(TypeT::Tuple), .val = args });
    }

    // pottier
    Type* get_ptr(TypePtr t) {
        assert(t.ptr != nullptr);
        s_type_ptr.add(t);
        assert(s_type_ptr.get(t) != nullptr);
        return (Type*) s_type_ptr.get(t);
    }
    Type* ptr_to(Type* t, u32 size) {
        return this->get_ptr(TypePtr { .self = { .tinfo = TypeI::Known, .ttype = TypeT::Ptr }, .ptr = t, .size = size });
    }
    Type* ptr_null(u32 size) {
        return this->get_ptr(TypePtr { .self = { .tinfo = TypeI::Top, .ttype = TypeT::Ptr }, .ptr = this->bottom, .size = size });
    }
    Type* mem(Type* t) {
        return this->get_ptr(TypePtr { .self = { .tinfo = TypeI::Known, .ttype = TypeT::Mem }, .ptr = t, .size = 1 });
    }
};

namespace type {
    static TypePool pool;
};