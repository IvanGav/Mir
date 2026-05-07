#pragma once

#include "reg_alloc.h"

// TODO ENSURE THAT ALL MASK SUB, AND, ETC OPERATIONS ARE ALSO MUTATING THE LRGS WHEN NEEDED

// Interference Graph
namespace reg_alloc {
    // Forward decls
    void do_block(RegAlloc* alloc, CFGNode* bb);
    void do_node(RegAlloc* alloc, Node* n);
    void kills(RegAlloc* alloc, Node* m);
    void self_conflict(RegAlloc* alloc, Node* n, LRG* lrg);
    void self_conflict(RegAlloc* alloc, Node* n, LRG* lrg, Node* prior);
    void merge_live_out(RegAlloc* alloc, CFGNode* priorbb, u32 i);
    void convert_2d_adj(RegAlloc* alloc);
    void pick_color(Vec<LRG*>& color_stack, u32 sptr, u32 swork);
    bool better_lrg(LRG* best, LRG* lrg);
    u32 pick_risky(Vec<LRG*>& color_stack, u32 sptr);
    u32 pick_risky_score(LRG* lrg);
    Reg bias_color(RegAlloc* alloc, LRG* lrg, Reg reg, RegMask& mask);
    u32 biasable(Node* split);
    Reg bias_color(RegAlloc* alloc, Node* split, RegMask& mask);
    Reg bias_color_neighbors(RegAlloc* alloc, Node* split, RegMask& mask);
    void swap(Vec<LRG*>& ary, u32 x, u32 y);

    // Map from a Basic Block to Live-Out: {a map from a Live Range to a Def}
    HMap<CFGNode*,HMap<LRG*,Node*>> bb_outs {};
    HMap<LRG*,Node*> tmp {};
    // Inteference Graph: Array of Bitsets
    Vec<BitSet> ifg {};
    Vec<CFGNode*> work {};

    void reset_bb_live_out() {
        for(P<CFGNode*, HMap<LRG*,Node*>>& bbout : bb_outs)
            bbout.b.clear();
    }
    void reset_ifg() {
        for(BitSet& bs : ifg)
            bs.clear();
    }
    
    // Set matching bit
    void add_ifg(LRG* lrg0, LRG* lrg1) {
        u16 x0 = lrg0->lrg;
        u16 x1 = lrg1->lrg;
        // Triangulate
        if(x0 < x1) {
            if(ifg.size <= x0) { ifg.resize(next_power_of_two(x0+1)); }
            ifg[x0].set(x1); // Add x1 to x0's conflict set
        }
        else {
            if(ifg.size <= x1) { ifg.resize(next_power_of_two(x1+1)); }
            ifg[x1].set(x0); // Add x0 to x1's conflict set
        }
    }

    void push_work(CFGNode* bb) {
        if(!work.contains(bb))
            work.push(bb);
    }

    // ------------------------------------------------------------------------
    // Visit all blocks, using the live-out LRGs per-block and doing a backwards
    // walk over each block.  At the end of the walk, push the live-out sets to
    // prior blocks.  Due to loops, no single visitation order suffices.  This
    // problem can be fairly efficiently solved with a proper LIVE calculation
    // and bitsets.  In the interests of simplicity (and assumption of smaller
    // programs) I am skipping LIVE and directly computing it during the IFG
    // building.

    // Start by setting the live-out of the exit block (to empty), and putting
    // it on a worklist.

    // - Pull from worklist a block who's live-out has changed
    // - Walk backwards, adding interferences and computing live.
    // - At block head, "push" the new live-out to prior blocks.
    // - - If they pick up new live-outs, put them on the worklist.

    bool build_ifg(RegAlloc* alloc) {
        // Reset all to empty
        reset_bb_live_out();
        reset_ifg();

        // Last block has nothing live out
        assert(work.empty());
        for(CFGNode* bb : node::cfgrp) {
            if(node::is_block_head(bb)) {
                work.push(bb);
            }
        }

        // Process blocks until no more changes
        while(!work.empty()) {
            do_block(alloc, work.pop());
        }

        if(alloc->success()) {
            convert_2d_adj(alloc);
        }
        return alloc->success();
    }

