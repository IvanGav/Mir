#pragma once

#include "prelude.h"
#include "mem.h"

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

    // size = in sizeof(bits), not bits
    void reserve(usize new_size) {
        mem::Arena* arena = (this->arena == nullptr) ? &default_arena : this->arena;
        data = arena->realloc(data, size, new_size);
        mem::zero(data+size, new_size-size); // zero out the new bytes
        size = new_size;
    }

    // num = number of bits to reserve for
    void reserve_bits(usize num) {
        usize i = ceil_div(num,(sizeof(bits)*8));
        if(i >= size) { this->reserve(i); }
    }

    void set(usize num) {
        usize i = num/(sizeof(bits)*8);
        usize offset = num%(sizeof(bits)*8);
        if(i >= size) { this->reserve(next_power_of_two(i+1)); }
        data[i] |= (1 << offset);
    }

    void unset(usize num) {
        usize i = num/(sizeof(bits)*8);
        usize offset = num%(sizeof(bits)*8);
        if(i >= size) { this->reserve(next_power_of_two(i+1)); }
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
        // size = 0;
        mem::zero(data, size);
    }

    /* Cloning */

    // if `new_arena` is `nullptr`, use the same arena as `this`
    BitSet clone(mem::Arena* new_arena = nullptr) {
        new_arena = (new_arena == nullptr ? this->arena : new_arena);
        BitSet newbs {
            .data = new_arena->alloc<bits>(size),
            .size = size,
            .arena = new_arena,
        };
        mem::copy<bits>(newbs.data, data, size);
        return newbs;
    }
};

std::ostream& operator<<(std::ostream& os, BitSet& bitset) {
    os << '[';
    for(usize i = 0; i < bitset.size * sizeof(BitSet::bits) * 8; i++) {
        os.put(bitset[i] ? '1' : '0');
    }
    os << ']';
    return os;
}