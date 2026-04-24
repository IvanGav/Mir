#pragma once

#include "node.h"

// Something something https://www.youtube.com/watch?v=5Po0gxfE7LA 1:40:00 path hack makes extra anti-deps sometimes, but lets you doing another path of post-dominance; even used in C2 so probably fine, although the idea of it is kinda eh

// Global Code Motion algorithm
namespace gcm {
    void schedule_early(NodeStart* start); // forward decl
    void schedule_late(NodeStop* start); // forward decl

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

    // find the earliest possible placement (ctrl) for every node
    // sets each data node's ctrl to be earlies possible
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

    CFGNode* anti_dep(NodeLoad* load, Node* stblk, Node* defblk, Node* lca, Node* st) {
        // Walk store blocks "reach" from its scheduled location to its earliest
        for(; stblk != defblk->idom(); stblk = stblk->idom()) {
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
        for(Node* cfg = lca; early != nullptr && cfg != early->idom(); cfg = cfg->idom())
            cfg->anti = load->self.uid;
        // Walk load->mem uses, looking for Stores causing an anti-dep
        for(Node* mem : load->mem()->output) {
            switch(mem->nt) {
            case NodeType::Store: {
                NodeStore* st = (NodeStore*) mem;
                assert(late[st->self.uid] != nullptr);
                lca = anti_dep(load, late[st->self.uid], st->self.ctrl(), lca, (Node*) st);
                break;
            }
            case NodeType::AllocA: {
                NodeAllocA* st = (NodeAllocA*) mem;
                assert(late[st->self.uid] != nullptr);
                lca = anti_dep(load, late[st->self.uid], st->self.ctrl(), lca, (Node*)st);
                break;
            }
            case NodeType::Phi: {
                NodePhi* phi = (NodePhi*) mem;
                // Repeat anti-dep for matching Phi inputs.
                // No anti-dep edges but may raise the LCA.
                for(u32 i = 1; i < phi->self.input.size; i++)
                    if(mem->input[i] == load->mem())
                        lca = anti_dep(load, ((NodeRegion*)(phi->region()))->ctrl(i), load->mem()->ctrl(), lca, nullptr);
                break;
            }
            case NodeType::Load: // Loads do not cause anti-deps on other loads
            case NodeType::Ret: // Load must already be ahead of Return
            case NodeType::Region:
                break;
            case NodeType::Scope: panic; // Mem uses now on ScopeMin (What the heck is ScopeMin)
            default: panic;
            }
        }
        return lca;
    }
    
    // CFG block (aka ctrl) of `output`. Normally from `late`, except for Phis, which go to the matching Region input.
    Node* cfg_block_of(Node* n, Node* output, Node** late) {
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

    // return true when `lca` is a better CFG block than `best`
    bool better(CFGNode* lca, CFGNode* best) {
        // TODO make sure that conditions 1 and 2 don't clash..
        return (
            lca->loop_depth() < best->loop_depth() || // we want to move things out of the loops
            lca->idepth() > best->idepth() || // we want to put them inside of if statements when possible
            best->nt == NodeType::If // um, not sure what this is doing here; apparently anything is better than an `if`?
        );
    }

    // put node's best schedule in `late` and itself in `ns`
    void schedule_node_late(Node* n, Node** ns, Node** late) {
        // Walk uses, gathering the LCA (Least Common Ancestor) of uses
        CFGNode* early = n->ctrl(); // calculated in `schedule_early`, so earliest possible ctrl for this node
        assert(early != nullptr);
        assert(n->output.size > 0);
        CFGNode* lca = gcm::cfg_block_of(n, n->output[0], late); // just grab the first one
        // the lowest we can go is just above every output `n` has -> find idom of all outputs' cfg nodes (cfg blocks)
        for(Node* output : n->output) {
            lca = node::idom(gcm::cfg_block_of(n, output, late), lca);
        }

        // Loads may need anti-dependencies, raising their LCA
        if(n->nt == NodeType::Load)
            lca = gcm::find_anti_dep(lca, (NodeLoad*)n, early, late);

        // Walk up from the LCA to the early, looking for best place. 
        // Effectively, try to minimize the execution frequency.
        CFGNode* best = lca;
        // TODO why call idom here?? wouldn't it schedule 1 block higher than we found to be the lowest, but still possible?
        lca = lca->idom(); // Already found best for starting LCA
        // TODO again, why comparing to early->idom() and not just early?
        for(; lca != early->idom(); lca = lca->idom())
            if(gcm::better(lca, best)) best = lca;
        
        assert(best->nt != NodeType::If);
        // TODO cliff said this is a garbage hack to make java happy about CME (concurrent modification exception), so maybe remove later?
        ns  [n->uid] = n;
        late[n->uid] = best;
    }

    void walk_breadth(Node* stop, Node** ns, Node** late) {
        mem::Arena scratch = mem::Arena::create(4 MB);
        // Things on the worklist have some (but perhaps not all) outputs done
        Vec<Node*> work = Vec<Node*>::create(scratch);
        work.push(stop);

        while(!work.empty()) {
            Node* n = work.pop();
            assert(late[n->uid] == nullptr); // No double visit
            // These we know the late schedule of, and need to set early for loops
            if(n->cfg()) {
                // we want to get the head of a block we schedule, and n->ctrl() will always get the head when `n` is a tail
                late[n->uid] = node::is_block_head(n) ? n : n->ctrl(); // note that calling `ctrl(void)` on CFG nodes will assert they have only (CFG) input; just a minor error check
            } else if(n->pinned()) {
                // we know the only possible cfg block of a pinned node; pinned = `Phi` or `Proj`
                late[n->uid] = n->ctrl();
            } else {
                // All outputs done?
                for(Node* output : n->output) {
                    assert(output != nullptr);
                    if(late[output->uid] == nullptr) goto continue_outer; // Nope, await all uses done
                }

                // Loads need their memory inputs' uses also done
                // this also means 2 loads cannot be input/output to each other
                if(n->nt == NodeType::Load) {
                    NodeLoad* load = (NodeLoad*)n;
                    for(Node* memuse : load->mem()->output) {
                        // TODO check if correct because I have no clue rn
                        if(late[memuse->uid] == nullptr &&
                            // Load output directly defines memory
                            (memuse->type->ttype == TypeT::Mem ||
                            // Load output indirectly defines memory
                            (memuse->type->ttype == TypeT::Tuple && ((TypeTuple*)memuse->type)->val[load->mem_alias]->ttype == TypeT::Mem)) // TODO wtf
                        ) {
                            goto continue_outer;
                        }
                    }
                }

                // All uses done, schedule; note that only called for non-cfg and non-pinned nodes
                gcm::schedule_node_late(n,ns,late);
            }

            // Walk all inputs and put on worklist, as all of their outputs might now be done
            for(Node* input : n->input) {
                assert(input != nullptr); // should've been scheduled in `early`, and no other inputs may be nullptr (for now at least)
                if(late[input->uid] == nullptr) { // i'm assuming it's only needed because of load edges that we check in both directions
                    work.push(input);
                    // if the input has a load output, maybe the load can fire
                    for(Node* load : input->output)
                        if(load->nt == NodeType::Load && late[load->uid] == nullptr)
                            work.push(load);
                }
            }
                
            continue_outer:;
        }
    }

    // find the best possible placement (ctrl) for every node (after finding the latest possible)
    // sets each data node's ctrl to be best found
    void schedule_late(NodeStop* stop) {
        mem::Arena scratch = mem::Arena::create(10 MB);
        u32 num_nodes = Node::uid_counter;
        Node** late = scratch.alloc<Node*>(num_nodes); mem::zero(late, num_nodes);
        Node** ns = scratch.alloc<Node*>(num_nodes); mem::zero(ns, num_nodes);
        gcm::walk_breadth((Node*) stop, ns, late); // find best cfg block
        // Copy the best placement choice into the ctrl slot
        for(u32 i = 0; i < num_nodes; i++) {
            // if(ns[i] != nullptr && !(ns[i]->nt == NodeType::Proj))
            if(ns[i] != nullptr && !ns[i]->pinned()) // TODO ..right? why would it be everything that's pinned *but* not Proj?
                ns[i]->set_ctrl(late[i]);
        }
    }
}
