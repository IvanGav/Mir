#pragma once

#include "prelude.h"
#include "mem.h"
#include "maybe.h"
#include "pair.h"
#include "vec.h"
#include "hash.h"
#include "bitset.h"

#define MAX_HIT_COUNT 0b01111111

// Hash Set (a bad implementation of one, at least)
// values must have == operator defined
template <typename T>
struct HSet {
    T* set;
    BitSet exists;
    BitSet tombstone;
    u32 size;
    u32 capacity;

    static constexpr usize c1 = 0;
    static constexpr usize c2 = 1;

    mem::Arena* arena;

    static HSet create(mem::Arena& arena = default_arena) {
        HSet<T> m {};
        m.arena = &arena;
        return m;
    }

    bool empty() {
        return size == 0;
    }

    f32 load_factor() {
        return (f32) (size+1) / (f32) capacity;
    }

    void add(T val) {
        if(this->load_factor() > 0.75) {
            this->resize();
        }
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index] == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        if(!(exists[index])) {
            size++;
            tombstone.set(index);
            exists.set(index);
        }
        set[index] = val;
    }

    void remove(T& val) {
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        u32 attempts = 1;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(; tombstone[index] && !(exists[index] && set[index] == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        if(!(exists[index])) { std::cout << "Element does not exist in the set" << std::endl; panic; } // cannot remove an element that doesn't exist
        exists.unset(index);
    }

    void resize() {
        if(arena == nullptr) { arena = &default_arena; }
        mem::Arena scratch = mem::Arena::create(10 MB);
        u32 old_capacity = capacity;
        T* old_set = set;
        BitSet old_exists = exists.clone(&scratch);

        capacity = next_prime_size(capacity);
        set = arena->alloc<T>(capacity);
        tombstone.clear();
        exists.clear();

        // copy all elements from the old set
        for(u32 i = 0; i < old_capacity; i++) {
            if(old_exists[i])
                this->add(old_set[i]);
        }
    }

    /* Access Member Functions */

    T const& operator[](T val) const {
        assert(capacity > 0);
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index] == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(exists[index]);
        return set[index];
    }

    T const* get(T val) const {
        todo;
        assert(capacity > 0);
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index] == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(exists[index]);
        return &set[index];
    }
    
    T* get(T val) {
        assert(capacity > 0);
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index] == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(exists[index]);
        return &set[index];
    }

    bool has(T val) const {
        if(capacity == 0) return false;
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index] == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        return exists[index];
    }

    /* Cloning */

    // if `new_arena` is `nullptr`, use the same arena as `this`
    HSet<T> clone(mem::Arena* new_arena = nullptr) {
        if(new_arena == nullptr) new_arena = arena;
        HSet<T> cloned = HSet<T> {
            .set = new_arena->alloc<T>(capacity),
            .exists = exists.clone(new_arena),
            .tombstone = tombstone.clone(new_arena),
            .size = size,
            .capacity = capacity,
            .arena = new_arena
        };
        mem::copy(cloned.set, set, capacity);
        assert(cloned.arena != nullptr);
        return cloned;
    }
};