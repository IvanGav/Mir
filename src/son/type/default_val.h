#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

namespace type {
    Type* default_val(Type* t) {
        switch (t->ttype) {
            case TypeT::Pure:
            case TypeT::Ctrl: todo;

            case TypeT::Bool:
            case TypeT::Int: {
                return type::pool.int_const(0);
            }

            case TypeT::Float: {
                todo;
            }

            case TypeT::Tuple: {
                todo;
            }

            case TypeT::Mem:
            case TypeT::Ptr: {
                TypePtr* tp = (TypePtr*) t;
                return type::pool.ptr_null(tp->size);
            }
        }
        unreachable;
    }
}