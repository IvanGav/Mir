#pragma once

#include "../../core/prelude.h"
#include "../../son/node.h"
#include "register.h"
#include "lrg.h"

struct RegAlloc;
namespace reg_alloc {
    bool build_lrg(RegAlloc* alloc);
    bool build_ifg(RegAlloc* alloc);
    bool coalesce(RegAlloc* alloc);
    bool color_ifg(RegAlloc* alloc);
};

/**
  * "Briggs/Chaitin/Click".
  * Graph coloring.
  * <p>
  * Every def and every use have a bit-set of allowed registers.
  * Fully general to bizarre chips.
  * Multiple outputs fully supported, e.g. add-with-carry or combined div/rem or "xor rax,rax" setting both rax and flags.
  * <p>
  * Intel 2-address accumulator-style ops fully supported.
  * Op-to-stack-spill supported.
  * All addressing modes supported.
  * Register pairs could be supported with some grief.
  * <p>
  * Splitting instead of spilling (simpler implementation).
  * Stack slots are "just another register" (tiny stack frames, simpler implementation).
  * Stack/unstack during coloring with a marvelous trick (simpler implementation).
  * <p>
  * Both bitset and adjacency list formats for the interference graph; one of
  * the few times it's faster to change data structures mid-flight rather than
  * just wrap one of the two.
  * <p>
  * Liveness computation and interference graph built in the same one pass (one
  * fewer passes per round of coloring).
  * <p>
  * Single-register def or use live ranges deny neighbors their required
  * register and thus do not interfere, vs interfering and denying the color
  * and coloring time.  Basically all function calls do this, but many oddball
  * registers also, e.g. older div/mod/mul ops. (5x smaller IFG, the only
  * O(n^2) part of this operation).
  */
struct RegAlloc {
    // Main Coloring Algorithm:
    // Repeat until colored:
    //   Build Live Ranges (LRGs)
    //   - Intersect allowed registers
    //   If hard conflicts (LRGs with no allowed registers)
    //   - Pre-split conflicted LRGs, and repeat
    //   Build Interference Graph (IFG)
    //   - Self conflicts split now
    //   Color (remove trivial first then any until empty; reverse assign colors
    //   - If color fails:
    //     - Split uncolorable LRGs

    u32 splills, spill_scaled;

    // Map from Nodes to Live Ranges
    HMap<Node*,LRG*> lrgs = {};
    u16 lrg_num = 0;

    Vec<LRG*> unify_lrgs = {};
    Vec<Node*> ns = {};

    // Live ranges with self-conflicts or no allowed registers
    HSet<LRG*> failed {};

    static RegAlloc create(mem::Arena* arena) {
        return RegAlloc{ .lrgs = HMap<Node*,LRG*>::create(*arena), .unify_lrgs = Vec<LRG*>::create(*arena), .ns = Vec<Node*>::create(*arena), .failed = HSet<LRG*>::create(*arena) };
    }

    void allocate() {
        // in Simple, there's an entire for loop here to save registers when a function gets called. I, of course, don't have functions.
        // Cache reg masks for New and Call
        // for(CFGNode* bb : node::cfgrp )
        //     for(Node* n : bb->output)
        //         if(n->nt == NodeType::AllocA) n->cache_regs(this);

        // Top driver: repeated rounds of coloring and splitting.
        u32 round = 0;
        while(!this->graph_color()) {
            this->split();
            if(round >= 12) // Really expect to be done soon
                panic;
            round++;
            printd(round);
        }
        this->post_color(); // Remove no-op spills
    }

    bool graph_color() {
        failed.clear();
        lrgs.clear();
        lrg_num = 1;
        unify_lrgs.clear();

        bool build_lrg_success = reg_alloc::build_lrg(this);
        if(!build_lrg_success) { printd(build_lrg_success); return false; }
        bool build_ifg_success = reg_alloc::build_ifg(this);
        if(!build_ifg_success) { printd(build_ifg_success); return false; }
        bool coalesce_success = reg_alloc::coalesce(this);
        if(!coalesce_success) { printd(coalesce_success); return false; }
        bool color_ifg_success = reg_alloc::color_ifg(this);
        if(!color_ifg_success) { printd(color_ifg_success); return false; }
        return true;
        //     // Build Live Ranges
        //     reg_alloc::build_lrg(this) && // if no hard register conflicts
        //     // Build Interference Graph
        //     reg_alloc::build_ifg(this) && // If no self conflicts or uncolorable
        //     // Conservative coalesce copies
        //     reg_alloc::coalesce(this) &&
        //     // Color attempt
        //     reg_alloc::color_ifg(this); // If colorable
    }

