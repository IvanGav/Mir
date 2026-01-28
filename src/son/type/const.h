#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

namespace type {
    bool constant(Type* t) {
        assert(t != nullptr);
        switch (t->ttype) {
            case TypeT::Pure: return false;
            case TypeT::Ctrl: return true; // TODO make sure this is right (i have a feeling it's not)

            case TypeT::Bool:
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                return ty->self.tinfo == TypeI::Known && ty->val_max == ty->val_min;
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                return ty->self.tinfo == TypeI::Known && ty->val_max == ty->val_min;
            }

            case TypeT::Tuple: {
                TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                return ty->self.tinfo == TypeI::Top; // TODO really?
            }
        }
        unreachable;
    }
}