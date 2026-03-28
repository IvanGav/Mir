#pragma once

#include "../../core/map.h"
#include "../../core/set.h"

struct Node;
struct NodeScope;

// Special nodes
Node* VOID_NODE;
Node* START_NODE;

// Scope nodes
NodeScope* SCOPE_NODE;
NodeScope* BREAK_SCOPE_NODE;
NodeScope* CONTINUE_SCOPE_NODE;