#pragma once

#include "../../core/prelude.h"
#include "../../son/node.h"
#include "register.h"

/** A Live Range
 * <p>
 *  A live range is a set of nodes and edges which must get the same register.
 *  Live ranges form an interconnected web with almost no limits on their
 *  shape.  Live ranges also gather the set of register constraints from all
 *  their parts.
 * <p>
 *  Most of the fields in this class are for spill heuristics, but there are a
 *  few key ones that define what a LRG is.  LRGs have a unique dense integer
 *  `_lrg` number which names the LRG.  New `_lrg` numbers come from the
 *  `RegAlloc._lrg_num` counter.  LRGs can be unioned together -
 *  (<a href="https://en.wikipedia.org/wiki/Disjoint-set_data_structure">this is the Union-Find algorithm</a>)
 *  - and when this happens the lower numbered `_lrg` wins.  Unioning only
 *  happens during `BuildLRG` and happens because either a `Phi` is forcing all
 *  its inputs and outputs into the same register, or because of a 2-address
 *  instruction.  LRGs have matching `union` and `find` calls, and a set
 *  `_leader` field.
 */
struct LRG {
    // Dense live range numbers
    u16 lrg;

    // U-F leader; null if leader (U-F = union-find)
    LRG* leader;

    // Choosen register
    Reg reg;

    // Count of single-register defs and uses
    u16 input_count, output_count;
    // Count of all defs, uses.  Mostly interested 1 vs many
    bool multi_input, multi_output;

    // A sample MachNode def in the live range
    Node* n_input, *n_output;
    u16 uidx; // n_output input TODO ??????

    // Some splits used in biased coloring
    Node* split_input, *split_output;

    // All the self-conflicting defs for this live range
    HSet<Node*> self_conflicts;

    // AND of all masks involved
    RegMask mask;

    // Adjacent Live Range neighbors.  Only valid during coloring
    Vec<LRG*> adj;

    static LRG create(u16 lrg) {
        return LRG { .lrg = lrg, .reg = Reg::UNDEFINED };
    }

    // More registers than neighbors
    bool low_degree() { return adj.size < mask.size(); }

    bool is_leader() { return leader == nullptr; }

    // Fast-path Find from the Union-Find algorithm
    // **get the leader**
    LRG* find_leader() {
        if(leader == nullptr) return this; // I am the leader
        if(leader->leader == nullptr) // I point to the leader
            return leader;
        return uf_rollup();
    }
    // TODO I have no idea what's happening here; also need to read about union-find
    // Slow-path rollup of U-F
    LRG* uf_rollup() {
        LRG* ldr = leader->leader;
        // Roll-up
        while(ldr->leader != nullptr) ldr = ldr->leader;
        LRG* l2 = this;
        while(l2 != ldr) {
            LRG* l3 = l2->leader;
            l2->leader = ldr;
            l2 = l3;
        }
        return ldr;
    }

    // Union `this` and `lrg`, keeping the lower numbered `lrg`.
    // Includes a number of fast-path cutouts.
    LRG* union_with(LRG* lrg) {
        assert(this->is_leader());
        if(lrg == nullptr) return this;
        lrg = lrg->find_leader();
        if(lrg == this) return this;
        return this->lrg < lrg->lrg ? this->merge_lrg(lrg) : lrg->merge_lrg(this);
    }
    // Union `this` and `lrg`, folding together all stats.
    LRG* merge_lrg(LRG* lrg) {
        // Set union-find leader
        lrg->leader = this;
        // Fold together stats
        if(this->n_input == nullptr) {
            this->n_input = lrg->n_input;
        } else if(lrg->n_input != nullptr) {
            if(this->n_input != lrg->n_input ) this->multi_input = true;
            if(this->input_count == 0)
                this->n_input = lrg->n_input;
            else if(this->n_input == this->n_input)
                this->input_count--;
        }
        this->input_count += lrg->input_count;
        this->multi_input |= lrg->multi_input;

        if(this->n_output == nullptr) {
            this->n_output = lrg->n_output;
            this->uidx = lrg->uidx;
        } else if(lrg->n_output != nullptr) {
            if(this->n_output != lrg->n_output) this->multi_output = true;
            if(this->output_count == 0) {
                this->n_output = lrg->n_output;
                this->uidx == lrg->uidx;
            } else if(this->n_output == lrg->n_output) {
                this->output_count--;
            }
        }
        this->output_count += lrg->output_count;
        this->multi_output |= lrg->multi_output;

        // Fold deepest Split
        this->split_input = this->deep_split(this->split_input, lrg->split_input);
        this->split_output = this->deep_split(this->split_output, lrg->split_output);

        // Fold together masks
        this->mask = this->mask & lrg->mask;
        return this;
    }

    // TODO what the heck is this doing exactly?
    Node* deep_split(Node* s0, Node* s1 ) {
        return s0 == nullptr || (s1 != nullptr && s0->ctrl()->loop_depth() < s1->ctrl()->loop_depth()) ? s1 : s0;
    }

    // Record any Mach def for spilling heuristics
    LRG* mach_input(Node* input, bool size1) {
        todo; // what is size1 even supposed to.. mean?
        if(n_input != nullptr && n_input != input) multi_input = true;
        if(n_input == nullptr || size1) n_input = input;
        if(size1) input_count++;
        if(input->nt == NodeType::Split)
            split_input = this->deep_split(split_input, input);
        return this;
    }

    // Record any Mach use for spilling heuristics
    LRG* mach_output(Node* output, u16 uidx, bool size1) {
        if(n_output != nullptr && n_output != output) multi_output = true;
        if(n_output == nullptr || size1) {
            n_output = output;
            uidx = uidx;
        }
        if(size1) output_count++;
        todo;
        if(output->nt == NodeType::Split)
            split_output = this->deep_split(split_output, output);
        return this;
    }

    bool has_split() { return split_input != nullptr || split_output != nullptr; }
    u8 size() { return mask.size(); }
    bool size1() { return mask.size1(); }

    void self_conflict(Node* input) {
        self_conflicts.add(input);
    }
};
