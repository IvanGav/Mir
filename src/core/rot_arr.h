#pragma once

#include "prelude.h"
#include "mem.h"

template <typename T, u32 S>
struct RotArr {
    T arr[S];
    u32 at;
    u32 size;

    bool empty() {
        return size == 0;
    }

    void push(T&& val) {
        assert(size < S);
        arr[at] = val;
        at = (at+1)%S;
        size++;
    }

    T pop() {
        assert(size > 0);
        size--;
        T to_ret = data[at];
        if(at == 0) at = S;
        at--;
        return to_ret;
    }

    /* Access Member Functions */

    // T const& operator[](usize i) const {
    //     assert(i >= 0);
    //     assert(i < size);
    //     return data[(at+i)%S];
    // }

    // T const& front() const {
    //     assert(size > 0);
    //     return data[(at-size)%S];
    // }

    // T const& back() const {
    //     assert(size > 0);
    //     return data[size-1];
    // }
};