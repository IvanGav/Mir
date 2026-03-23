#pragma once

#include "prelude.h"
#include "mem.h"
#include "slice.h"

// any methods that accept size/capacity, will accept number of bits, not bytes or words
struct BitSet {
    typedef u8 bits;
    bits* data; // nullable, owned
    usize size; // real size of the array (not in bits)

    mem::Arena* arena;

    static BitSet create(mem::Arena& arena = default_arena) {
        BitSet s {};
        s.arena = &arena;
        return s;
    }

    void reserve(usize new_size) {
        new_size = ceil_div(new_size, sizeof(bits)*8);
        mem::Arena* arena = (this->arena == nullptr) ? &default_arena : this->arena;
        data = arena->realloc(data, size, new_size);
        size = new_size;
    }

    void set(usize num) {
        usize i = num/(sizeof(bits)*8);
        usize offset = num%(sizeof(bits)*8);
        if(i >= size) { this->reserve(next_power_of_two(i)); }
        data[i] |= (1 << offset);
    }

    void unset(usize num) {
        usize i = num/(sizeof(bits)*8);
        usize offset = num%(sizeof(bits)*8);
        if(i >= size) { this->reserve(next_power_of_two(i)); }
        data[i] &= ~(1 << offset);
    }

    void toggle(usize num) {
        if((*this)[num] == false) { this->set(num); }
        else { this->unset(num); }
    }

    /* Access Member Functions */

    bool operator[](usize i) const {
        usize bits_i = i/(sizeof(bits)*8);
        usize bits_offset = i%(sizeof(bits)*8);
        if(bits_i >= size) { return false; }
        return (data[bits_i] >> bits_offset) & 1;
    }

    /* Util functions */

    void clear() {
        size = 0;
    }

    /* Cloning */

    // if `new_arena` is `nullptr`, use the same arena as `this`
    // shallow clone **NOT DEEP CLONE**
    BitSet clone(mem::Arena* new_arena = nullptr) {
        if(new_arena == nullptr) new_arena = arena;
        BitSet cloned {};
        cloned.arena = new_arena;
        cloned.size = size;
        cloned.data = new_arena->alloc<bits>(size);
        mem::copy<bits>(cloned.data, data, size);
        return cloned;
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& os, BitSet& bitset) {
    os << '[';
    todo;
    os << ']';
    return os;
}