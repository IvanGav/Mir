#pragma once

#include "prelude.h"
#include "str.h"

std::ostream& operator<<(std::ostream& os, const Str& str) {
    os.write((const char*) str.data, (std::streamsize) str.size);
    return os;
}