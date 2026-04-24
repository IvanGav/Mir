#pragma once

#include "node.h"
#include "cfg.h"
#include "debug.h"

// Imlementation of the dominance algorithm
//  from:   "A Simple, Fast Dominance Algorithm" 
//  by:     Keith D. Cooper, Timothy J. Harvey, and Ken Kennedy
//  link:   https://www.cs.tufts.edu/~nr/cs257/archive/keith-cooper/dom14.pdf
namespace node {
    CFGNode* idom(CFGNode* n1, CFGNode* n2);

    // Note: due to jank, these have been relocated to `static.h`
    // to index into these vectors, use `CFGNode::cfgid` that's assigned during `compute_idom`
    // u32 cfg_size; // number of cfg nodes in the graph
    // Vec<CFGNode*> cfgrp; // reverse postordering of cfg nodes
    // Vec<CFGNode*> dom; // array of immidiate dominators of all cfg nodes
    // Vec<u32> domdepth; // depth in the dominator tree for all cfg nodes
    // Vec<u32> loopdepth; // loop depth of all cfg nodes

    // fill the `cfgrp` with the postordering of the cfg graph
    void cfg_postorder(CFGNode* n, BitSet& visited) {
        assert(n->cfg());
        if(visited[n->uid]) return;
        visited.set(n->uid);
        for(u32 i = 0; i < n->output.size; i++) {
            if(n->output[i]->cfg()) {
                node::cfg_postorder(n->output[i], visited);
            }
        }
        cfgrp.push(n);
    }

    // after parsing the graph, this function generates the idom tree of the graph, populating the arrays `cfgid`, `cfgrp`, `dom`, `domdepth` and `loopdepth`
    void compute_idom() {
        // default arena should be fine, since we only generate this once
        cfgrp = Vec<CFGNode*>::create(default_arena);
        dom = Vec<CFGNode*>::create(default_arena);
        domdepth = Vec<u32>::create(default_arena);
        loopdepth = Vec<u32>::create(default_arena);
        CFGNode* start = START_NODE;
        // get reverse postordering of the cfg graph
        BitSet visited{};
        node::cfg_postorder(start, visited);
        cfgrp.reverse();
        cfg_size = cfgrp.size;
        // give all nodes their `cfgid`
        for(u32 i = 0; i < cfg_size; i++) {
            cfgrp[i]->cfgid = i;
        }
        // populate the `dom` and `domdepth` array
        dom.resize(cfg_size); domdepth.resize(cfg_size);
        assert(cfgrp[0] == START_NODE);
        dom[0] = START_NODE; // define the Start's idom is itself
        // also assume that all other nodes have inputs; aka NodeStop has at least 1 return connected = no true infinite loops
        // start at 1, because 0 is NodeStart (the only node with no idom)
        // 1 loop over should be enough, since my graphs should be reducible (no goto's, basically)
        bool changed = true; u32 loop_count = 0;
        while(changed) {
            changed = false;
            for(u32 i = 1; i < cfg_size; i++) {
                CFGNode* n = cfgrp[i];
                CFGNode* common_idom = n->ctrl(0);
                u32 max_depth = domdepth[n->ctrl(0)->cfgid];
                for(u32 i = 1; i < n->ctrl_size(); i++) {
                    if(dom[n->ctrl(i)->cfgid] == nullptr) continue;
                    common_idom = node::idom(common_idom, n->ctrl(i));
                    max_depth = max(max_depth, domdepth[n->ctrl(i)->cfgid]);
                }
                if(dom[n->cfgid] != common_idom) {
                    dom[n->cfgid] = common_idom;
                    domdepth[n->cfgid] = max_depth+1;
                    changed = true;
                }
            }
            loop_count++;
        }
        if(loop_count != 2) {
            std::cout << "--WARNING: create_idom took more than 2 loops" << std::endl;
            std::cout << loop_count << std::endl;
            // printd(loop_count);
        }
        // find the loop depth of each cfg node
        loopdepth.resize(cfg_size);
        for(u32 i = 1; i < cfg_size; i++) {
            CFGNode* n = cfgrp[i];
            loopdepth[n->cfgid] = loopdepth[n->idom()->cfgid];
            // NodeType::Loop is the header of the loop; if something has it as its idom, it must be inside of the loop
            if(n->idom()->nt == NodeType::Loop) loopdepth[n->cfgid] += 1;
        }
    }

    // get first immidiate dominator of 2 cfg nodes (aka where their dom paths meet)
    CFGNode* idom(CFGNode* n1, CFGNode* n2) {
        // the algorithm is to go up the idom tree until we meet 
        assert(node::cfg(n1)); assert(node::cfg(n2));
        while(n1 != n2) {
            while(n1->cfgid > n2->cfgid) n1 = n1->idom();
            while(n1->cfgid < n2->cfgid) n2 = n2->idom();
        }
        return n1;
    }
}