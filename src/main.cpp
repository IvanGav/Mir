#include "core/prelude.h"
#include "core/str.h"
#include "core/vec.h"
#include "core/slice.h"
#include "core/debug.h"
#include "core/map.h"

#include "token/tokenizer.h"
#include "token/debug.h"

Str readFile(const char* path, mem::Arena& arena = default_arena) {
    std::ifstream infile(path);
    return str::clone_cstr(std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>()).data(), arena);
}

enum class Test {
    one, two, three, four, five, six, seven, eight, nine, ten, eleven, twelve, factorio
};

int main() {
    mem::Arena node_arena = mem::Arena::create(1 MB);
    mem::Arena arena = mem::Arena::create(1 KB);
    Str src = readFile("src.mir");
    Tokenizer p { .source = src };
    while(!p.eof()) {
        std::cout << p.next_token() << std::endl;
    }

    // HMap<u32, u32> m = HMap<u32, u32>::empty(arena);
    // m.add(1, 10);
    // m.add(2, 'a');
    // m.add(54, 100);
    // m.add(107, 101);
    // std::cout << "(" << (u16) m[54] << ")" << std::endl;
    // std::cout << "(" << (u16) m[1] << ")" << std::endl;
    // std::cout << "(" << (u16) m[2] << ")" << std::endl;
    // std::cout << "(" << (u16) m[107] << ")" << std::endl;

    // HMap<Test, u32> m = HMap<Test, u32>::empty(arena);
    // m.add(Test::one, 10);
    // m.add(Test::factorio, 'a');
    // m.add(Test::twelve, 100);
    // m.add(Test::factorio, 101);
    // std::cout << "(" << m[Test::twelve] << ")" << std::endl;
    // std::cout << "(" << m[Test::one] << ")" << std::endl;
    // std::cout << "(" << m[Test::factorio] << ")" << std::endl;
}