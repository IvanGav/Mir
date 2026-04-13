#pragma once

#include "node.h"

// Global Code Motion algorithm
namespace gcm {
    void schedule_early(NodeStart* start); // forward decl
    void schedule_late(NodeStop* start); // forward decl

    // Assume no infinite loops TODO don't
    void build(NodeStart* start, NodeStop* stop) {
        gcm::schedule_early(start);
        gcm::schedule_late(stop);
    }

    void build_cfg_post_order(Node* n, BitSet& visit, Vec<Node*>& po) {
        if(!node::cfg(n) || visit[n->uid]) return; // already visited or not relevant

        visit.set(n->uid); // mark visited
        for(Node* output : n->output )
            build_cfg_post_order(output, visit, po);
        
        po.push(n); // add to list after all outputs = postorder
    }

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
                todo; //_doSchedLate(n,ns,late);
            }

            // Walk all inputs and put on worklist, as their last-use might now be done
            for( Node def : n._inputs )
                if( def!=null && late[def._nid]==null ) {
                    work.push(def);
                    // if the def has a load use, maybe the load can fire
                    for( Node ld : def._outputs )
                        if( ld instanceof LoadNode && late[ld._nid]==null )
                            work.push(ld);
                }
                
            end_outer:;
        }
    }

    // given a non-cfg node, schedule it as early as possible
    void schedule_node_early(Node* n, BitSet& visit) { todo; // need to make every node have "input[0]" to be the ctrl input
        if(n == nullptr || visit[n->uid]) return; // any cfg nodes would already be visited
        assert(!node::cfg(n)); // if some were not, that's an issue
        visit.set(n->uid);

        // Schedule not-pinned not-CFG inputs before self.  Since skipping
        // Pinned, this never walks the backedge of Phis (and thus spins around
        // a data-only loop, eventually attempting relying on some pre-visited-
        // not-post-visited data op with no scheduled control.

        // TODO in Simple, the loop goes over all elements (including input[0]), but has a null check (i left it in for now)
        // since input[0] (ctrl) can be null, it skips over it, but if it's executed, it returns right away. So that's weird.
        // Anyway, for now I'll just assume that starting at 1 is correct and will never hit `input[i] == nullptr`
        for(u32 i = 1; i < n->input.size; i++) {
            assert(n->input[i] != nullptr); // TODO remove
            // ^^^ this is not safe if going over *all* inputs
            if(n->input[i]->nt != NodeType::Phi) // TODO wait, why only phi? huh; i think i get it, but I'm leaving the message for now
                gcm::schedule_node_early(n->input[i], visit);
        }
        
        if(!node::pinned(n)) {
            // Schedule at deepest input
            Node* early = START_NODE; // Maximally early, lowest idepth
            if(n->input[0] != nullptr && node::cfg(n->input[0]))
                early = n->input[0]; // TODO I hate this a lot, but technically works for now

            for(u32 i = 1; i < n->input.size; i++) {
                // Place `n` at earliest possible = at the same place as it's deepest input.
                // The deepest input *has* to be this deep. So earlier than it is illegal.
                if(node::dom_depth(n->input[i]->input[0]) > node::dom_depth(early)) {
                    // TODO I once again immensely hate accessing "input[0]", but I'll fix that later
                    early = n->input[i]->input[0]; // Latest/deepest input
                }
            }
            n->set_input(0, early); // First place this can go; TODO I again hate it a bunch, but for now it'll suffice; btw, setting the ctrl to `early`
        }
    }

    // ------------------------------------------------------------------------
    // Visit all nodes in CFG Reverse Post-Order, essentially defs before uses
    // (except at loops).  Since defs are visited first - and hoisted as early
    // as possible, when we come to a use we place it just after its deepest
    // input.
    void schedule_early(NodeStart* start) {
        mem::Arena scratch = mem::Arena::create(1 MB);
        Vec<Node*> po = Vec<Node*>::create(scratch); // rpo = post order
        BitSet visit = BitSet::create(scratch);
        build_cfg_post_order((Node*) start, visit, po);

        // go in reverse over post order vec => reverse post order
        for(u32 i = po.size-1; i >= 0; i--) {
            Node* cfg = po[i];
            // node::loop_depth(cfg);
            for(Node* n : cfg->input)
                gcm::schedule_node_early(n, visit);
            if(cfg->nt == NodeType::Region)
                for(Node* phi : cfg->output)
                    if(phi->nt == NodeType::Phi)
                        gcm::schedule_node_early(phi, visit);
        }
    }

    // ------------------------------------------------------------------------
    void schedule_late(NodeStop* stop) {
        CFGNode[] late = new CFGNode[Node.UID()];
        Node[] ns = new Node[Node.UID()];
        // Breadth-first scheduling
        breadth(stop,ns,late);

        // Copy the best placement choice into the control slot
        for( int i=0; i<late.length; i++ )
            if( ns[i] != null && !(ns[i] instanceof ProjNode) )
                ns[i].setDef(0,late[i]);
    }

    private static void _doSchedLate(Node n, Node[] ns, CFGNode[] late) {
        // Walk uses, gathering the LCA (Least Common Ancestor) of uses
        CFGNode early = n.in(0) instanceof CFGNode cfg ? cfg : n.in(0).cfg0();
        assert early != null;
        CFGNode lca = null;
        for( Node use : n._outputs )
            if( use != null )
              lca = use_block(n,use, late)._idom(lca,null);

        // Loads may need anti-dependencies, raising their LCA
        if( n instanceof LoadNode load )
            lca = find_anti_dep(lca,load,early,late);

        // Walk up from the LCA to the early, looking for best place.  This is
        // the lowest execution frequency, approximated by least loop depth and
        // deepest control flow.
        CFGNode best = lca;
        lca = lca.idom();       // Already found best for starting LCA
        for( ; lca != early.idom(); lca = lca.idom() )
            if( better(lca,best) )
                best = lca;
        assert !(best instanceof IfNode);
        ns  [n._nid] = n;
        late[n._nid] = best;
    }

    // Block of use.  Normally from late[] schedule, except for Phis, which go
    // to the matching Region input.
    private static CFGNode use_block(Node n, Node use, CFGNode[] late) {
        if( !(use instanceof PhiNode phi) )
            return late[use._nid];
        CFGNode found=null;
        for( int i=1; i<phi.nIns(); i++ )
            if( phi.in(i)==n )
                if( found==null ) found = phi.region().cfg(i);
                else Utils.TODO(); // Can be more than once
        assert found!=null;
        return found;
    }


    // Least loop depth first, then largest idepth
    private static boolean better( CFGNode lca, CFGNode best ) {
        return lca.loopDepth() < best.loopDepth() ||
                (lca.idepth() > best.idepth() || best instanceof IfNode);
    }

    private static CFGNode find_anti_dep(CFGNode lca, LoadNode load, CFGNode early, CFGNode[] late) {
        // We could skip final-field loads here.
        // Walk LCA->early, flagging Load's block location choices
        for( CFGNode cfg=lca; early!=null && cfg!=early.idom(); cfg = cfg.idom() )
            cfg._anti = load._nid;
        // Walk load->mem uses, looking for Stores causing an anti-dep
        for( Node mem : load.mem()._outputs ) {
            switch( mem ) {
            case StoreNode st:
                assert late[st._nid]!=null;
                lca = anti_dep(load,late[st._nid],st.cfg0(),lca,st);
                break;
            case NewNode st:
                assert late[st._nid]!=null;
                lca = anti_dep(load,late[st._nid],st.cfg0(),lca,st);
                break;
            case PhiNode phi:
                // Repeat anti-dep for matching Phi inputs.
                // No anti-dep edges but may raise the LCA.
                for( int i=1; i<phi.nIns(); i++ )
                    if( phi.in(i)==load.mem() )
                        lca = anti_dep(load,phi.region().cfg(i),load.mem().cfg0(),lca,null);
                break;
            case LoadNode ld: break; // Loads do not cause anti-deps on other loads
            case ReturnNode ret: break; // Load must already be ahead of Return
            case ScopeMinNode ret: break; // Mem uses now on ScopeMin
            case NeverNode never: break;
            default: throw Utils.TODO();
            }
        }
        return lca;
    }

    //
    private static CFGNode anti_dep( LoadNode load, CFGNode stblk, CFGNode defblk, CFGNode lca, Node st ) {
        // Walk store blocks "reach" from its scheduled location to its earliest
        for( ; stblk != defblk.idom(); stblk = stblk.idom() ) {
            // Store and Load overlap, need anti-dependence
            if( stblk._anti==load._nid ) {
                lca = stblk._idom(lca,null); // Raise Loads LCA
                if( lca == stblk && st != null && st._inputs.find(load) == -1 ) // And if something moved,
                    st.addDef(load);   // Add anti-dep as well
                return lca;            // Cap this stores' anti-dep to here
            }
        }
        return lca;
    }

}
