#pragma once

#include "../prelude.h"

struct Node;
namespace node {
    bool eq(Node*, Node*);
}

struct GVN {
    Vec<Node*> nodes;
    Node* last_lookup;
    u32 last_index;

    static GVN create(mem::Arena& arena = default_arena) {
        return GVN {
            .nodes = Vec<Node*>::create(arena),
            .last_lookup = nullptr
        };
    }

    u32 index_of(Node* n) {
        if(last_lookup == n) return last_index;
        for(u32 i = 0; i < nodes.size; i++) {
            if(node::eq(nodes[i], n)) return i;
        }
        panic;
    }

    bool has(Node* n) {
        for(u32 i = 0; i < nodes.size; i++) {
            if(node::eq(nodes[i], n)) {
                last_lookup = n; last_index = i;
                return true;
            }
        }
        last_lookup = nullptr;
        return false;
    }

    void insert(Node* n) {
        if(!this->has(n)) nodes.push(n);
    }

    // returned the removed node; SHOULDN'T BE RELIED ON, ONLY USE FOR ASSERTS
    Node* remove(Node* n) {
        if(this->has(n)) {
            u32 i = this->index_of(n);
            Node* to_remove = nodes[i];
            nodes.remove(i);
            return to_remove;
        }
        return nullptr;
    }

    Node* get(Node* n) {
        if(this->has(n)) return nodes[this->index_of(n)];
        insert(n);
        return n;
    }
};