    void fail(LRG* lrg) {
        assert(lrg->is_leader());
        failed.add(lrg);
    }
    bool success() {
        return failed.empty();
    }

    // Define a new LRG, and assign n
    LRG* create_lrg(Node* n) {
        LRG* lrg = this->get_lrg(n);
        if(lrg != nullptr) return lrg;
        lrg = default_arena.push(LRG::create(lrg_num));
        lrg_num++;
        lrgs.add(n, lrg);
        return lrg;
    }

    // LRG for n
    LRG* get_lrg(Node* n) {
        if(!lrgs.has(n)) return nullptr;
        LRG* lrg = lrgs[n];
        LRG* lrg2 = lrg->find_leader();
        if(lrg != lrg2)
            lrgs.add(n, lrg2);
        return lrg2;
    }

    // Find LRG for n->input[idx], and also map n to it
    LRG* get_lrg_of_input(Node* n, u32 idx) {
        LRG* lrg = this->get_lrg(n->input[idx]);
        return this->union_with(lrg, n);
    }

    // Union any lrg for n with lrg and map to the union
    LRG* union_with(LRG* lrg, Node* n) {
        LRG* lrgn = nullptr;
        if(lrgs.has(n)) { lrgn = lrgs[n]; }
        LRG* lrg3 = lrg->union_with(lrgn);
        lrgs.add(n, lrg3);
        return lrg3;
    }

    // Force all unified to roll up; collect live ranges
    void unify() {
        unify_lrgs.clear();
        unify_lrgs.resize(this->lrg_num);
        for(P<Node*, LRG*> e : lrgs) {
            Node* n = e.a;
            LRG* lrg = this->get_lrg(n);
            unify_lrgs[lrg->lrg] = lrg;
        }
        // Remove unified lrgs from failed set also
        for(LRG* lrg : failed) {
            if(!lrg->is_leader()) {
                LRG* lrg2 = lrg->find_leader();
                failed.remove(lrg);
                failed.add(lrg2);
            }
        }
    }

    u16 regnum(Node* n) {
        LRG* lrg = this->get_lrg(n);
        if(lrg == nullptr) { warn; } // not in Simple
        return lrg->reg;
    }

    // Split conflicted live ranges.
    void split() {
        // In C2, all splits are handling in one pass over the program.  Here,
        // in the name of clarity, we'll handle each failing live range
        // independently... which generally requires a full pass over the
        // program for each failing live range.  i.e., might be a lot of
        // passes.
        mem::Arena scratch = mem::Arena::create(1 MB);
        Vec<LRG*> ordered_fail = failed.to_vec(&scratch);
        std::sort(ordered_fail.begin(), ordered_fail.end());
        for(LRG* lrg : ordered_fail) {
            this->split(lrg);
        }
    }

    // Split this live range, top level heuristic
    bool split(LRG* lrg) {
        assert(lrg->is_leader());  // Already rolled up

        if(lrg->self_conflicts.size > 0)
            return this->split_self_conflict(lrg);

        // Register mask when empty; split around defs and uses with limited register masks.
        if(lrg->mask.is_empty() && (!lrg->multi_input || lrg->output_count == 1)) {
            if(lrg->input_count <= 1 && lrg->output_count <= 1 && (lrg->input_count + lrg->output_count) > 0)
                return this->split_empty_mask_simple(lrg);
            // Repeated single-reg uses from a single def.  Special for archs with more fixed regs.
            if(!lrg->multi_input && lrg->input_count <= 1 && lrg->output_count > 2)
                if(this->split_empty_mask_by_output(lrg))
                    return true;
        }

        // Generic split-by-loop depth.
        return split_by_loop(lrg);
    }

    // Split live range with an empty mask. Specifically forces splits at single-register defs or uses and not elsewhere.
    bool split_empty_mask_simple(LRG* lrg) {
        // Live range has a single-def single-register, and/or a single-use
        // single-register.  Split after the def and before the use.  Does not
        // require a full pass.

        // Split just after def
        if(lrg->input_count == 1 && !x86::is_clone(lrg->n_input)) {
            // Force must-split, even if a prior split same block because register conflicts.  Example:
            //   alloc
            //     V1/rax - forced by alloc
            //   alloc
            //     V2/rax - kills prior RAX
            //   st4 [V1],len - No good, must split around
            this->insert_after_and_replace(this->make_split(lrg), (Node*)lrg->n_input, false/*true*/);
        }
        // Split just before use
        if(lrg->output_count == 1 || (lrg->input_count == 1 && ((Node*)lrg->n_input)->output.size == 1)) {
            this->insert_before((Node*)lrg->n_output, lrg->uidx, lrg);
        }
        return true;
    }

