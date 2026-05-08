#pragma once

#include "prelude.h"

template <typename F, typename S>
struct P {
    F a;
    S b;

    bool operator<(P<F,S>& other) const {
        if(a == other.a) return b < other.b;
        return a < other.a;
    }
    bool operator==(P<F,S>& other) const {
        return a == other.a && b == other.b;
    }
};