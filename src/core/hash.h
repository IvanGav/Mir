#pragma once

#include "prelude.h"
#include "slice.h"

#define hash_int(type) u64 from(type int_t) { return (u64) int_t; }

namespace hash {
    hash_int(u8); hash_int(i8);
    hash_int(u16); hash_int(i16);
    hash_int(u32); hash_int(i32);
    hash_int(u64); hash_int(i64);
    // hash_int(usize); // apparently same as u64 on my machine...
    
    template <typename T>
    u64 from(Slice<T> s) {
        u64 acc_hash = 0;
        for(usize i = 0; i < s.size; i++) {
            acc_hash ^= std::rotl(hash::from(s[i]), i);
        }
        return acc_hash;
    }

    template <typename T>
    concept EnumClassConcept = std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>; // I have no idea either
    template <EnumClassConcept T>
    u64 from(T e) {
        return hash::from((usize) e);
    }

    template <typename T>
    u64 from(T s) {
        return s.hash();
    }
}