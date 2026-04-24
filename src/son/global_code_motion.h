#pragma once

#include "node.h"

// Global Code Motion algorithm
namespace gcm {
    void schedule_early(NodeStart* start); // forward decl
    void schedule_late(NodeStop* start); // forward decl
    void schedule_node_late(Node* n, Node** ns, Node** late); // forward decl

    // Assume no infinite loops TODO don't
    void build(NodeStart* start, NodeStop* stop) {
        gcm::schedule_early(start);
        gcm::schedule_late(stop);
    }

    /* schedule early */

    // given a non-cfg node, schedule it as early as possible
    void schedule_node_early(Node* n, BitSet& visit) {
        if(n == nullptr || n->cfg() || visit[n->uid]) return;
        visit.set(n->uid);

        // Schedule not-pinned not-CFG inputs before self.  Since skipping pinned, this never walks the backedge of Phis.
        // skip index 0 because all data nodes are defined to have it as their ctrl (aka cfg node)
        for(u32 i = 1; i < n->input.size; i++) {
            assert(n->input[i] != nullptr);
            if(n->input[i]->nt != NodeType::Phi)
                gcm::schedule_node_early(n->input[i], visit);
        }
        
        // Pinned nodes already have ctrl assigned and are not allowed to move
        if(!node::pinned(n)) {
            // Schedule at deepest input
            Node* early = n->ctrl();
            if(early == nullptr) early = START_NODE; // should be true for most/all nodes, but still

            for(u32 i = 1; i < n->input.size; i++) {
                // Place `n` at earliest possible = at the same place as it's deepest input.
                // The deepest input *has* to be this deep. So earlier than it is illegal.
                if(n->input[i]->ctrl()->idepth() > early->idepth())
                    early = n->input[i]->ctrl(); // Latest/deepest input
            }

            n->set_ctrl(early); // Earliest place this can go
        }
        assert(n->ctrl() != nullptr);
    }

    // Visit all nodes in CFG Reverse Post-Order, essentially defs before uses
    // (except at loops).  Since defs are visited first - and hoisted as early
    // as possible, when we come to a use we place it just after its deepest
    // input.
    void schedule_early(NodeStart* start) {
        mem::Arena scratch = mem::Arena::create(1 MB);
        assert(node::cfg_size > 0); // `compute_idom` has been called
        Vec<Node*>& rpo = node::cfgrp; // rpo = reverse post order
        BitSet visit { .arena = &scratch };

        for(u32 i = 0; i < rpo.size; i++) {
            Node* cfg = rpo[i];
            for(Node* n : cfg->input)
                gcm::schedule_node_early(n, visit);
            if(cfg->nt == NodeType::Region)
                for(Node* phi : cfg->output)
                    if(phi->nt == NodeType::Phi)
                        gcm::schedule_node_early(phi, visit);
        }
    }

    /* schedule late */

    void build_cfg_breadth(Node* stop, Node** ns, Node** late) {
        mem::Arena scratch = mem::Arena::create(4 MB);
        // Things on the worklist have some (but perhaps not all) uses done.
        Vec<Node*> work = Vec<Node*>::create(scratch);
        work.push(stop);
        Node* n;

        while((n = work.pop()) != nullptr) {
            assert(late[n->uid] == nullptr); // No double visit
            // These we know the late schedule of, and need to set early for loops
            if(node::cfg(n)) {
                late[n->uid] = node::is_block_head(n) ? n : n->input[0]; // TODO this must be gone, later
            } else if(n->nt == NodeType::Phi) {
                late[n->uid] = ((NodePhi*)n)->region();
            } else if(n->nt == NodeType::Proj && node::cfg(n->input[0])) { // TODO remove this garbage
                late[n->uid] = n->input[0]; // TODO again, remove
            } else {
                // All uses done?
                for(Node* use : n->output) {
                    // TODO why null check? I thought outputs are supposed to be non-null
                    if(use != nullptr && late[use->uid] == nullptr) goto end_outer; // Nope, await all uses done
                }

                // Loads need their memory inputs' uses also done
                if(n->nt == NodeType::Load) {
                    NodeLoad* ld = (NodeLoad*)n;
                    for(Node* memuse : ld->mem()->output) {
                        if(late[memuse->uid] == nullptr &&
                            // Load output directly defines memory
                            (memuse->type->ttype == TypeT::Mem ||
                            // Load output indirectly defines memory
                            (memuse->type->ttype == TypeT::Tuple && ((TypeTuple*)memuse->type)->val[ld->mem_alias]->ttype == TypeT::Mem)) // TODO wtf
                        ) {
                            goto end_outer;
                        }
                    }
                }

                // All uses done, schedule
                gcm::schedule_node_late(n,ns,late);
            }

            // Walk all inputs and put on worklist, as their last output might now be done
            for(Node* input : n->input) {
                if(input != nullptr && late[input->uid]== nullptr) {
                    work.push(input);
                    // if the input has a load output, maybe the load can fire
                    for(Node* ld : input->output)
                        if(ld->nt == NodeType::Load && late[ld->uid] == nullptr)
                            work.push(ld);
                }
            }
                
            end_outer:;
        }
    }

