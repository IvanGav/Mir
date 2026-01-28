#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

namespace type {
    u64 hash(Type* t) {
        assert(t != nullptr);
        switch (t->ttype) {
            case TypeT::Pure:
            case TypeT::Ctrl: {
                return hash::from(t->tinfo) ^ hash::from(t->ttype) * 3;
            }

            case TypeT::Bool:
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                return (hash::from(t->tinfo) ^ hash::from(t->ttype) * 3) ^ 
                    std::rotl(hash::from(ty->val_min), 8) ^ std::rotl(hash::from(ty->val_max), 16);
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                return (hash::from(t->tinfo) ^ hash::from(t->ttype) * 3) ^ 
                    std::rotl(hash::from(ty->val_min), 8) ^ std::rotl(hash::from(ty->val_max), 16);
            }

            case TypeT::Tuple: {
                TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                
                return (hash::from(t->tinfo) ^ hash::from(t->ttype) * 3) ^ 
                    std::rotl(hash::from(ty->val), 16);
            }
        }
        unreachable;
    }
}