    // Single-def live range with an empty mask.  There are many single-reg
    // uses.  Theory is there's many repeats if the same reg amongst the uses.
    // In of splitting once per use, start by splitting into groups based on
    // required input register.
    bool split_empty_mask_by_output(LRG* lrg) {
        Node* def = (Node*)lrg->n_input;

        // Look at each use, and break into non-overlapping register classes.
        Vec<RegMask> rclass = {}; // TODO make scratch
        bool done = false;
        while(!done) {
            done = true;
            for(Node* use : def->output)
                if(x86::is_mach(use))
                    for(u32 i = 1; i < use->input.size; i++)
                        if(use->input[i] == def)
                            done = this->put_into_reg_class(rclass, x86::regmap(use, i));
        }

        // See how many register classes we split into
        if(rclass.size <= 1) return false;

        // Split by class
        for(RegMask rmask : rclass) {
            Node* split = this->make_split(def, lrg);
            split->insert_after(def);
            if(split->input.size > 1) split->set_input(1, def); // TODO what?
            // all uses by class to split
            for(u32 j = 0; j < def->output.size; j++) {
                Node* use = def->output[j];
                if(x86::is_mach(use) && use != split) {
                    // Check all use inputs for n, in case there's several
                    for(u32 i = 1; i < use->input.size; i++ )
                        // Find a def input, and check register class
                        if(use->input[i] == def && x86::regmap(use, i).overlaps(rmask)) {
                            // Modify use to use the split version specialized to this rclass
                            use->set_input(i, split);
                            j--;
                            break;
                        }
                }
            }
        }
        return true;
    }

    // Put use into a register class, perhaps adding a class or perhaps narrowing a class (and causing a repeat)
    bool put_into_reg_class(Vec<RegMask>& rclass, RegMask rmask) {
        for(u32 i = 0; i < rclass.size; i++) {
            RegMask omask = rclass[i];
            if((omask & rmask) == omask) return true; // Within the same register class
            if(omask.overlaps(rmask)) {
                rclass[i] = omask & rmask;
                return false; // Need to go again
            }
        }
        // Add a new class, no need to go again
        rclass.push(rmask);
        return true;
    }

    // Self conflicts require Phis (or two-address). Insert a split after every def.
    bool split_self_conflict(LRG* lrg) {
        mem::Arena scratch = mem::Arena::create(1 MB);
        Vec<Node*> conflicts = lrg->self_conflicts.to_vec(&scratch);
        std::sort(conflicts.begin(), conflicts.end());

        // For all conflicts
        for(Node* def : conflicts) {
            assert(this->get_lrg(def) == lrg); // Might be conflict use-side
            // Split before each use that extends the live range; i.e. is a Phi or two-address
            for(u32 i = 0; i < def->output.size; i++) {
                Node* use = def->output[i];
                // TODO this needs to be done right
                if((use->nt == NodeType::Phi && 
                    !(use->ctrl()->nt == NodeType::Loop && use->input[2] == def && 
                    def->ctrl()->idepth() > use->ctrl()->idepth())) ||
                    (x86::is_mach(use) && x86::two_address(use) != 0 && use->input[x86::two_address(use)] == def)
                ) {
                    this->insert_before(use, use->input.index_of(def), lrg);
                }
            }
            // Split after the Phi which extends the LRG.  Split also before
            // Phi slot 1 (and not all inputs), because Phis extend the live range.
            // TODO: split before all inputs (except the last; at least 1 split here must be extra)
            if(def->nt == NodeType::Phi) {
                Node* split = this->make_split(lrg);
                this->insert_after_and_replace(split, def, false);
                if(split->output.size == 0)
                    split->kill_ordered();
                this->insert_before(def, 1, lrg);
            }
            // Split before two-address ops which extend the live range
            if(x86::is_mach(def) && x86::two_address(def) != 0)
                this->insert_before(def, x86::two_address(def), lrg);
        }
        return true;
    }


