#pragma once

#include "prelude.h"
#include "mem.h"
#include "slice.h"

template <typename T>
struct Vec {
    T* data; // nullable, owned
    usize size;
    usize capacity;

    mem::Arena* arena;

    template <typename... Args>
    static Vec with(mem::Arena& arena, Args&&... args) {
        Vec<T> v {};
        v.arena = &arena;
        v.reserve(next_power_of_two(sizeof...(args)));
        (v.push(std::forward<Args>(args)), ...);
        return v;
    }

    template <typename... Args>
    static Vec with(Args&&... args) {
        Vec<T> v {};
        v.reserve(next_power_of_two(sizeof...(args)));
        (v.push(std::forward<Args>(args)), ...);
        return v;
    }

    static Vec create(mem::Arena& arena = default_arena) {
        Vec<T> v {};
        v.arena = &arena;
        return v;
    }

    static Vec clone_slice(Slice<T>& to_clone, mem::Arena& arena = default_arena) {
        Vec<T> v {};
        v.arena = &arena;
        v.reserve(to_clone.size);
        for(T& e : to_clone) {
            v.push(e);
        }
        return v;
    }

    void reserve(usize capacity) {
        mem::Arena* arena = (this->arena == nullptr) ? &default_arena : this->arena;
        data = arena->realloc(data, this->capacity, capacity);
        this->capacity = capacity;
    }
    
    bool empty() {
        return size == 0;
    }

    void push(T& e) {
        if(size == capacity) {
            if(capacity == 0) reserve(8);
            else reserve(capacity*2);
        }
        data[size] = e;
        size++;
    }

    T pop() {
        assert(size > 0);
        size--;
        return data[size];
    }

    /* Access Member Functions */

    T const& operator[](usize i) const {
        assert(i >= 0);
        assert(i < size);
        return data[i];
    }

    T const& front() const {
        assert(size > 0);
        return data[0];
    }

    T const& back() const {
        assert(size > 0);
        return data[size-1];
    }

    T& operator[](usize i) {
        assert(i >= 0);
        assert(i < size);
        return data[i];
    }

    T& front() {
        assert(size > 0);
        return data[0];
    }

    T& back() {
        assert(size > 0);
        return data[size-1];
    }

    /* Util functions */

    void reverse() {
        usize size = this->size;
        usize midpoint = size/2;
        for(usize i = 0; i < midpoint; i++) {
            mem::swap(&this->data[i], &this->data[size-1-i]);
        }
    }

    void clear() {
        size = 0;
    }

    // Remove an element at a given index
    void remove(usize remove_index) {
        assert(remove_index < size);
        size--;
        for(usize i = remove_index; i < size; i++) {
            data[i] = data[i+1];
        }
    }

    // Remove a single element by value. Return true if something was deleted.
    bool remove_first_of(T e) {
        // find the first occurance of e
        for(usize i = 0; i < size; i++) {
            if(data[i] == e) {
                this->remove(i);
                return true;
            }
        }
        return false;
    }

    /* Slice compatibility */

    Slice<T> full_slice() {
        return Slice<T> { .data = data, .size = size };
    }
    Slice<T> slice(usize start, usize size) {
        assert(start+size <= this->size);
        return Slice<T> { .data = data + start, .size = size };
    }
    // `end` exclusive
    Slice<T> slice_range(usize start, usize end) {
        assert(end <= this->size);
        return Slice<T> { .data = data + start, .size = size };
    }

    MutSlice<T> mut_full_slice() {
        return MutSlice<T> { .data = data, .size = size };
    }
    MutSlice<T> mut_slice(usize start, usize size) {
        assert(start+size <= this->size);
        return MutSlice<T> { .data = data + start, .size = size };
    }
    // `end` exclusive
    MutSlice<T> mut_slice_range(usize start, usize end) {
        assert(end <= this->size);
        return MutSlice<T> { .data = data + start, .size = size };
    }

    /* Cloning */

    // if `new_arena` is `nullptr`, use the same arena as `this`
    // shallow clone **NOT DEEP CLONE**
    Vec<T> clone(mem::Arena* new_arena = nullptr) {
        if(new_arena == nullptr) new_arena = arena;
        Vec<T> cloned {};
        cloned.arena = new_arena;
        cloned.capacity = capacity;
        cloned.size = size;
        cloned.data = new_arena->alloc<T>(capacity);
        mem::copy<T>(cloned.data, data, size);
        return cloned;
    }

    /* STL Compatibility */

    T* begin() {
        return data;
    }
    T* end() {
        return data + size;
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& os, Vec<T>& vec) {
    os << '[';
    for(T& e : vec) {
        os << e << ',';
    }
    os << ']';
    return os;
}