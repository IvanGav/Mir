#pragma once

#include "../prelude.h"

struct Node;
namespace node {
    bool eq(Node*, Node*);
}

struct GVN {
    Vec<Node*> nodes;

    Node* has(Node* other) {

    }
};