#include "core/prelude.h"
#include "core/str.h"
#include "core/vec.h"
#include "core/slice.h"
#include "core/debug.h"
#include "core/map.h"

#include "token/tokenizer.h"
#include "token/debug.h"

#include "ast/node.h"
#include "ast/parser.h"

Str readFile(const char* path, mem::Arena& arena = default_arena) {
    std::ifstream infile(path);
    return str::clone_cstr(std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>()).data(), arena);
}

int main() {
    mem::Arena node_arena = mem::Arena::create(1 MB);
    Str src = readFile("mir/simple2.mir");

    // Tokenizer t = Tokenizer::create(src);
    // while(!t.eof()) {
    //     std::cout << t.next_token() << std::endl;
    // }

    Parser p = Parser::create(src, &node_arena);
    while(!p.done()) {
        Node* expr = p.next_expr();
        if(expr == nullptr) break;
        expr->debug_print(); std::cout << "\n";
    }
}