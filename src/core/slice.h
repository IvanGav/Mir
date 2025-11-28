#pragma once

#include "prelude.h"

// Immutable slice on contiguous memory
template <typename T>
struct Slice {
    T* data; //nonull
    usize size; // non resizable

    T const& operator[](usize i) const {
        assert(i >= 0);
        assert(i < size);
        return data[i];
    }

    bool operator==(const Slice& other) const {
        if (size != other.size) return false;
        for (usize i = 0; i < size; ++i)
            if (!(data[i] == other.data[i])) return false;
        return true;
    }

    Slice<T> slice(usize start, usize size) {
        assert(start+size <= this->size);
        return Slice<T> { .data = data + start, .size = size };
    }
    // `end` exclusive
    Slice<T> slice_range(usize start, usize end) {
        assert(end <= this->size);
        return Slice<T> { .data = data + start, .size = end - start };
    }

    /* STL Compatibility */

    T* begin() {
        return data;
    }
    T* end() {
        return data + size;
    }
};

// Mutable slice on contiguous memory
template <typename T>
struct MutSlice : Slice<T> {
    T& operator[](int i) {
        assert(i >= 0);
        assert(i < this->size);
        return this->data[i];
    }

    MutSlice<T> mut_slice(usize start, usize size) {
        assert(start+size <= this->size);
        return MutSlice<T> { .data = this->data + start, .size = size };
    }
    // `end` exclusive
    MutSlice<T> mut_slice_range(usize start, usize end) {
        assert(end <= this->size);
        return MutSlice<T> { .data = this->data + start, .size = end - start };
    }
};