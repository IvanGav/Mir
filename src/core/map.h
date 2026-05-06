#pragma once

#include "prelude.h"
#include "mem.h"
#include "maybe.h"
#include "pair.h"
#include "vec.h"
#include "hash.h"
#include "bitset.h"

#define MAX_HIT_COUNT 0b01111111

template <typename K, typename V>
struct HMap {
    P<K,V>* set;
    BitSet exists;
    BitSet tombstone;
    u32 size;
    u32 capacity;

    static constexpr usize c1 = 0;
    static constexpr usize c2 = 1;

    mem::Arena* arena;

    static HMap create(mem::Arena& arena = default_arena) {
        HMap<K,V> m {};
        m.arena = &arena;
        return m;
    }

    void clear() {
        exists.clear();
        tombstone.clear();
    }

    bool empty() {
        return size == 0;
    }

    f32 load_factor() {
        return (f32) (size+1) / (f32) capacity;
    }

    void add(K& key, V& val) {
        if(this->load_factor() > 0.75) {
            this->resize();
        }
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index].a == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        if(!(exists[index])) {
            size++;
            tombstone.set(index);
            exists.set(index);
        }
        set[index] = P{key,val};
    }

    void remove(K& key) {
        if(capacity == 0) return;
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        u32 attempts = 1;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(; tombstone[index] && !(exists[index] && set[index].a == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        // if(!(exists[index])) { std::cout << "Element does not exist in the set" << std::endl; panic; } // cannot remove an element that doesn't exist
        if(exists[index]) {
            exists.unset(index);
        }
    }

    void resize() {
        if(arena == nullptr) { arena = &default_arena; }
        mem::Arena scratch = mem::Arena::create(10 MB);
        u32 old_capacity = capacity;
        P<K,V>* old_set = set;
        BitSet old_exists = exists.clone(&scratch);

        capacity = next_prime_size(capacity);
        set = arena->alloc<P<K,V>>(capacity);
        tombstone.clear();
        exists.clear();

        // copy all elements from the old set
        for(u32 i = 0; i < old_capacity; i++) {
            if(old_exists[i])
                this->add(old_set[i].a, old_set[i].b);
        }
    }

    /* Access Member Functions */

    V& operator[](K key) {
        assert(capacity > 0);
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index].a == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(exists[index]);
        return set[index].b;
    }

    V* get(K key) {
        assert(capacity > 0);
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index].a == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(exists[index]);
        return &set[index].b;
    }

    bool has(K& key) const {
        if(capacity == 0) return false;
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; tombstone[index] && !(exists[index] && set[index].a == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        return exists[index];
    }

    // linear lookup time
    Maybe<K> key_of(V& val) const {
        for(u32 i = 0; i < capacity; i++) {
            if(exists[i] && set[i].b == val) {
                return { .val = set[i].a, .here = true };
            }
        }
        return { .here = false };
    }

    /* Cloning */

    // if `new_arena` is `nullptr`, use the same arena as `this`
    HMap<K,V> clone(mem::Arena* new_arena = nullptr) {
        if(new_arena == nullptr) new_arena = arena;
        HMap<K,V> cloned = HMap<K,V> {
            .set = new_arena->alloc<P<K,V>>(capacity),
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

    Vec<P<K,V>> to_vec(mem::Arena* new_arena = nullptr) {
        if(new_arena == nullptr) new_arena = arena;
        Vec<P<K,V>> cloned = Vec<P<K,V>>::create(*new_arena);
        cloned.reserve(size);
        for(P<K,V>& item : *this) {
            cloned.push(item);
        }
        return cloned;
    }

    /* STL Compatibility */

    struct Iterator {
        HMap* hmap;
        u32 index;

        static Iterator create(HMap* hmap, u32 index) {
            Iterator i { .hmap = hmap, .index = index };
            i.skip_to_valid();
            return i;
        }

        void skip_to_valid() {
            for(; index < hmap->size && !hmap->exists[index]; index++);
        }

        P<K,V>& operator*() {
            return hmap->set[index];
        }

        P<K,V>* operator->() {
            return &hmap->set[index];
        }

        Iterator& operator++() {
            index++;
            skip_to_valid();
            return *this;
        }

        Iterator operator++(int) {
            Iterator old = *this;
            (*this)++;
            return old;
        }

        auto operator<=>(Iterator const& other) const = default;
    };

    Iterator begin() {
        return Iterator::create(this, 0);
    }

    Iterator end() {
        return Iterator::create(this, size);
    }
};