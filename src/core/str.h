#pragma once

#include "prelude.h"
#include "mem.h"
#include "slice.h"
#include "vec.h"

typedef Slice<u8> Str;

namespace str {
    Str clone_str(Str&& other, mem::Arena& arena = default_arena) {
        return Str { .data = arena.clone(other.data, other.size), .size = other.size };
    }
    Str clone_str(Str& other, mem::Arena& arena = default_arena) {
        return Str { .data = arena.clone(other.data, other.size), .size = other.size };
    }

    Str clone_cstr(const char* cstr, usize size, mem::Arena& arena = default_arena) {
        Str s = Str { .size = size };
        s.data = arena.clone((u8*) cstr, s.size);
        return s;
    }
    Str clone_cstr(const char* cstr, mem::Arena& arena = default_arena) {
        return str::clone_cstr(cstr, strlen(cstr), arena);
    }

    Str from_cstr(const char* cstr, usize size) {
        return Str { .data = (u8*) cstr, .size = size };
    }
    Str from_cstr(const char* cstr) {
        return str::from_cstr(cstr, strlen(cstr));
    }

    // Clone; Save a snapshot of this string builder
    Str from_vec_u8(Vec<u8>& v, mem::Arena& arena = default_arena) {
        return str::clone_str(v.slice(0, v.size), arena);
    }

    Str from_slice_of_str(Slice<Str>& v, mem::Arena& arena = default_arena) {
        usize comb_size = 0;
        for(Str& s : v) comb_size += s.size;
        Str combined = Str { .size = comb_size };
        combined.data = arena.alloc<u8>(comb_size);

        usize accumulated = 0;
        for(Str& s : v) {
            mem::copy(combined.data + accumulated, s.data, s.size);
            accumulated += s.size;
        }
        return combined;
    }

    template <typename T>
    Str from_int(T num, mem::Arena& arena = default_arena) {
        Vec<u8> v {.arena = &arena};
        while (num > 0) { v.push(num % 10 + '0'); num /= 10; }
        v.reverse();
        return v.slice(0,v.size);
    }

    template <typename T>
    T to_int(Str str) {
        T num = 0;
        for(u8 i : str) {
            num *= 10;
            num += (i-'0');
        }
        return num;
    }

    std::string to_cppstr(Str str) {
        return std::string((const char*) str.data, str.size);
    }
};

Str operator ""_s(const char* s, long unsigned int len) {
    return str::from_cstr(s, len);
}

std::ostream& operator<<(std::ostream& os, const Str& str) {
    os.write((const char*) str.data, (std::streamsize) str.size);
    return os;
}