    // Walk one block backwards, compute live-in from live-out, and build IFG
    void do_block(RegAlloc* alloc, CFGNode* bb) {
        mem::Arena scratch = mem::Arena::create(1 MB);
        assert(node::is_block_head(bb));
        tmp.clear();
        if(bb_outs.has(bb)) {
            tmp = bb_outs[bb].clone(&scratch);
        }

        // A backwards walk over instructions in the basic block
        for(i32 inum = bb->output.size - 1; inum >= 0; inum--) {
            Node* n = bb->output[inum];
            if(n->input[0] != bb) continue;
            // In a backwards walk, proj users come before the node itself
            if(node::is_multinode(n))
                for(Node* proj : n->output)
                    if(proj->nt == NodeType::Proj)
                        do_node(alloc, proj);
            if(x86::is_mach(n))
                do_node(alloc, n);
        }

        // The bb head kills register, e.g. a CallEnd and caller-save registers
        if(x86::is_mach(bb))
            kills(alloc, bb);

        // Push live-sets backwards to priors in CFG.
        if(bb->nt == NodeType::Region || bb->nt == NodeType::Loop) {
            for(u32 i = 1; i < bb->input.size; i++)
                merge_live_out(alloc, bb, i);
        } else {
            merge_live_out(alloc, bb, 0);
        }
    }

    void do_node(RegAlloc* alloc, Node* n) {
        mem::Arena scratch = mem::Arena::create(10 MB);
        // Defining means killing live LRG
        LRG* lrg = alloc->get_lrg(n);
        if(lrg != nullptr) {
            // Check for def-side self-conflict live ranges.  These must split,
            // and only happens during the first round a particular LRG splits.
            self_conflict(alloc, n, lrg);
            tmp.remove(lrg); // Kill def
        }

        // Phis use and define the same live range, i.e. these LRGs already
        // marked conflicted, no need to mark again
        if(n->nt == NodeType::Phi)
            return;

        // Kill any killed registers; milli-code routines like New can kill will not being a CFG.
        if(x86::is_mach(n))
            kills(alloc, n);

        // Interfere n with all live
        if(lrg != nullptr) {
            // Interfere n with all live
            for(P<LRG*,Node*>& tlrg_p : tmp) {
                LRG* tlrg = tlrg_p.a;
                assert(tlrg->is_leader());
                // Skip self && do only when and register sets overlap
                if(lrg != tlrg && lrg->mask.overlaps(tlrg->mask)) {
                    // Then tlrg and lrg interfere.  If lrg needs the single
                    // last tlrg register at some point, either tlrg or lrg
                    // must fail.  If *n* (a subset of lrg) needs the single
                    // last tlrg register then only tlrg must fail.
                    if(x86::outregmap(n).is_size_1()) {
                        tlrg->mask = tlrg->mask - lrg->mask.first_reg(); // TODO MAKE ABSOLUTELY SURE THAT SHOULD MUTATE
                        if(tlrg->mask.is_empty())
                            alloc->fail(tlrg); // Clearing drives mask to empty
                    } else {
                        add_ifg(lrg, tlrg); // Add interference
                    }
                }
            }
        }


        // Check for self-conflict live ranges.  These must split, and only
        // happens during the first round when a particular LRG splits.
        // Also record use-side spills for biased coloring.
        // Then make all inputs live.
        for(u32 i = 1; i < n->input.size; i++) {
            Node* def = n->input[i];
            if(def == nullptr) continue;
            LRG* lrg1 = alloc->get_lrg(def);
            if(lrg1 == nullptr) continue; // Anti-dependence, etc, no register

            // Check use-side for self-conflict; if so we MUST split
            self_conflict(alloc, def, lrg1);

            // Look for a must-use single register conflicting with some other must-def.
            if(x86::is_mach(n)) {
                RegMask ni_mask = x86::regmap(n, i);
                if(ni_mask.is_size_1()) { // Must-use single register
                    // Search all current live
                    for(P<LRG*, Node*>& tlrg_p : tmp) {
                        LRG* tlrg = tlrg_p.a;
                        assert(tlrg->is_leader());
                        Node* live = tlrg_p.b;
                        if(live != def && x86::is_mach(live) && x86::outregmap(live).overlaps(ni_mask)) {
                            // Deny the register, since it absolutely must be used here
                            tlrg->mask = tlrg->mask - ni_mask.first_reg(); // TODO MAKE ABSOLUTELY SURE THIS SHOULD MUTATE
                            if(tlrg->mask.is_empty()) {
                                // Then direct reg-reg conflict between use here (at n.in(i)) and def (of tlrg) there.
                                // Fail the older live range, it must move its register.
                                alloc->fail(tlrg);
                            }
                        }
                    }
                }
            }

            // All inputs live - except in self conflicts, where we keep the prior def alive until it goes.
            tmp.add(lrg1, def);
        }
    }

