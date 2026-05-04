#pragma once

#include "../../core/prelude.h"
#include "reg_alloc.h"

namespace reg_alloc {
    // Compute live ranges in a single forwards pass.  Every def is a new live
    // range except live ranges are joined at Phi.  Also, gather the
    // intersection of register masks in each live range.  Example: "free
    // always zero" register forces register zero, and calling conventions
    // typically force registers.

    // Sets FAILED to the set of hard-conflicts (means no need for an IFG,
    // since it will not color, just split the conflicted ranges now).  Returns
    // true if no hard-conflicts, although we still might not color.
    bool build_lrg(RegAlloc* alloc) {
        for(Node* bb : node::cfgrp)
            for(Node* n : bb->output) {
                if(n->nt == NodeType::Phi && n->type->ttype != TypeT::Mem) {
                    // All Phi inputs end up with the same LRG.
                    // Pass 1: find any pre-existing LRG, to avoid make-then-Union a LRG
                    LRG* lrg = alloc->get_lrg(n);
                    if(lrg == nullptr) {
                        for(u32 i = 1; i < n->input.size; i++) {
                            lrg = alloc->get_lrg(n->input[i]);
                            if(lrg != nullptr )
                                break;
                        }
                    }
                    // If none, make one.
                    if(lrg == nullptr) lrg = alloc->create_lrg(n);
                    if(n->nt == NodeType::MachineNode) reg_alloc::def_lrg(alloc, n); // TODO how can it be Phi AND Mach???????
                    // Pass 2: everybody uses the same LRG
                    lrg = alloc->union_with(lrg, n);
                    for(u32 i = 1; i < n->input.size; i++)
                        lrg = alloc->union_with(lrg, n->input[i]);
                    if(lrg->mask.is_empty())
                        alloc->fail(lrg);
                } else if(n->nt == NodeType::MachineNode) {
                    // Attempt to commute ops to keep live ranges compatible.
                    if(x86::commutes(n) && n->output.size == 1) {
                        u32 uidx = n->output[0]->input.index_of(n);
                        RegMask mask1 = n->input[1]->nt == NodeType::MachineNode ? x86::outregmap(n->input[1]) : alloc->get_lrg(n->input[1])->mask;
                        RegMask mask2 = n->input[2]->nt == NodeType::MachineNode ? x86::outregmap(n->input[2]) : alloc->get_lrg(n->input[2])->mask;
                        RegMask masko = n->output[0]->nt == NodeType::MachineNode ? x86::regmap(n->output[0], uidx) : alloc->get_lrg(n)->mask;
                        if(!mask1.overlaps(masko) && mask2.overlaps(masko))
                            mem::swap(n->input[1], n->input[2]);
                    }

                    // Define live range
                    reg_alloc::def_lrg(alloc, n);

                    // Now, look in the opposite direction. How are incoming
                    // LRGs affected by this node: For all uses, make live lrgs
                    for(u32 i = 1; i < n->input.size; i++)
                        if(n->input[i] != nullptr) {
                            LRG* lrg2 = alloc->get_lrg(n->input[i]);
                            if(lrg2 != nullptr) { // Anti-dep or other, no LRG
                                RegMask use_mask = x86::regmap(n, i); // use_mask is also ~~null~~ **empty** for anti-dep
                                if(!(lrg2->mach_output(n, (u16) i, use_mask.is_size_1()) & use_mask)) // TODO
                                    alloc->fail(lrg2); // Empty register mask, must split
                            }
                        }
                }

                // MultiNodes have projections which set registers
                if(n->nt == NodeType::Proj || n->nt == NodeType::CtrlProj) // TODO instanceof MultiNode 
                    for(Node* proj : n->output)
                        if(proj->nt == NodeType::MachineNode)
                            reg_alloc::def_lrg(alloc, proj);
            }

        // Collect live ranges
        alloc->unify();

        return alloc->success();
    }

    void def_lrg(RegAlloc* alloc, Node* n) {
        RegMask def_mask = x86::outregmap(n);
        // if(def_mask == nullptr) return; // TODO ???
        LRG* lrg = x86::two_address(n) == 0
            ? alloc->create_lrg(n) // Define a new LRG for N
            : alloc->get_lrg_of_input(n, x86::two_address(n)); // Use the matching 2-adr input
        // Record mask and mach
        if(!lrg->mach_input(n, def_mask.is_size_1()) & def_mask) // TODO argh, this garbage again
            alloc->fail(lrg); // Empty register mask, must split
    }
};