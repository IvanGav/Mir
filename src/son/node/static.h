#pragma once

#include "../../core/map.h"
#include "../../core/set.h"

struct Node;
struct NodeScope;
typedef Node CFGNode;

// Special nodes
Node* VOID_NODE;
CFGNode* START_NODE;
CFGNode* STOP_NODE;

// Scope nodes
NodeScope* SCOPE_NODE;
NodeScope* BREAK_SCOPE_NODE;
NodeScope* CONTINUE_SCOPE_NODE;

namespace node {
    // to index into these vectors, use `CFGNode::cfgid` that's assigned during `compute_idom`
    u32 cfg_size; // number of cfg nodes in the graph
    Vec<CFGNode*> cfgrp; // reverse postordering of cfg nodes
    Vec<CFGNode*> dom; // array of immidiate dominators of all cfg nodes
    Vec<u32> domdepth; // depth in the dominator tree for all cfg nodes
    Vec<u32> loopdepth; // loop depth of all cfg nodes
};