    // Single-register defines *must* have their register; other live ranges
    // *must* avoid this register, so instead of interfering we remove the
    // single register from the other rmask.
    void kills(RegAlloc* alloc, Node* m) {
        RegMask kill_mask = x86::killmap(m);
        if(kill_mask.regs == 0) return;

        // Kill registers with all live
        for(P<LRG*,Node*>& tlrg_p : tmp) {
            LRG* tlrg = tlrg_p.a;
            assert(tlrg->is_leader());
            // Always, tlrg cannot use kills
            if(tlrg->mask.overlaps(kill_mask)) {
                // Disallow clone-ables from killing registers.  Just fail
                // them and re-clone closer to target... so no kill.
                // Special case for Intel XOR used to zero.
                Node* maybe_phi = m->output[0];
                CFGNode* effective_use_block = maybe_phi->nt == NodeType::Phi ? maybe_phi->ctrl()->ctrl(maybe_phi->input.index_of(m)) : maybe_phi->ctrl();
                if(x86::is_clone(m) &&  // Must be clonable
                    (m->output.size > 1 || // Has many users OR
                    effective_use_block != m->ctrl()) // Only 1 user but effective use is remote block
                ) {
                    // Then fail the clonable; it should split or move
                    alloc->fail(alloc->get_lrg(m));
                } else {
                    tlrg->mask = tlrg->mask - kill_mask;
                    if(tlrg->mask.is_empty()) {
                        // Else clonable cannot move
                        alloc->fail(tlrg);
                    }
                }
            }
        }
    }

    // Check for self-conflict live ranges.  These must split, and only happens during the first round a particular LRG splits.
    void self_conflict(RegAlloc* alloc, Node* n, LRG* lrg) {
        if(tmp.has(lrg)) {
            self_conflict(alloc, n, lrg, tmp[lrg]);
        } else {
            self_conflict(alloc, n, lrg, nullptr);
        }
    }
    void self_conflict(RegAlloc* alloc, Node* n, LRG* lrg, Node* prior) {
        if(prior != nullptr && prior != n) {
            lrg->self_conflict(prior);
            lrg->self_conflict(n);
            alloc->fail(lrg); // 2 unrelated values live at once same live range; self-conflict
        }
    }

    // Merge TMP into bb's live-out set; if changes put bb on `work`
    void merge_live_out(RegAlloc* alloc, CFGNode* priorbb, u32 i) {
        CFGNode* bb = priorbb->ctrl(i);
        if(bb == nullptr || bb == START_NODE) return; // Start has no prior
        if(!node::is_block_head(bb)) bb = bb->ctrl();
        //if( i==0 && !(bb instanceof StartNode) ) bb = bb.cfg0();
        assert(node::is_block_head(bb));

        // Lazy get live-out set for bb
        if(!bb_outs.has(bb)) {
            bb_outs.add(bb, ref(HMap<LRG*,Node*>::create()));
        }
        HMap<LRG*, Node*>& lrgs = bb_outs[bb];

        for(P<LRG*,Node*>& lrg_p : tmp) {
            LRG* lrg = lrg_p.a;
            Node* def = lrg_p.b;
            // Effective def comes from phi input from prior block
            if(def->nt == NodeType::Phi && def->ctrl() == priorbb) {
                assert(i != 0);
                def = def->input[i];
            }
            if(lrgs.has(lrg)) {
                // Alive twice with different definitions; self-conflict
                self_conflict(alloc, def, lrg, lrgs[lrg]);
            } else {
                lrgs.add(lrg,def);
                push_work(bb);
            }
        }
    }

