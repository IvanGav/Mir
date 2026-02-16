#pragma once

#include "prelude.h"
#include "mem.h"
#include "maybe.h"
#include "pair.h"
#include "vec.h"
#include "hash.h"

#define MAX_HIT_COUNT 0b01111111

// Hash Map (a bad implementation of one, at least)
// keys must have == operator defined
template <typename K, typename V>
struct HMap {
    struct Entry {
        u8 flags; // 0bccccccce where e=exists flag, c=hit count bits (7 total)
        // u64 key_hash; // calculating the hash is expensive
        K key;
        V value;

        bool exists() const { return flags & 1; }
        u8 hit_count() const { return flags >> 1; }
        void increment_hit_count() { assert(this->hit_count() < (MAX_HIT_COUNT)); flags += 2; }
        void decrement_hit_count() { assert(this->hit_count() > 0); flags -= 2; }
        void replace_with(K key, V val) { this->flags |= 1; this->key = key; this->value = val; }
        void destroy() { this->flags &= ~(1); }
    };

    Vec<Entry> map;
    u32 size;
    u32 capacity;

    static constexpr usize c1 = 0;
    static constexpr usize c2 = 1;

    mem::Arena* arena;

    static HMap empty(mem::Arena* arena = &default_arena) {
        HMap<K,V> m {};
        m.arena = arena;
        return m;
    }

    bool empty() {
        return size == 0;
    }

    f32 load_factor() {
        return (f32) size / (f32) capacity;
    }

    void add(K key, V val) {
        size++;
        if(this->load_factor() > 0.75) {
            this->resize();
        }
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; map[index].exists() && !(map[index].key == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        map[index].replace_with(key, val);
        if(init_index != index) {
            map[init_index].increment_hit_count();
        }
    }

    void resize() {
        Vec<Entry> old_map = map;
        this->map = Vec<Entry>::create(*arena); // create a new vec; it'd be difficult to rehash in-place
        this->capacity = next_prime_size(capacity);
        this->map.reserve(capacity);
        this->map.size = map.capacity;
        // copy all elements from the old map
        for(Entry& e : old_map) {
            // theoretically preserving element order may be beneficial? buuut
            if(e.exists())
                this->add(e.key, e.value);
        }
    }

    void clear() {
        for(Entry& e : map) {
            e.flags = 0; // full reset
        }
    }

    /* Access Member Functions */

    V const& operator[](K key) const {
        assert(capacity > 0);
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; map[index].exists() && !(map[index].key == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(map[index].exists());
        return map[index].value;
    }

    V& operator[](K key) {
        assert(capacity > 0);
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; map[index].exists() && !(map[index].key == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        assert(map[index].exists());
        return map[index].value;
    }

    bool exists(K key) const {
        if(capacity == 0) return false;
        u64 hash = hash::from(key);
        usize init_index = hash%capacity;
        usize index = init_index;
        // find the next available spot using quadratic probing, if initial is taken (or do nothing otherwise)
        for(u32 attempts = 1; map[index].exists() && !(map[index].key == key); attempts++) {
            index = (init_index + c1 * attempts + c2 * attempts * attempts)%capacity;
        }
        return map[index].exists();
    }

    /* Cloning */

    // if `new_arena` is `nullptr`, use the same arena as `this`
    HMap<K, V> clone(mem::Arena* new_arena = nullptr) {
        if(new_arena == nullptr) new_arena = arena;
        HMap<K, V> cloned {};
        cloned.size = size;
        cloned.capacity = capacity;
        cloned.map = map.clone(new_arena);
        return cloned;
    }
};