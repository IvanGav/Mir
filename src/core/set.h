#pragma once

#include "prelude.h"
#include "mem.h"
#include "maybe.h"
#include "pair.h"
#include "vec.h"
#include "hash.h"

#define MAX_HIT_COUNT 0b01111111

// Hash Set (a bad implementation of one, at least)
// values must have == operator defined
template <typename T>
struct HSet {
    struct Entry {
        u8 flags; // 0bccccccce where e=exists flag, c=hit count bits (7 total)
        // u64 key_hash; // calculating the hash is expensive
        T val;

        bool exists() const { return flags & 1; }
        u8 hit_count() const { return flags >> 1; }
        void increment_hit_count() { assert(this->hit_count() < (MAX_HIT_COUNT)); flags += 2; }
        void decrement_hit_count() { assert(this->hit_count() > 0); flags -= 2; }
        void replace_with(T val) { this->flags |= 1; this->val = val; }
        void destroy() { this->flags &= ~(1); }
    };

    Vec<Entry> set;
    u32 size;
    u32 capacity;

    static constexpr usize c1 = 0;
    static constexpr usize c2 = 1;

    mem::Arena* arena;

    static HSet empty(mem::Arena& arena = default_arena) {
        HSet<T> m {};
        m.arena = &arena;
        return m;
    }

    bool empty() {
        return size == 0;
    }

    f32 load_factor() {
        return (f32) size / (f32) capacity;
    }

    void add(T val) {
        size++;
        if(this->load_factor() > 0.75) {
            this->resize();
        }
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; set[index].exists() && !(set[index].val == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        set[index].replace_with(val);
        if(init_index != index) {
            set[init_index].increment_hit_count();
        }
    }

    void resize() {
        Vec<Entry> old_set = set;
        this->set = Vec<Entry>::create(*arena); // create a new vec; it'd be difficult to rehash in-place
        this->capacity = next_prime_size(capacity);
        this->set.reserve(capacity);
        this->set.size = set.capacity;
        // copy all elements from the old set
        for(Entry& e : old_set) {
            // theoretically preserving element order may be beneficial? buuut
            if(e.exists())
                this->add(e.val);
        }
    }

    /* Access Member Functions */

    T const& operator[](T val) const {
        assert(capacity > 0);
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; set[index].exists() && !(set[index].val == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(set[index].exists());
        return set[index].val;
    }

    T const* get(T val) const {
        assert(capacity > 0);
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; set[index].exists() && !(set[index].val == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(set[index].exists());
        return &set[index].val;
    }
    
    T* get(T val) {
        assert(capacity > 0);
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; set[index].exists() && !(set[index].val == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(set[index].exists());
        return &set[index].val;
    }

    bool exists(T val) const {
        if(capacity == 0) return false;
        u64 hash = hash::from(val);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; set[index].exists() && !(set[index].val == val); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        return set[index].exists();
    }
};