    void convert_2d_adj(RegAlloc* alloc) {
        // Convert the 2-D array of bits (a 1-D array of BitSets) into an adjacency matrix.
        u32 lrgnum = alloc->unify_lrgs.size;
        if(ifg.size < lrgnum) {
            ifg.resize(lrgnum);
        }
        for(u32 i = 1; i < lrgnum; i++) {
            BitSet& ifg_i = ifg[i];
            if(ifg_i.size == 0) continue; // shouldn't be required, but also shouldn't hurt
            LRG* lrg0 = alloc->unify_lrgs[i];
            for(u32 lrg = ifg_i.next_set_bit(0); lrg != U32_MAX; lrg = ifg_i.next_set_bit(lrg+1)) {
                LRG* lrg1 = alloc->unify_lrgs[lrg];
                lrg0->adj.push(lrg1);
                lrg1->adj.push(lrg0);
            }
        }
    }

    // ------------------------------------------------------------------------
    // Color the inference graph.

    // Coloring works by removing "trivial" live ranges from the IFG - those
    // live ranges with fewer neighbors than available colors (registers).
    // These are trivial because even if every neighbor takes a unique color,
    // there's at least one more available to color this live range.

    // If we hit a clique of non-trivial live ranges, we pick one to be "at
    // risk" of not-coloring.  Good choices include live ranges with a large
    // area and few hot uses.

    // Then we reverse and put-back live ranges - picking a color as we go.
    // If there's no spare color we'll have to spill this at-risk live range.

    bool color_ifg(RegAlloc* alloc) {
        u32 maxlrg = alloc->unify_lrgs.size;
        u32 nlrgs = 0;
        for(u32 i = 1; i < maxlrg; i++) {
            if(alloc->unify_lrgs[i] != nullptr) {
                assert(alloc->unify_lrgs[i]->reg == Reg::UNDEFINED); // must enter coloring uncolored
                assert(alloc->unify_lrgs[i]->mask.regs != 0);        // must have at least one allowed register
                assert(alloc->unify_lrgs[i]->mask.regs <= U16_MAX);  // sanity: only 16 registers exist
                nlrgs++;
            }
        }

        // Simplify

        // Walk all the LRGS looking for some trivial ones to start with.
        // During this pass all LRGS are broken into 3 disjoint sets:
        // - trivial, removed from IFG;           color_stack[0 to sptr]
        // - trivial, not (yet) removed from IFG; color_stack[sptr to swork]
        // - unknown;                             color_stack[work to maxlrg]

        // Gather all not-unified (not-null); separate trivial and non-trivial set.
        u32 sptr = 0, swork = 0;
        Vec<LRG*> color_stack = Vec<LRG*>::create(default_arena); // TODO use scratch arena
        color_stack.resize(nlrgs);
        for(u32 i = 0, j = 0; i < maxlrg; i++) {
            LRG* lrg = alloc->unify_lrgs[i];
            if(lrg == nullptr) continue; // Unified lrgs are null here
            color_stack[j] = lrg;
            j++;
            if(lrg->low_degree()) {
                reg_alloc::swap(color_stack, swork, j-1);
                swork++;
            }
        }

        // Pull all lrgs from IFG, in trivial order if possible
        while(sptr < color_stack.size) {
            // Swap best color up front
            pick_color(color_stack, sptr, swork);

            // Pick a trivial lrg, and (temporarily) remove from the IFG.
            LRG* lrg = color_stack[sptr++];
            // If sptr was swork, then pulled an at-risk lrg
            if(sptr > swork)
                swork = sptr;

            // Walk all neighbors and remove
            for(LRG* nlrg : lrg->adj) {
                // Remove and compress out neighbor
                nlrg->adj.remove_swap_first_of(lrg);
                if(nlrg->adj.size == nlrg->mask.size()) {
                    // Neighbor is just exactly going trivial as 'lrg' is removed from IFG
                    // Find "j" position in the color_stack
                    u32 jx = swork;
                    while(color_stack[jx] != nlrg) jx++;
                    // Add trivial neighbor to trivial list.  Pull lrg j out of
                    // unknown set, since its now in the trivial set
                    reg_alloc::swap(color_stack, swork++, jx);
                }
            }
        }

        // Reverse simplify (unstack the color stack), and set colors (registers) for live ranges
        while(sptr > 0) {
            sptr--;
            LRG* lrg = color_stack[sptr];
            if(lrg == nullptr) continue;
            RegMask& rmask = lrg->mask; // TODO CHECK HERE
            // Walk neighbors and remove adjacent colors
            for(LRG* nlrg : lrg->adj) {
                // if(!nlrg->adj.contains(lrg)) nlrg->adj.push(lrg); // less true to the Simple impl
                assert(nlrg->adj[nlrg->adj.size] == lrg); // more true to the Simple impl
                nlrg->adj.size++;

                Reg reg = nlrg->reg;
                if(reg != Reg::UNDEFINED) // Failed neighbors do not count
                    rmask = rmask - reg; // Remove neighbor from my choices
            }
            // At-risk live-range did not color?
            if(rmask.is_empty()) {
                alloc->fail(lrg);
                lrg->reg = Reg::UNDEFINED;
            } else {
                // Pick first available register
                Reg reg = rmask.first_reg();
                // Pick a "good" color from the choices.  Typically, biased-coloring
                // removes some over-spilling.
                if(rmask.size() > 1) reg = bias_color(alloc, lrg, reg, rmask);
                lrg->reg = reg; // Assign the register
            }
        }

        return alloc->success();
    }

