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

    static Vec empty(mem::Arena& arena = default_arena) {
        Vec<T> v {};
        v.arena = &arena;
        return v;
    }

    // ~Vec() { if(data != nullptr) arena->free(data); }

    void reserve(usize capacity) {
        mem::Arena* arena = (this->arena == nullptr) ? &default_arena : this->arena;
        data = arena->realloc(data, this->capacity, capacity);
        this->capacity = capacity;
    }
    
    bool empty() {
        return size == 0;
    }

    void push(T&& e) {
        if(size == capacity) {
            if(capacity == 0) reserve(8);
            else reserve(capacity*2);
        }
        data[size] = e;
        size++;
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

    /* Slice compatibility */

    Slice<T> slice(usize start, usize size) {
        assert(start+size <= this->size);
        return Slice<T> { .data = data + start, .size = size };
    }
    // `end` exclusive
    Slice<T> slice_range(usize start, usize end) {
        assert(end <= this->size);
        return Slice<T> { .data = data + start, .size = size };
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

    /* STL Compatibility */

    T* begin() {
        return data;
    }
    T* end() {
        return data + size;
    }
};