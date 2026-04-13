// no optimizations please
// #define NOOPTS

#include "core/prelude.h"
#include "core/str.h"
#include "core/vec.h"
#include "core/slice.h"
#include "core/debug.h"
#include "core/map.h"

#include "son/parser.h"

#include "compile/dot.h"
#include "compile/graph_evaluator.h"

Str readFile(const char* path, mem::Arena& arena = default_arena) {
    std::ifstream infile(path);
    return str::clone_cstr(std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>()).data(), arena);
}

void writeFile(const char* path, Str content) {
    std::ofstream outfile(path);
    outfile << content;
}

int main(int argc, char* argv[]) {
    mem::Arena node_arena = mem::Arena::create(10 MB);
    mem::Arena scope_arena = mem::Arena::create(10 MB);
    mem::Arena type_arena = mem::Arena::create(10 MB);
    type::pool = TypePool::create(type_arena);
    Node::init(node_arena);
    Type* inputs[2] = { type::pool.ctrl, (Type*) type::pool.get_bottom(TypeT::Int) };
    START_NODE = NodeStart::create(Slice<Type*>::from_ptr(inputs, 2));
    STOP_NODE = NodeStop::create();
    SCOPE_NODE = NodeScope::create(scope_arena, NodeProj::create(0, START_NODE, true));
    SCOPE_NODE->define("arg"_s, NodeProj::create(1, START_NODE, false));
    SCOPE_NODE->define("$1"_s, NodeConst::create(type::pool.mem(type::pool.int_const(0)), START_NODE)); // manually load the alias class $1
    BREAK_SCOPE_NODE = CONTINUE_SCOPE_NODE = nullptr;
    
    // Str src;
    // if(argc > 1)
    //     src = readFile(argv[1]);
    // else
    //     src = readFile("mir/hello.mir");
    Str src = readFile("mir/hello.mir");

    Parser p = Parser::create(src);

    Node* n = nullptr;
    do {
        n = p.next_top_level_expr();
    } while(n != nullptr && !p.done());
    
    if(p.err()) {
        printd(p.error);
        std::cout << "at:\n" << p.t.source.slice(p.t.at, 10) << std::endl;
    }

    SCOPE_NODE->pop();
    // node::print_tree(START_NODE); // not very useful, when `dot` exists

    Str dot = compile::dot(START_NODE);
    writeFile("./graph.gv", dot);

    if(argc > 1) {
        u64 program_input = atoi(argv[1]);
        u64 output_value = Evaluator::create_and_run(START_NODE, program_input, 100000);
        std::cout << "Program output: " << output_value << std::endl;
    }
}