    // Generic: split around the outermost loop with non-split def/uses.  This
    // covers both self-conflicts (once we split deep enough) and register pressure issues.
    bool split_by_loop(LRG* lrg) {
        this->find_all_lrg(lrg);

        // Find min loop depth for all non-split defs and uses.
        P<u32,u32> ld {0,9999};
        for(Node* n : this->ns ) {
            if(this->get_lrg(n) == lrg) // This is a LRG def
                ld = this->ldepth(ld, n, n->ctrl());
            // PhiNodes check all CFG inputs
            if(n->nt == NodeType::Phi) {
                for(u32 i = 1; i < n->input.size; i++)
                    ld = this->ldepth(ld, n->input[i], n->ctrl()->ctrl(i));
            } else {
                // Others check uses
                for(u32 i = 1; i < n->input.size; i++)
                    if(this->lrg_same(n->input[i], lrg)) // This is a LRG use
                        ld = this->ldepth(ld, n, n->ctrl());
            }
        }
        u32 minl = ld.a;
        u32 maxl = ld.b;


        // If the minLoopDepth is less than the maxLoopDepth: for-all defs and
        // uses, if at minLoopDepth or lower, split after def and before use.
        for(Node* n : ns) {
            if(n->nt == NodeType::Split) continue; // Ignoring splits; since spilling need to split in a deeper loop
            if(n->is_dead()) continue; // Some Clonable went dead by other spill changes
            // If this is a 2-address commutable op (e.g. AddX86, MulX86) and the rhs has only a single user,
            // commute the inputs... which chops the LHS live ranges' upper bound to just the RHS.
            if(x86::is_mach(n) && this->get_lrg(n) == lrg && x86::two_address(n) == 1 && x86::commutes(n) && n->input[2]->output.size == 1) {
                mem::swap(n->input[1], n->input[2]); // swap 1 and 2 because.. idk, Simple
            }

            if(this->get_lrg(n) == lrg && // This is a LRG def
                // At loop boundary, or splitting in inner loop
                (minl == maxl || n->ctrl()->loop_depth() <= minl)) {
                // Cloneable constants will be cloned at uses, not after def
                if(!(x86::is_mach(n) && x86::is_clone(n)) &&
                    // Single user is already a split adjacent
                    !(n->output.size == 1 && n->output[0]->nt == NodeType::Split && this->same_block_no_clobber(n->output[0])))
                    // Split after def in min loop nest
                    this->insert_after_and_replace(this->make_split(lrg), n, true);
            }

            // PhiNodes check all CFG inputs
            if(n->nt == NodeType::Phi) {
                for(u32 i = 1; i < n->input.size; i++)
                    // No split in front of a split
                    if(!(n->input[i]->nt == NodeType::Split) &&
                        // splitting in inner loop or at loop border
                        (minl == maxl || n->ctrl()->ctrl(i)->loop_depth() <= minl) &&
                        // and not around the backedge of a loop (bad place to force a split, hard to remove)
                        !(n->ctrl()->nt == NodeType::Loop && i == 2 && (n->input[i]->nt == NodeType::Phi && n->input[i]->ctrl() == n->ctrl()))
                    ) {
                        // Split before phi-use in prior block
                        this->insert_before(n, i, lrg);
                    }
            } else {
                // Others check uses
                for(u32 i = 1; i < n->input.size; i++) {
                    // This is a LRG use
                    // splitting in inner loop or at loop border
                    if(this->lrg_same(n->input[i], lrg) &&
                        (minl == maxl || (x86::is_mach(n->input[i]) && x86::is_clone(n->input[i])) || n->ctrl()->loop_depth() <= minl))
                        // Split before in this block
                        this->insert_before(n, i, lrg, false);
                }
            }
        }
        return true;
    }

    bool lrg_same(Node* x, LRG* lrg) {
        LRG* xlrg = this->get_lrg(x);
        while(xlrg == nullptr && x->nt == NodeType::Split) {
            x = x->input[1]; // TODO wth
            xlrg = this->get_lrg(x);
        }
        return xlrg==lrg;
    }