    void schedule_late(NodeStop* stop) {
        mem::Arena scratch = mem::Arena::create(10 MB);
        u32 num_nodes = Node::uid_counter;
        Node** late = scratch.alloc<Node*>(num_nodes); mem::zero(late, num_nodes);
        Node** ns = scratch.alloc<Node*>(num_nodes); mem::zero(ns, num_nodes);
        // Breadth-first scheduling
        gcm::build_cfg_breadth((Node*) stop, ns, late);
        // Copy the best placement choice into the control slot
        for(u32 i = 0; i < num_nodes; i++)
            if(ns[i] != nullptr && !(ns[i]->nt == NodeType::Proj))
                ns[i]->set_input(0, late[i]);
    }

    // Block of `output`.  Normally from late[] schedule, except for Phis, which go
    // to the matching Region input.
    Node* use_block(Node* n, Node* output, Node** late) {
        if(output->nt != NodeType::Phi)
            return late[output->uid];
        NodePhi* phi = (NodePhi*) output;
        Node* found = nullptr;
        for(u32 i = 1; i < phi->self.input.size; i++) {
            if(phi->self.input[i] == n)
                if(found == nullptr) found = phi->region()->input[i];
                else todo; // Can be more than once
        }
        assert(found != nullptr);
        return found;
    }

    Node* anti_dep(NodeLoad* load, Node* stblk, Node* defblk, Node* lca, Node* st) {
        // Walk store blocks "reach" from its scheduled location to its earliest
        for(; stblk != node::idom(defblk); stblk = node::idom(stblk)) {
            // Store and Load overlap, need anti-dependence
            if(stblk->anti == load->self.uid) {
                lca = node::idom(stblk, lca); // Raise Loads LCA
                if(lca == stblk && st != nullptr && !st->input.contains((Node*)load)) // And if something moved,
                    st->push_input((Node*)load);   // Add anti-dep as well
                return lca;            // Cap this stores' anti-dep to here
            }
        }
        return lca;
    }

    Node* find_anti_dep(Node* lca, NodeLoad* load, Node* early, Node** late) {
        // We could skip final-field loads here.
        // Walk LCA->early, flagging Load's block location choices
        for(Node* cfg = lca; early != nullptr && cfg != node::idom(early); cfg = node::idom(cfg))
            cfg->anti = load->self.uid;
        // Walk load->mem uses, looking for Stores causing an anti-dep
        for(Node* mem : load->mem()->output) {
            switch(mem->nt) {
            case NodeType::Store: {
                NodeStore* st = (NodeStore*) mem;
                assert(late[st->self.uid] != nullptr);
                lca = anti_dep(load, late[st->self.uid], st->self.input[0], lca, (Node*) st); // TODO no input[0] garbage
                break;
            }
            case NodeType::AllocA: {
                NodeAllocA* st = (NodeAllocA*) mem;
                assert(late[st->self.uid] != nullptr);
                lca = anti_dep(load, late[st->self.uid], st->self.input[0], lca, (Node*)st); // TODO you know the drill
                break;
            }
            case NodeType::Phi: {
                NodePhi* phi = (NodePhi*) mem;
                // Repeat anti-dep for matching Phi inputs.
                // No anti-dep edges but may raise the LCA.
                for(u32 i = 1; i < phi->self.input.size; i++)
                    if(mem->input[i] == load->mem())
                        lca = anti_dep(load, ((NodeRegion*)(phi->region()))->ctrl(i), load->mem()->input[0], lca, nullptr); // TODO bleh
                break;
            }
            case NodeType::Load: // Loads do not cause anti-deps on other loads
            case NodeType::Ret: // Load must already be ahead of Return
            case NodeType::Region: break;
            case NodeType::Scope: panic; // Mem uses now on ScopeMin (What the heck is ScopeMin)
            default: panic;
            }
        }
        return lca;
    }

    // Least loop depth first, then largest idepth
    bool better(Node* lca, Node* best) {
        return node::loop_depth(lca) < node::loop_depth(best) || node::idepth(lca) > node::idepth(best) || best->nt == NodeType::If;
    }

    void schedule_node_late(Node* n, Node** ns, Node** late) {
        // Walk uses, gathering the LCA (Least Common Ancestor) of uses
        Node* early = node::cfg(n->input[0]) ? n->input[0] : n->input[0]->input[0]; // originally not `n->input[0]->input[0]` but `n->input[0]->cfg0()`
        assert(early != nullptr);
        Node* lca = nullptr;
        for(Node* output : n->output)
            if(output != nullptr)
              lca = node::idom(gcm::use_block(n, output, late), lca);

        // Loads may need anti-dependencies, raising their LCA
        if(n->nt == NodeType::Load)
            lca = find_anti_dep(lca, (NodeLoad*)n, early, late);

        // Walk up from the LCA to the early, looking for best place.  This is
        // the lowest execution frequency, approximated by least loop depth and
        // deepest control flow.
        Node* best = lca;
        lca = node::idom(lca);       // Already found best for starting LCA
        for(; lca != node::idom(early); lca = node::idom(lca))
            if(better(lca, best))
                best = lca;
        assert(best->nt != NodeType::If);
        ns  [n->uid] = n;
        late[n->uid] = best;
    }
}
