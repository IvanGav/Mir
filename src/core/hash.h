#pragma once

#include "prelude.h"

// #define hash_int(type) u64 from(type int_t) { return (u64) int_t; }

// Does not support unions
namespace hash {
    template <std::integral T>
    u64 from(T i) {
        return (u64) i;
    }

    // template <std::floating_point T>
    // u64 from(T i) {
    //     return (u64) std::bit_cast(i);
    // }

    u64 from(f32 f) {
        return (u64) std::bit_cast<f32>(f);
    }

    u64 from(f64 f) {
        return (u64) std::bit_cast<f64>(f);
    }

    // hash_int(u8); hash_int(i8); hash_int(u16); hash_int(i16); hash_int(u32); hash_int(i32); hash_int(u64); hash_int(i64); hash_int(usize); // apparently same as u64 on my machine...

    template <typename T>
    concept EnumClassConcept = std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>; // I have no idea either

    template <EnumClassConcept T>
    u64 from(T e) {
        return hash::from((usize) e);
    }

    template <typename T>
    concept PointerConcept = std::is_pointer_v<T>;

    template <PointerConcept T>
    u64 from(T s) {
        return s->hash();
    }

    template <typename T>
    u64 from(T s) {
        return s.hash();
    }
}