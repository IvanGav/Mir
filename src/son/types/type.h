#pragma once

#include "../../core/prelude.h"

enum class TypeClass {
    Top, Bottom, Const
};

enum class TypeEnum {
    None, Int, Float, Tuple
};

struct Type {
    TypeEnum t;
};