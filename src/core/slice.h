#pragma once

#include "prelude.h"
#include "hash.h"

// Immutable slice on contiguous memory
template <typename T>
struct Slice {
    T* data; //nonull
    usize size; // non resizable

    static Slice<T> from_ptr(T* ptr, usize size) {
        return Slice<T> { .data = ptr, .size = size };
    }

    T const& operator[](usize i) const {
        assert(i >= 0);
        assert(i < size);
        return data[i];
    }

    bool operator==(const Slice& other) const {
        // requires (!std::is_pointer_v<T>) {
        if (size != other.size) return false;
        for (usize i = 0; i < size; ++i) {
            if (!(data[i] == other.data[i])) return false;
        }
        return true;
    }

    // // deep compare by pointed-to value (*a[i] == *b[i])
    // bool operator==(const Slice& other) const
    //     requires (std::is_pointer_v<T>) {
    //     if (size != other.size) return false;
    //     for (usize i = 0; i < size; i++) {
    //         T a = data[i];
    //         T b = other.data[i];
    //         if (a == nullptr || b == nullptr) {
    //             if (a != b) return false;
    //             continue;
    //         }
    //         if (!(*a == *b)) return false;
    //     }
    //     return true;
    // }

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

    /* hash Compatibility */

    u64 hash() {
        u64 acc_hash = 0;
        for(usize i = 0; i < size; i++) {
            acc_hash ^= std::rotl(hash::from(data[i]), i);
        }
        return acc_hash;
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