    // the input and output loopdepth ld is Pair{min loop depth, max loop depth}
    P<u32,u32> ldepth(P<u32,u32> ld, Node* n, CFGNode* cfg) {
        // Do not count splits
        if(n->nt == NodeType::Split) return ld;
        // Collect min/max loop depth
        u32 minl = ld.a;
        u32 maxl = ld.b;
        u32 d = cfg->loop_depth();
        // if n will lower the min loop and is in the tail end of the loop
        // header, splitting "around" the loop will not help.  Treat n as being
        // in the loop.
        if(d < minl) {
            if(cfg->ctrl()->nt == NodeType::Loop && ((NodeRegion*)(cfg->ctrl()))->entry() == cfg) {
                CFGNode* loop = cfg->ctrl();
                for(i32 i = cfg->output.size - 2; i >= 0; i--) {
                    Node* out = cfg->output[i];
                    if(n == out) {
                        // Treat n as being "in the loop"
                        d = loop->loop_depth(); 
                        break;
                    }
                    if(!((x86::is_mach(out) && x86::is_clone(out)) || out->nt == NodeType::Split))
                        break; // Treat b as "normal", out of loop
                }
            }
        }

        // lower min, raise max, and re-fold
        minl = min(minl, d);
        maxl = max(maxl, d);
        return P{minl,maxl};
    }

    // Find all members of a live range, both defs and uses
    void find_all_lrg(LRG* lrg) {
        ns.clear();
        u32 wd = 0;
        assert(lrg->n_input != nullptr);
        assert(lrg->n_output != nullptr);
        ns.push(lrg->n_input);
        ns.push(lrg->n_output);
        while(wd < ns.size) {
            Node* n = ns[wd];
            wd++;
            if(this->get_lrg(n) != lrg) continue;
            for(Node* def : n->input)
                if(this->get_lrg(def) == lrg && !ns.contains(def))
                    ns.push(def);
            for(Node* use : n->output)
                if(!ns.contains(use))
                    ns.push(use);
        }
        for(Node* n : ns) assert(!n->is_dead());
    }

    void insert_before(Node* n, u32 i, LRG* lrg, bool skip = true) {
        Node* def = n->input[i];
        // Effective block for use
        CFGNode* cfg = n->nt == NodeType::Phi ? n->ctrl()->ctrl(i) : n->ctrl();
        // Def is a split ?
        if(skip && def->nt == NodeType::Split) {
            bool single_reg = x86::is_mach(n) && x86::regmap(n,i).is_size_1();
            // Same block, multiple registers, split is only used by n,
            // assume this is good enough and do not split again.
            if(cfg == def->ctrl() && def->output.size == 1 && !single_reg)
                return;
        }
        this->make_split(def, lrg)->insert_before(n, i);
        // Skip split-of-split same block
        if(skip && def->nt == NodeType::Split && cfg == def->ctrl())
            n->input[i]->set_input_ordered(1, def->input[1]); // TODO what the hhhh
    }

    // Replace uses of `def` with `split`, and insert `split` immediately after `def` in the basic block.
    void insert_after_and_replace(Node* split, Node* def, bool must) {
        split->insert_after(def);
        if(split->input.size > 1) split->set_input(1, def);
        for(i32 j = def->output.size - 1; j >= 0; j--) {
            Node* use = def->output[j];
            if(use == split) continue; // Skip self
            // Can we avoid a split of a split?  'this' split is used by another split in the same block.
            if(!must && use->nt == NodeType::Split && this->same_block_no_clobber(use) )
                continue;
            u32 idx = use->input.index_of(def);
            use->set_input_ordered(idx, split);
            if(j < (i32)(def->output.size)) j++;
        }
    }

    Node* make_split(Node* def, LRG* lrg) {
        Node* split = x86::is_mach(def) && x86::is_clone(def) ? x86::copy_node(def) : NodeSplit::create_unscheduled(def->ctrl(), def);
        this->lrgs.add(split, lrg);
        return split;
    }
    // returns a NodeSplit
    Node* make_split(LRG* lrg) {
        // version without a known def yet; ctrl and val set later by insert_before/insert_after_and_replace
        // create with placeholders — caller is responsible for setting inputs
        assert(lrg->n_input != nullptr);
        Node* def = (Node*) lrg->n_input;
        Node* split = NodeSplit::create_unscheduled(def->ctrl(), def);
        lrgs.add(split, lrg);
        return split;
    }

