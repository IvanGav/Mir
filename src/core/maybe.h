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

    int operator==(const Maybe<T>& rhs) const {
        if(here == rhs.here) {
            return val == rhs.val;
        }
        return false;
    }
};