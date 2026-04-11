#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

#define defhash (hash::from(t->tinfo) ^ hash::from(t->ttype) * 3)

namespace type {
    u64 hash(Type* t) {
        assert(t != nullptr);
        switch (t->ttype) {
            case TypeT::Pure:
            case TypeT::Ctrl: {
                return defhash;
            }

            case TypeT::Bool:
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                return defhash ^ 
                    std::rotl(hash::from(ty->val_min), 8) ^ std::rotl(hash::from(ty->val_max), 16);
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                return defhash ^ 
                    std::rotl(hash::from(ty->val_min), 8) ^ std::rotl(hash::from(ty->val_max), 16);
            }

            case TypeT::Tuple: {
                TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                return defhash ^ 
                    std::rotl(hash::from(ty->val), 16);
            }

            case TypeT::Mem:
            case TypeT::Ptr: {
                TypePtr* ty = reinterpret_cast<TypePtr*>(t);
                return defhash ^ 
                    (ty->ptr == nullptr ? 0 : std::rotl(type::hash(ty->ptr), 16)) ^
                    std::rotl(hash::from(ty->size), 48);
            }
        }
        unreachable;
    }
}