    // Pick LRG from color stack
    void pick_color(Vec<LRG*>& color_stack, u32 sptr, u32 swork) {
        // Out of trivial colorable, pick an at-risk to pull
        if(sptr == swork)
            reg_alloc::swap(color_stack, sptr, pick_risky(color_stack, sptr));
        // When coloring, we'd like to give more choices; so when coloring we'd
        // like to see the single-def first (since no choices anyway), then
        // non-split related (so more live ranges get colored), then
        // split-related last, so they have more colors to bias towards.

        // Working in reverse, pick first split-related with many regs, then
        // those with some regs, then single-def.
        u32 bidx = sptr;
        LRG* best = color_stack[bidx];
        for(u32 idx = sptr + 1; idx < swork; idx++) {
            if(better_lrg(best, color_stack[idx])) {
                bidx = idx;
                best = color_stack[bidx];
            }
        }
        if(bidx != sptr) {
            reg_alloc::swap(color_stack, sptr, bidx); // Pick best at sptr
        }
    }

    bool better_lrg(LRG* best, LRG* lrg) {
        // If single-def varies, keep the not-single-def
        if(best->is_size_1() != lrg->is_size_1())
            return best->is_size_1();
        // If hasSplit varies, keep the hasSplit
        if(best->has_split() != lrg->has_split())
            return lrg->has_split();
        // Keep large register count
        return best->size() < lrg->size();
    }

    // Pick a live range that hasn't already spilled, or has a single-def-
    // single-use that are not adjacent.
    u32 pick_risky(Vec<LRG*>& color_stack, u32 sptr) {
        u32 best = sptr;
        u32 bestScore = pick_risky_score(color_stack[best]);
        for(u32 i = sptr + 1; i < color_stack.size; i++) {
            if(bestScore == 1000000) return best; // Already max score
            u32 iScore = pick_risky_score(color_stack[i]);
            if(iScore > bestScore) {
                best = i;
                bestScore = iScore;
            }
        }
        return best;
    }

