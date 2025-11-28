#pragma once

#include "prelude.h"
#include "str.h"

// void print(Str&& str) {
//    write(fileno(stdout), (void*) str.data, (size_t) str.size);
// }

// void print(Str& str) {
//    write(fileno(stdout), (void*) str.data, (size_t) str.size);
// }

// void println(Str&& str) {
//    print(str);
//    write(fileno(stdout), &"\n", (size_t) 1);
// }

// void println(Str& str) {
//    print(str);
//    write(fileno(stdout), &"\n", (size_t) 1);
// }

std::ostream& operator<<(std::ostream& os, const Str& str) {
    os.write((const char*) str.data, (std::streamsize) str.size);
    return os;
}