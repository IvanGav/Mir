#pragma once

#include "../../core/prelude.h"
#include "../../son/node.h"
#include "register.h"
#include "lrg.h"

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
class RegAlloc {
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

    Vec<LRG*> unify_lrgs = {}; // TODO why.. i hate it. but maybe more useful that *not* having it

    // -----------------------
    // Live ranges with self-conflicts or no allowed registers
    HSet<LRG*> failed {};
    void fail(LRG* lrg) {
        assert(lrg->is_leader());
        failed.add(lrg);
    }
    bool success() { return failed.empty(); }

    // -----------------------
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
        if(!lrgs.exists(n)) return nullptr;
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
        LRG* lrgn = lrgs[n];
        LRG* lrg3 = lrg->union_with(lrgn);
        lrgs.add(n, lrg3);
        return lrg3;
    }

    // Force all unified to roll up; collect live ranges
    void unify() {
        unify_lrgs.clear();
        unify_lrgs.resize(this->lrg_num);
        for(HMap<Node*, LRG*>::Entry e : lrgs) {
            if(!e.exists()) continue;
            Node* n = e.key;
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
        assert(lrg != nullptr); // not in Simple
        return lrg->reg;
    }

    // -----------------------
    void allocate() {
        // in Simple, there's an entire for loop here to save registers when a function gets called. I, of course, don't have functions.
        // Cache reg masks for New and Call
        // for(CFGNode* bb : node::cfgrp )
        //     for(Node* n : bb->output)
        //         if(n->nt == NodeType::AllocA) n->cache_regs(this);

        // Top driver: repeated rounds of coloring and splitting.
        u8 round = 0;
        while(!this->graph_color()) {
            this->split();
            if(round >= 7) // Really expect to be done soon
                panic;
            round++;
        }
        this->post_color(); // Remove no-op spills
    }

    bool graph_color() {
        failed.clear();
        lrgs.clear();
        lrg_num = 1;
        unify_lrgs.clear();

        return
            // Build Live Ranges
            reg_alloc::build_lrg(this) && // if no hard register conflicts
            // Build Interference Graph
            reg_alloc::build_ifg(this) && // If no self conflicts or uncolorable
            // Conservative coalesce copies
            reg_alloc::coalesce(this) &&
            // Color attempt
            reg_alloc::color_ifg(this); // If colorable
    }

    // -----------------------
    // Split conflicted live ranges.
    void split() {
        // In C2, all splits are handling in one pass over the program.  Here,
        // in the name of clarity, we'll handle each failing live range
        // independently... which generally requires a full pass over the
        // program for each failing live range.  i.e., might be a lot of
        // passes.

        for(LRG* lrg : failed)
            this->split(lrg);
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
        if(lrg->input_count == 1 && !node::x86_is_clone(lrg->n_input)) // is clone = cheaper to recreate than to spill
            // Force must-split, even if a prior split same block because register conflicts.  Example:
            //   alloc
            //     V1/rax - forced by alloc
            //   alloc
            //     V2/rax - kills prior RAX
            //   st4 [V1],len - No good, must split around
            this->insert_after_and_replace(this->make_split(lrg), (Node*)lrg->n_input, false/*true*/);
        // Split just before use
        if(lrg->output_count == 1 || (lrg->input_count == 1 && ((Node*)lrg->n_input)->output.size == 1))
            this->insert_before((Node*)lrg->n_output, lrg->uidx, lrg);
        return true;
    }

    // Single-def live range with an empty mask.  There are many single-reg
    // uses.  Theory is there's many repeats if the same reg amongst the uses.
    // In of splitting once per use, start by splitting into groups based on
    // required input register.
    bool split_empty_mask_by_use(LRG* lrg) {
        Node* def = (Node*)lrg->n_input;

        // Look at each use, and break into non-overlapping register classes.
        Vec<RegMask> rclass = {}; // TODO make scratch
        bool done = false;
        while(!done) {
            done = true;
            for(Node* use : def->output)
                // if(use->nt == NodeType::MachineNode) // TODO wrong
                    for(u32 i = 1; i < use->input.size; i++)
                        if(use->input[i] == def)
                            done = put_into_reg_class(rclass, use->regmap(i)); // TODO
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
                if(use->nt == NodeType::MachineNode && use != split) { // TODO
                    // Check all use inputs for n, in case there's several
                    for(u32 i = 1; i < use->input.size; i++ )
                        // Find a def input, and check register class
                        if(use->input[i] == def && use->regmap(i)->overlap(rmask)) {
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


    // // Put use into a register class, perhaps adding a class or perhaps
    // // narrowing a class (and causing a repeat)
    // private static boolean putIntoRegClass( Ary<RegMask> rclass, RegMask rmask ) {
    //     for( int i=0; i<rclass._len; i++ ) {
    //         RegMask omask = rclass.at(i);
    //         if( omask.and(rmask) == omask ) return true; // Within the same register class
    //         if( omask.overlap(rmask) ) {
    //             rclass.set(i,new RegMask(omask.copy().and(rmask)));
    //             return false;   // Need go again
    //         }
    //     }
    //     // Add a new class, no need to go again
    //     rclass.push(rmask);
    //     return true;
    // }

    // // Self conflicts require Phis (or two-address).
    // // Insert a split after every def.
    // boolean splitSelfConflict( byte round, LRG lrg ) {
    //     // Sort conflict set, so we're deterministic
    //     Node[] conflicts = lrg._selfConflicts.keySet().toArray(new Node[0]);
    //     Arrays.sort(conflicts, (x,y) -> x._nid - y._nid );

    //     // For all conflicts
    //     for( Node def : conflicts ) {
    //         assert lrg(def)==lrg; // Might be conflict use-side
    //         // Split before each use that extends the live range; i.e. is a
    //         // Phi or two-address
    //         for( int i=0; i<def._outputs._len; i++ ) {
    //             Node use = def.out(i);
    //             if( (use instanceof PhiNode phi &&
    //                  !(phi.region() instanceof LoopNode loop && phi.in(2)==def && def.cfg0().idepth() > loop.idepth() ) ) ||
    //                     (use instanceof MachNode mach && mach.twoAddress()!=0 && use.in(mach.twoAddress())==def) )
    //                 insertBefore( use, use._inputs.find(def), "use/self/use",round,lrg );
    //         }
    //         // Split after the Phi which extends the LRG.  Split also before
    //         // Phi slot 1 (and not all inputs), because Phis extend the live range.
    //         // TODO: split before all inputs (except the last; at least 1 split here must be extra)
    //         if( def instanceof PhiNode phi && !(def instanceof ParmNode) ) {
    //             SplitNode split = makeSplit("def/self",round,lrg);
    //             insertAfterAndReplace(split,def,false);
    //             if( split.nOuts()==0 )
    //                 split.killOrdered();
    //             insertBefore(phi,1,"use/self/phi",round,lrg);
    //         }
    //         // Split before two-address ops which extend the live range
    //         if( def instanceof MachNode mach && mach.twoAddress()!= 0 )
    //             insertBefore(def,mach.twoAddress(),"use/self/two",round,lrg);
    //     }
    //     return true;
    // }


    // // Generic: split around the outermost loop with non-split def/uses.  This
    // // covers both self-conflicts (once we split deep enough) and register
    // // pressure issues.
    // boolean splitByLoop( byte round, LRG lrg ) {
    //     findAllLRG(lrg);

    //     // Find min loop depth for all non-split defs and uses.
    //     long ld = (-1L<<32) | 9999;
    //     for( Node n : _ns ) {
    //         if( lrg(n)==lrg ) // This is a LRG def
    //             ld = ldepth(ld,n,n.cfg0());
    //         // PhiNodes check all CFG inputs
    //         if( n instanceof PhiNode phi ) {
    //             for( int i=1; i<n.nIns(); i++ )
    //                 ld = ldepth(ld, phi.in(i), phi.region().cfg(i));
    //         } else {
    //             // Others check uses
    //             for( int i=1; i<n.nIns(); i++ )
    //                 if( lrgSame(n.in(i),lrg) ) // This is a LRG use
    //                     ld = ldepth(ld,n,n.cfg0());
    //         }
    //     }
    //     int min = (int)ld;
    //     int max = (int)(ld>>32);


    //     // If the minLoopDepth is less than the maxLoopDepth: for-all defs and
    //     // uses, if at minLoopDepth or lower, split after def and before use.
    //     for( Node n : _ns ) {
    //         if( n instanceof SplitNode ) continue; // Ignoring splits; since spilling need to split in a deeper loop
    //         if( n.isDead() ) continue; // Some Clonable went dead by other spill changes
    //         // If this is a 2-address commutable op (e.g. AddX86, MulX86) and the rhs has only a single user,
    //         // commute the inputs... which chops the LHS live ranges' upper bound to just the RHS.
    //         if( n instanceof MachNode mach && lrg(n)==lrg && mach.twoAddress()==1 && mach.commutes() && n.in(2).nOuts()==1 )
    //             n.swap12();

    //         if( lrg(n)==lrg && // This is a LRG def
    //             // At loop boundary, or splitting in inner loop
    //             (min==max || n.cfg0().loopDepth() <= min) ) {
    //             // Cloneable constants will be cloned at uses, not after def
    //             if( !(n instanceof MachNode mach && mach.isClone()) &&
    //                 // Single user is already a split adjacent
    //                 !(n.nOuts()==1 && n.out(0) instanceof SplitNode split && sameBlockNoClobber(split) ) )
    //                 // Split after def in min loop nest
    //                 insertAfterAndReplace( makeSplit("def/loop",round,lrg), n,true);
    //         }

    //         // PhiNodes check all CFG inputs
    //         if( n instanceof PhiNode phi && !(n instanceof ParmNode)) {
    //             for( int i=1; i<n.nIns(); i++ )
    //                 // No split in front of a split
    //                 if( !(n.in(i) instanceof SplitNode) &&
    //                     // splitting in inner loop or at loop border
    //                     (min==max || phi.region().cfg(i).loopDepth() <= min) &&
    //                     // and not around the backedge of a loop (bad place to force a split, hard to remove)
    //                     !(phi.region() instanceof LoopNode && i==2 && (phi.in(i) instanceof PhiNode pp && pp.region()==phi.region())) )
    //                     // Split before phi-use in prior block
    //                     insertBefore(phi,i, "use/loop/phi",round,lrg);

    //         } else {
    //             // Others check uses
    //             for( int i=1; i<n.nIns(); i++ ) {
    //                 // This is a LRG use
    //                 // splitting in inner loop or at loop border
    //                 if( lrgSame( n.in( i ), lrg ) &&
    //                     (min == max || (n.in(i) instanceof MachNode mach && mach.isClone()) || n.cfg0().loopDepth() <= min) )
    //                     // Split before in this block
    //                     insertBefore( n, i, "use/loop/use", round,lrg, false );
    //             }
    //         }
    //     }
    //     return true;
    // }

    // private boolean lrgSame(Node x, LRG lrg) {
    //     LRG xlrg = lrg(x);
    //     while( xlrg==null && x instanceof SplitNode )
    //         xlrg=lrg(x = x.in(1));
    //     return xlrg==lrg;
    // }

    // private static long ldepth( long ld, Node n, CFGNode cfg ) {
    //     // Do not count splits
    //     if( n instanceof SplitNode ) return ld;
    //     // Collect min/max loop depth
    //     int min = (int)ld;
    //     int max = (int)(ld>>32);
    //     int d = cfg.loopDepth();
    //     // if n will lower the min loop and is in the tail end of the loop
    //     // header, splitting "around" the loop will not help.  Treat n as being
    //     // in the loop.
    //     if( d < min ) {
    //         if( cfg.uctrl() instanceof LoopNode loop && loop.entry()==cfg ) {
    //             for( int i=cfg.nOuts()-2; i>=0; i-- ) {
    //                 Node out = cfg.out(i);
    //                 if( n==out )
    //                     { d = loop.loopDepth(); break; } // Treat n as being "in the loop"
    //                 if( !((out instanceof MachNode mach && mach.isClone()) || out instanceof SplitNode ) )
    //                     break;  // Treat b as "normal", out of loop
    //             }
    //         }
    //     }

    //     // lower min, raise max, and re-fold
    //     min = Math.min(min,d);
    //     max = Math.max(max,d);
    //     return ((long)max<<32) | min;
    // }

    // // Find all members of a live range, both defs and uses
    // private final Ary<Node> _ns = new Ary<>(Node.class);
    // void findAllLRG( LRG lrg ) {
    //     _ns.clear();
    //     int wd = 0;
    //     _ns.push((Node)lrg._machDef);
    //     _ns.push((Node)lrg._machUse);
    //     while( wd < _ns._len ) {
    //         Node n = _ns.at(wd++);
    //         if( lrg(n)!=lrg ) continue;
    //         for( Node def : n._inputs )
    //             if( lrg(def)==lrg && _ns.find(def)== -1 )
    //                 _ns.push(def);
    //         for( Node use : n._outputs )
    //             if( _ns.find(use)== -1 )
    //                 _ns.push(use);
    //     }
    //     for( Node n : _ns ) assert !n.isDead();
    // }

    // void insertBefore(Node n, int i, String kind, byte round, LRG lrg, boolean skip) {
    //     Node def = n.in(i);
    //     // Effective block for use
    //     CFGNode cfg = n instanceof PhiNode phi ? phi.region().cfg(i) : n.cfg0();
    //     // Def is a split ?
    //     if( skip && def instanceof SplitNode ) {
    //         boolean singleReg = n instanceof MachNode mach && mach.regmap(i).size1();
    //         // Same block, multiple registers, split is only used by n,
    //         // assume this is good enough and do not split again.
    //         if( cfg==def.cfg0() && def.nOuts()==1 && !singleReg )
    //             return;
    //     }
    //     makeSplit(def,kind,round,lrg).insertBefore(n, i);
    //     // Skip split-of-split same block
    //     if( skip && def instanceof SplitNode && cfg==def.cfg0() )
    //         n.in(i).setDefOrdered(1,def.in(1));
    // }
    // void insertBefore(Node n, int i, String kind, byte round, LRG lrg) {
    //     insertBefore(n,i,kind,round,lrg,true);
    // }

    // // Replace uses of `def` with `split`, and insert `split` immediately after
    // // `def` in the basic block.
    // public void insertAfterAndReplace( Node split, Node def, boolean must ) {
    //     split.insertAfter(def);
    //     if( split.nIns()>1 ) split.setDef(1,def);
    //     for( int j=def.nOuts()-1; j>=0; j-- ) {
    //         Node use = def.out(j);
    //         if( use==split ) continue; // Skip self
    //         // Can we avoid a split of a split?  'this' split is used by
    //         // another split in the same block.
    //         if( !must && use instanceof SplitNode split2 && sameBlockNoClobber(split2) )
    //             continue;
    //         int idx = use._inputs.find(def);
    //         use.setDefOrdered(idx,split);
    //         if( j < def.nOuts() ) j++;
    //     }
    // }

    // private Node makeSplit( Node def, String kind, byte round, LRG lrg ) {
    //     Node split = def instanceof MachNode mach && mach.isClone()
    //         ? mach.copy()
    //         : _code._mach.split(kind,round,lrg);
    //     _lrgs.put(split,lrg);
    //     return split;
    // }
    // private SplitNode makeSplit( String kind, byte round, LRG lrg ) {
    //     SplitNode split = _code._mach.split(kind,round,lrg);
    //     _lrgs.put(split,lrg);
    //     return split;
    // }


    // // -----------------------
    // // POST PASS: Remove empty spills that biased-coloring made
    // private void postColor() {
    //     int maxReg = -1;
    //     for( CFGNode bb : _code._cfg ) { // For all ops
    //         if( bb instanceof FunNode fun )
    //             maxReg = -1;   // Reset for new function
    //         // Compute frame size, based on arguments and largest reg seen
    //         if( bb instanceof ReturnNode ret )
    //             ret.fun().computeFrameAdjust(_code,maxReg);
    //         // Raise frame size by max stack args passed, even if ignored
    //         if( bb instanceof CallEndNode cend )
    //             maxReg = Math.max(maxReg,cend._xslot);

    //         for( int j=0; j<bb.nOuts(); j++ ) {
    //             Node n = bb.out(j);
    //             if( lrg(n)!=null )
    //                 maxReg = Math.max(maxReg,lrg(n).reg+1);
    //             // Raise frame size by max stack args passed to New
    //             if( n instanceof NewNode nnn )
    //                 maxReg = Math.max(maxReg,nnn._xslot);

    //             if( !(n instanceof SplitNode ) ) continue;
    //             int defreg = lrg(n     ).reg;
    //             int usereg = lrg(n.in(1)).reg;
    //             // Attempt to bypass split
    //             if( defreg != usereg && splitBypass(bb,j,n,defreg) )
    //                 usereg = lrg(n.in(1)).reg;

    //             // Split has same reg?  Useless!  Can remove it!
    //             if( defreg == usereg ) {
    //                 n.removeSplit();
    //                 j--;
    //                 continue;
    //             }
    //             // Split is kept; count against split score
    //             splills++;
    //             spill_scaled += (1<<bb.loopDepth()*3);
    //             assert _spillScaled >= 0;
    //         }
    //     }
    // }

    // private boolean splitBypass( CFGNode bb, int j, Node lo, int defreg ) {
    //     // Attempt to bypass split
    //     Node hi = lo.in(1);
    //     while( true ) {
    //         if( !(hi instanceof SplitNode && lo.cfg0() == hi.cfg0()) )
    //             return false;
    //         if( lrg(hi.in(1)).reg==defreg )
    //             break;
    //         hi = hi.in(1);
    //     }
    //     // Check no clobbers
    //     for( int idx = j-1; bb.out(idx) != hi; idx++) {
    //         Node n = bb.out(idx);
    //         if( lrg(n)!=null && lrg(n).reg == defreg )
    //             return false;   // Clobbered
    //     }
    //     lo.setDefOrdered(1,hi.in(1));
    //     return true;
    // }


    // public boolean sameBlockNoClobber( SplitNode split ) {
    //     Node def = split.in(1);
    //     CFGNode cfg = def.cfg0();
    //     if( cfg != split.cfg0() ) return false; // Not same block
    //     // Get multinode head
    //     Node def0 = def instanceof ProjNode ? def.in(0) : def;
    //     int defreg = lrg(def).reg;
    //     if( defreg == -1 ) defreg = lrg(def).mask.firstReg();
    //     if( defreg == -1 ) return false; // no allowed registers -> clobbered
    //     for( int idx = cfg._outputs.find(split) -1; idx >= 0; idx-- ) {
    //         Node n = cfg.out(idx);
    //         if( n==def0 ) return true;    // No clobbers
    //         if( lrg(n) == lrg(def) ) return false; // Self conflict
    //         if( lrg(n)!=null && lrg(n).reg == defreg )
    //             return false;   // Clobbered
    //     }
    //     throw Utils.TODO();
    // }
};
