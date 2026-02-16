#include "core/prelude.h"
#include "core/str.h"
#include "core/vec.h"
#include "core/slice.h"
#include "core/debug.h"
#include "core/map.h"

#include "son/parser.h"

Str readFile(const char* path, mem::Arena& arena = default_arena) {
    std::ifstream infile(path);
    return str::clone_cstr(std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>()).data(), arena);
}

int main(int argc, char* argv[]) {
    mem::Arena node_arena = mem::Arena::create(10 MB);
    mem::Arena scope_arena = mem::Arena::create(10 MB);
    Node::init(node_arena);
    // Type* inputs[2] = { type::pool.ctrl(), (Type*) type::pool.bottom(TypeT::Int) };
    Type* inputs[2] = { type::pool.ctrl(), (Type*) type::pool.int_const(5) };
    START_NODE = NodeStart::create(Slice<Type*>::from_ptr(inputs, 2));
    SCOPE_NODE = (NodeScope*) NodeScope::create(scope_arena, NodeProj::create(0, START_NODE));
    SCOPE_NODE->define("arg"_s, NodeProj::create(1, START_NODE));
    
    Str src;
    if(argc > 1)
        src = readFile(argv[1]);
    else
        src = readFile("mir/hello.mir");

    Parser p = Parser::create(src);

    Node* n = nullptr;
    do {
        n = p.next_top_level_expr();
        // if(n == nullptr) { printd("nullptr"); } else { printd(n); }
    } while(n != nullptr && n->nt != NodeType::Ret);

    SCOPE_NODE->pop();

    if(p.err()) printd(p.error);

    node::print_tree(START_NODE);
}