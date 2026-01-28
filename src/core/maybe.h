#pragma once

#include "prelude.h"

template <typename T>
struct Maybe {
    T val;
    bool here;

    T& get() {
        assert(here);
        return val;
    }

    static Maybe none() {
        return Maybe<T> {};
    }
    static Maybe some(T&& val) {
        return Maybe<T> { .val = val, .here = true };
    }
    static Maybe some(T& val) {
        return Maybe<T> { .val = val, .here = true };
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& os, Maybe<T>& m) {
    if(m.here) {
        return os << "Some(" << m.val << ")";
    }
    return os << "None";
}