    // Pick a live range to pull, that might not color.
    //
    // Picking a live range with a very large span, with defs and uses outside
    // loops means spilling a relative cheap live range and getting that
    // register over a large area.
    //
    // Picking a live range that is very close to coloring might allow it to
    // color despite being risky.
    u32 pick_risky_score(LRG* lrg) {
        // Pick single-def clonables that are not right next to their single-use.
        // Failing to color these will clone them closer to their uses.
        if(!lrg->multi_input && x86::is_clone(lrg->n_input)) {
            Node* def = lrg->n_input;
            Node* use = lrg->n_output;
            CFGNode* cfg = def->ctrl();
            // Different blocks OR Same block, but not close
            if(cfg != use->ctrl() || cfg->output.index_of(def) < cfg->output.index_of(use) + 1)
                return 1000000;
        }

        // TODO: cost/benefit model.  Perhaps counting loop-depth (freq) of def/use for cost and "area" for benefit
        return 1000;
    }

    Reg bias_color(RegAlloc* alloc, LRG* lrg, Reg reg, RegMask& mask) {
        if(mask.is_size_1()) return reg;
        // Check chain of splits up the def-chain.  Take first allocated
        // register, and if it's available in the mask, take it.
        Node* def = lrg->split_input;
        Node* use = lrg->split_output;
        u32 tidx = 0, cnt = 0;

        while(def != nullptr || use != nullptr) {
            if(cnt > 10) break; // what's cnt?
            cnt++;

            if(def != nullptr) {
                Reg bias = bias_color(alloc, def, mask);
                if(bias != Reg::KILL && bias != Reg::UNDEFINED) return bias; // Good bias
                if(bias == Reg::KILL) def = nullptr; // Kill this side, no more searching
                else {
                    tidx = biasable(def);
                    if(tidx == 0) def = nullptr;
                }
            }

            if(use != nullptr) {
                Reg bias = bias_color(alloc, use, mask);
                if(bias != Reg::KILL && bias != Reg::UNDEFINED) return bias; // Good bias
                if(bias == Reg::KILL) use = nullptr; // Kill this side, no more searching
                else if(biasable(use) == 0) use = nullptr;
            }

            if(def != nullptr) {
                Reg bias = bias_color_neighbors(alloc, def, mask);
                if(bias != Reg::KILL && bias != Reg::UNDEFINED) return bias;
                // Advance def side
                def = def->input[tidx];
                if(alloc->get_lrg(def) == nullptr) def = nullptr;
            }

            if(use != nullptr) {
                Reg bias = bias_color_neighbors(alloc, use, mask);
                if(bias != Reg::KILL && bias != Reg::UNDEFINED) return bias;
                use = use->output[0];
                if(biasable(use) == 0) use = nullptr;
            }
        }
        return mask.first_reg();
    }

    u32 biasable(Node* split) {
        if(split->nt == NodeType::Split) return 1; // Yes biasable, advance is slot 1
        if(split->nt == NodeType::Phi) return split->ctrl()->nt == NodeType::Loop ? 2 : 1; // Yes biasable, advance is slot 1
        if(!x86::is_mach(split)) return 0; // Not biasable
        return x86::two_address(split); // Only biasable if 2-addr
    }

    // 3-way return:
    // - good bias reg, take it & exit
    // - this path is cutoff; do not search here anymore
    // - advance this side
    Reg bias_color(RegAlloc* alloc, Node* split, RegMask& mask) {
        Reg bias = alloc->get_lrg(split)->reg;
        if(bias != Reg::UNDEFINED) {
            if(mask.test(bias)) return bias; // Good bias
            else return Reg::KILL;           // Kill this side
        } else {
            return Reg::UNDEFINED;           // Advance this side
        }
    }

    // Check if we can match "split" color, or else trim "mask" to colors
    // "split" might get.
    Reg bias_color_neighbors(RegAlloc* alloc, Node* split, RegMask& mask) {
        LRG* slrg = alloc->get_lrg(split);
        if(slrg->adj.size == 0) return Reg::UNDEFINED; // No trimming

        // Can I limit my own choices to valid neighbor choices?
        for(LRG* alrg : slrg->adj) {
            Reg reg = alrg->reg;
            if(reg == Reg::UNDEFINED && alrg->mask.is_size_1())
                reg = alrg->mask.first_reg();
            if(reg != Reg::UNDEFINED) {
                mask = mask - alrg->reg;
                if(mask.is_size_1())
                    return mask.first_reg();
            }
        }
        return Reg::UNDEFINED; // No obvious color choice, but mask got trimmed
    }

