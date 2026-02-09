#pragma once

#include "../../core/prelude.h"
#include "../../core/vec.h"
#include "../../core/pair.h"
#include "../../core/maybe.h"

#include "../../lang/op.h"
#include "../type.h"

#include "../../token/tokenizer.h"

enum class NodeType {
    Undefined,
    Scope,
    Proj,

    // Control
    Start, Ret,
    If, Region,

    // Data
    Const,
    Add, Sub, Mul, Div, Mod, Neg,
    Phi,
};

namespace node {
    NodeType type_of_op(Op op) {
        switch (op) {
            case Op::Neg:           return NodeType::Neg;
            case Op::LogiNot:       todo;
            case Op::BitNot:        todo;

            case Op::Add:           return NodeType::Add;
            case Op::Sub:           return NodeType::Sub;
            case Op::Mul:           return NodeType::Mul;
            case Op::Div:           return NodeType::Div;
            case Op::Mod:           return NodeType::Mod;
            case Op::LogiOr:        todo;
            case Op::LogiAnd:       todo;
            case Op::BitOr:         todo;
            case Op::BitAnd:        todo;
            case Op::BitXor:        todo;
            case Op::Eq:            todo;
            case Op::Less:          todo;
            case Op::Greater:       todo;
            case Op::LessEq:        todo;
            case Op::GreaterEq:     todo;
            case Op::Assignment:    todo;

            case Op::Undefined:
            case Op::Minus:
            case Op::Star:
            case Op::Ampersand:     panic;
        }
        unreachable;
    }
}

// Assume that *every* node is reachable from Start by *only* using `output` edges
struct Node {
    u32 uid;
    NodeType nt;
    Maybe<Token> token;
    Vec<Node*> input; // use-def references; nullable, fixed length, ordered
    Vec<Node*> output; // def-use references; nonull
    Type* type; // best known type of this node; if null, this node is dead (for nonull for alive nodes)

    inline static u32 uid_counter = 0;
    inline static mem::Arena* node_arena = nullptr;

    // CALL AT THE BEGINNING OF MAIN
    static void init(mem::Arena& arena) {
        Node::uid_counter = 0;
        Node::node_arena = &arena;
    }

    // No token
    static Node empty(NodeType type) {
        Node::uid_counter++;
        return Node { .uid=Node::uid_counter, .nt=type, .token=Maybe<Token>::none(),
            .input=Vec<Node*>::create(*Node::node_arena), .output=Vec<Node*>::create(*Node::node_arena), .type=type::pool.top()
        };
    }
    // Yes token
    static Node from_token(NodeType type, Token t) {
        Node n = Node::empty(type);
        n.token = Maybe<Token>::some(t);
        return n;
    }

    /* Methods */
    // # WARNING 
    // Only ever call these methods on concrete Nodes that have been put on an arena. Don't call on dangling nodes. They will use their address.
    template <typename... Args>
    void push_inputs(Args&&... inputs) {
        (this->push_input(std::forward<Args>(inputs)), ...);
    }
    void push_input(Node* new_input) {
        input.push(new_input);
        if(new_input != nullptr) new_input->output.push(this);
    }
    void pop_input() {
        Node* last_input = input.pop();
        if(last_input != nullptr) {
            last_input->output.remove_first_of(this); // remove this from popped node's output
            // If we removed the last use, the old input is now dead
            if(last_input->output.empty())
                last_input->kill();
        }
    }
    void pop_inputs(usize n) {
        for(usize i = 0; i < n; i++) this->pop_input();
    }
    Node* set_input(usize index, Node* new_input) {
        Node* old_input = input[index];
        if(old_input == new_input) return this; // No change
        if(new_input != nullptr)
            new_input->output.push(this);
        // If the old input exists, remove a def->use edge
        if(old_input != nullptr) {
            old_input->output.remove_first_of(this); // remove this from last node's output
            // If we removed the last use, the old input is now dead
            if(old_input->output.empty())
                old_input->kill();
        }
        // Set the new_def over the old (killed) edge
        input[index] = new_input;
        // Return self for easy flow-coding
        return new_input;
    }
    bool unused() {
        return output.empty();
    }
    bool dead() {
        return this->unused() && input.empty() && type == nullptr;
    }
    void kill() {
        assert(this->unused()); // Has no uses, so it is dead
        this->pop_inputs(input.size); // Set all inputs to null, recursively killing unused Nodes
        // for(usize i = 0; i < input.size; i++) { this->set_input(i, nullptr); } input.clear();
        type = nullptr; // Flag as dead
        assert(this->dead());
    }
};