    // POST PASS: Remove empty spills that biased-coloring made
    void post_color() {
        // TODO_AI done by Claude; later, make sure it's correct
        for(CFGNode* bb : node::cfgrp) {
            // Walk outputs in forward order; use index because we may remove entries
            for(i32 j = 0; j < (i32)bb->output.size; j++) {
                Node* n = bb->output[j];

                // Only care about splits
                if(n->nt != NodeType::Split) continue;

                // n is a Split: input[1] is the value being split (the def)
                // After coloring, check if the split got the same register as its input
                LRG* split_lrg = this->get_lrg(n);
                LRG* def_lrg   = this->get_lrg(n->input[1]);
                assert(split_lrg != nullptr && def_lrg != nullptr);

                u16 split_reg = split_lrg->reg;
                u16 def_reg = def_lrg->reg;

                // Try to bypass a chain of same-block splits before deciding
                if(split_reg != def_reg) {
                    this->split_bypass(bb, (u32)j, n, split_reg);
                    // Re-read in case bypass updated input[1]
                    def_reg = this->get_lrg(n->input[1])->reg;
                }

                if(split_reg == def_reg) {
                    // Split is a no-op move: same register in and out, remove it
                    n->remove_split();
                    j--; // recheck this index, since output shrunk
                    continue;
                }

                // Split is real: count it
                splills++;
                spill_scaled += (1 << (bb->loop_depth() * 3));
            }
        }
    }
        // u32 max_reg = U32_MAX;
        // // For all ops
        // for(CFGNode* bb : node::cfgrp) {
        //     // Compute frame size, based on arguments and largest reg seen
        //     if( bb instanceof ReturnNode ret )
        //         ret.fun().computeFrameAdjust(_code,maxReg);
        //     // Raise frame size by max stack args passed, even if ignored
        //     if( bb instanceof CallEndNode cend )
        //         maxReg = Math.max(maxReg,cend._xslot);

        //     for( int j=0; j<bb.nOuts(); j++ ) {
        //         Node n = bb.out(j);
        //         if( lrg(n)!=null )
        //             maxReg = Math.max(maxReg,lrg(n).reg+1);
        //         // Raise frame size by max stack args passed to New
        //         if( n instanceof NewNode nnn )
        //             maxReg = Math.max(maxReg,nnn._xslot);

        //         if( !(n instanceof SplitNode ) ) continue;
        //         int defreg = lrg(n     ).reg;
        //         int usereg = lrg(n.in(1)).reg;
        //         // Attempt to bypass split
        //         if( defreg != usereg && splitBypass(bb,j,n,defreg) )
        //             usereg = lrg(n.in(1)).reg;

        //         // Split has same reg?  Useless!  Can remove it!
        //         if( defreg == usereg ) {
        //             n.removeSplit();
        //             j--;
        //             continue;
        //         }
        //         // Split is kept; count against split score
        //         splills++;
        //         spill_scaled += (1<<bb.loopDepth()*3);
        //         assert _spillScaled >= 0;
        //     }
        // }

    bool split_bypass(CFGNode* bb, u32 j, Node* lo, u32 defreg) {
        // Attempt to bypass split
        Node* hi = lo->input[1]; // TODO wth
        while(true) {
            if(!(hi->nt == NodeType::Split && lo->ctrl() == hi->ctrl()))
                return false;
            if(this->get_lrg(hi->input[1])->reg == defreg)
                break;
            hi = hi->input[1];
        }
        // Check no clobbers
        for(u32 idx = j-1; bb->output[idx] != hi; idx++) {
            Node* n = bb->output[idx];
            if(this->get_lrg(n) != nullptr && this->get_lrg(n)->reg == defreg)
                return false; // Clobbered
        }
        lo->set_input_ordered(1, hi->input[1]);
        return true;
    }

    bool same_block_no_clobber(Node* split) {
        assert(split->nt == NodeType::Split);
        Node* def = split->input[1]; // TODO don't
        CFGNode* cfg = def->ctrl();
        if(cfg != split->ctrl()) return false; // Not same block
        // Get multinode head
        // TODO later i'd need to do this: // Node* def0 = def->nt == NodeType::Proj ? def->input[0] : def;
        u32 defreg = this->get_lrg(def)->reg;
        if(defreg == U32_MAX) defreg = this->get_lrg(def)->mask.first_reg();
        if(defreg == U32_MAX) return false; // no allowed registers -> clobbered
        for(i32 idx = cfg->output.index_of(split) - 1; idx >= 0; idx--) {
            Node* n = cfg->output[idx];
            if(n == def) return true; // No clobbers
            if(this->get_lrg(n) == this->get_lrg(def)) return false; // Self conflict
            if(this->get_lrg(n) != nullptr && this->get_lrg(n)->reg == defreg)
                return false; // Clobbered
        }
        panic;
    }
};
