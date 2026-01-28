#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

namespace type {
    // "Intersection"
    Type* meet(Type* t1, Type* t2) {
        assert(t1 != nullptr);
        assert(t2 != nullptr);
        // meet of two unrelated types is always Pure:Bottom
        if(t1->ttype != t2->ttype) return pool.request(Type { .tinfo = TypeI::Bottom, .ttype = TypeT::Pure });

        // for all types, Top meet Something = Something
        if(t1->tinfo == TypeI::Top) return t2;
        if(t2->tinfo == TypeI::Top) return t1;
        // for all types, Something meet Bottom = Bottom
        if(t1->tinfo == TypeI::Bottom) return t1;
        if(t2->tinfo == TypeI::Bottom) return t2;
        
        switch (t1->ttype) {
            case TypeT::Pure: return t1;
            case TypeT::Ctrl: return t1;

            case TypeT::Bool:
            case TypeT::Int: {
                TypeInt* ti1 = reinterpret_cast<TypeInt*>(t1);
                TypeInt* ti2 = reinterpret_cast<TypeInt*>(t2);
                i64 min = max(ti1->val_min, ti2->val_min);
                i64 max = min(ti1->val_max, ti2->val_max);
                if(min > max)
                    return reinterpret_cast<Type*>(pool.get_int( TypeInt { .self = Type { .tinfo = TypeI::Bottom, .ttype = ti1->self.ttype } } ));
                return reinterpret_cast<Type*>(pool.get_int( TypeInt { .self = Type { .tinfo = TypeI::Known, .ttype = ti1->self.ttype }, .val_min = min, .val_max = max } ));
            }

            case TypeT::Float: {
                TypeFloat* tf1 = reinterpret_cast<TypeFloat*>(t1);
                TypeFloat* tf2 = reinterpret_cast<TypeFloat*>(t2);
                f64 min = max(tf1->val_min, tf2->val_min);
                f64 max = min(tf1->val_max, tf2->val_max);
                if(min > max)
                    return reinterpret_cast<Type*>(pool.get_float( TypeFloat { .self = Type { .tinfo = TypeI::Bottom, .ttype = tf1->self.ttype } } ));
                return reinterpret_cast<Type*>(pool.get_float( TypeFloat { .self = Type { .tinfo = TypeI::Known, .ttype = tf1->self.ttype }, .val_min = min, .val_max = max } ));
            }

            case TypeT::Tuple: {
                todo; // TODO literally *this* in chapter 4
            }
        }
        unreachable;
    }
}