    void swap(Vec<LRG*>& ary, u32 x, u32 y) {
        LRG* tmp = ary[x]; ary[x] = ary[y]; ary[y] = tmp;
    }
    
    bool coalesce(RegAlloc* alloc) {
        // Walk all the splits, looking for coalesce chances
        bool progress = false;
        for(CFGNode* bb : node::cfgrp) {
            for(i32 j = 0; j < (i32)bb->output.size; j++) {
                Node* n = bb->output[j];
                if(n->nt != NodeType::Split) continue;
                // only look at splits here
                LRG* v1 = alloc->get_lrg(n);
                LRG* v2 = alloc->get_lrg(n->input[1]);
                if(v1 != v2) {
                    LRG* ov1 = v1;
                    LRG* ov2 = v2;
                    // Get the smaller neighbor count in v1
                    if(v1->adj.size > v2->adj.size) mem::swap(&v1, &v2);
                    u32 v2size_saved = v2->adj.size;

                    // See that they do not conflict (coalescing would make a self-conflict)
                    if(v1->adj.size > 0 && v2->adj.size > 0 && v1->mask.overlaps(v2->mask)) {
                        // Walk the smaller neighbors, and add to the larger.
                        // Check for direct conflicts along the way.
                        for(LRG* v3 : v1->adj) {
                            if(v3 == v2) {
                                // Direct conflict, skip this LRG pair
                                v2->adj.size = v2size_saved;
                                goto continue_outer;
                            }
                            // Add missing LRGs from V1 to V2
                            if(!v2->adj.contains(v3)) v2->adj.push(v3);
                        }
                    }
                    // Most constrained mask
                    RegMask new_mask = v1->mask & v2->mask; // TODO CHECK HERE should not mutate.. right?
                    // Check for capacity
                    if(v2->adj.size >= new_mask.size()) {
                        // Fails capacity, will not be trivial colorable
                        if(v2->adj.size > 0) v2->adj.size = v2size_saved;
                        continue;
                    }

                    // Union lrgs.
                    progress = true;

                    // Since `n` is dying, remove from the LRGS BEFORE doing
                    // the union; this will let the union code keep the
                    // matching bits from the other side, preserving some
                    // info for biased coloring
                    if(ov1->n_input == n) ov1->n_input = nullptr;
                    if(ov1->split_input == n) ov1->split_input = nullptr;
                    if(ov2->n_output == n) ov2->n_output = nullptr;
                    if(ov2->split_output == n) ov2->split_output = nullptr;
                    // Keep larger adjacency list.
                    Vec<LRG*> larger = v2->adj;

                    v2->adj.invalidate(); // this will make sure that when new elements are pushed to v2->adj, it will request a new memory slice

                    // Union lrgs.  Swaps again, based on small lrg
                    LRG* v_win = v1->union_with(v2);
                    v_win->adj = larger;
                    LRG* v_lose = v1->is_leader() ? v2 : v1;
                    // LRG* v_lose = v_win == v1 ? v2 : v1; // TODO < why no something like this instead

                    // LRGs below v2len were always in v2, but maybe not in v1.
                    // LRGs above v2len are in v1 but were not in v2.

                    // Walk v2's neighbors, replacing vLose with vWin.
                    for(LRG* vn : larger) {
                        u32 idx = vn->adj.index_of(v_lose);
                        if(idx != vn->adj.size) { // index_of returns own size if not found
                            vn->adj.remove(idx);
                        }
                        if(!vn->adj.contains(v_win)) {
                            vn->adj.push(v_win);
                        }
                    }
                } else {
                    // Since `n` is dying, remove from the LRGS
                    if(v1->n_input == n) v1->n_input = nullptr;
                    if(v1->split_input == n) v1->split_input = nullptr;
                    if(v1->n_output == n) v1->n_output = nullptr;
                    if(v1->split_output == n) v1->split_output = nullptr;
                }

                // Remove junk split
                n->remove_split();
                j--;
                continue_outer:;
            }
        }

        if(progress)
            alloc->unify();

        return true;
    }
};