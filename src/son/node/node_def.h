#pragma once

#include "../../core/prelude.h"
#include "../../core/vec.h"
#include "../../core/pair.h"
#include "../../core/maybe.h"

#include "../../lang/op.h"
#include "../type.h"

#include "../../token/tokenizer.h"

#include "static.h"

struct Node;
typedef Node CFGNode; // semantically must be a cfg node
namespace node {
    u64 hash(Node*);
    bool cfg(Node* n);
    bool is_multinode(Node* n);
    Type* compute(Node* n);
    bool eq(Node* left, Node* right);
    bool glb(Node* n);
    Node* idealize(Node* n);
    Node* peephole(Node* n);
    bool pinned(Node* n);
    CFGNode* get_ctrl(Node* n);
    CFGNode* get_cfg_ctrl(CFGNode* n, u32 i);
    u32 ctrl_size(CFGNode* n);
    Op op(Node* n);
};

enum class NodeType {
    Undefined = 0,
    Scope,
    Split, // When compiling, sometimes need to split live ranges; really shouldn't be in this "ideal" collection, but oh well

    // Control
    Start, Stop, Ret,
    If, // Never, // both are NodeIf; semantically Never will always be false (used for handling infinite loops)
    Region, Loop, // both are NodeRegion; semantically different though
    CtrlProj,

    // Data
    Const,
    BinOp,
    UnOp,
    Phi, Proj,
    Load, Store, AllocA,
};

// Assume that *every* node is reachable from Start by *only* using `output` edges and from Stop by *only* using `input` edges
struct Node {
    u32 uid;
    NodeType nt;
    Vec<Node*> input; // use-def references; nullable, fixed length, ordered; for data nodes, `input[0]` is always ctrl
    Vec<Node*> output; // def-use references
    Vec<Node*> deps; // dependents; when optimizing this node, the dependents should also be optimized (during the iterative peeps)
    Type* type; // best known type of this node; if null, this node is dead (nonull for alive nodes)
    bool keepalive;
    bool locked;

    u32 cfgid; // assigned and used during `compute_idom` step; only defined for cfg nodes; index into the `dom` and other vectors

    inline static u32 uid_counter = 0;
    inline static mem::Arena* node_arena = nullptr;
    // inline static GVN gvn = {}; // global value numbering

    // CALL AT THE BEGINNING OF MAIN
    static void init(mem::Arena& arena) {
        Node::uid_counter = 0;
        Node::node_arena = &arena;
        // Node::gvn = GVN::create(arena);
    }

    static Node create(NodeType type) {
        Node::uid_counter++;
        return Node {
            .uid=Node::uid_counter, .nt=type,
            .input=Vec<Node*>::create(*Node::node_arena),
            .output=Vec<Node*>::create(*Node::node_arena),
            .deps=Vec<Node*>::create(*Node::node_arena),
            .type=type::pool.top,
            .keepalive=false, .locked=false
        };
    }

    /* Methods */
    // # WARNING 
    // Only ever call these methods on concrete Nodes that have been put on an arena. Don't call on dangling nodes. They will use their address.
    template <typename... Args>
    void push_inputs(Args&&... inputs) {
        (this->push_input(std::forward<Args>(inputs)), ...);
    }
    void push_input(Node* new_input) {
        input.push(new_input);
        if(new_input != nullptr) new_input->output.push(this);
    }
    void pop_input() {
        Node* last_input = input.pop();
        if(last_input != nullptr) {
            last_input->output.remove_first_of(this); // remove this from popped node's output
            // If we removed the last use, the old input is now dead
            if(last_input->is_unused()) {
                last_input->kill();
            }
        }
    }
    void pop_inputs(usize n) {
        for(usize i = 0; i < n; i++) this->pop_input();
    }
    // set given index in `this->input` to `new_input` and return `new_input`
    // kill the previous node if it becomes unused
    Node* set_input(usize index, Node* new_input) {
        Node* old_input = input[index];
        if(old_input == new_input) return this; // No change
        if(new_input != nullptr)
            new_input->output.push(this);
        // If the old input exists, remove a def->use edge
        if(old_input != nullptr) {
            old_input->output.remove_first_of(this); // remove this from last node's output
            // If we removed the last use, the old input is now dead
            if(old_input->is_unused())
                old_input->kill();
        }
        // Set the new_def over the old (killed) edge
        input[index] = new_input;
        // Return self for easy flow-coding
        return new_input;
    }
    void copy_inputs(Node* n) {
        assert(this->input.size == 0);
        this->input.push_slice(n->input.full_slice());
    }
    bool is_unused() {
        return output.empty() && !keepalive;
    }
    bool is_dead() {
        return this->is_unused() && input.empty() && type == nullptr;
    }
    void kill() {
        assert(this->is_unused()); // Has no uses, so it is dead
        this->pop_inputs(input.size); // Set all inputs to null, recursively killing unused Nodes
        type = nullptr; // Flag as dead
        assert(this->is_dead());
    }

    // Replace self with `other` in the graph, making `this` go dead
    void subsume(Node* other) {
        assert(other != this);
        while(output.size > 0) {
            Node* n = output.pop();
            u32 i = n->input.index_of(this);
            n->input[i] = other;
            other->output.push(n);
        }
        this->kill();
    }
    
    // helpters to stop DCE mid-parse
    void keep() { keepalive = true; }
    void unkeep() { keepalive = false; }

    // void unlock() {
    //     if(!locked) return;
    //     Node* old = Node::gvn.remove(ref(this));
    //     assert(old == this);
    //     locked = false;
    // }
    // void lock() {
    //     todo;
    //     if(locked) return;
    //     Node::gvn.insert(this);
    //     locked = true;
    // }

    Op op() {
        return node::op(this);
    }

    /* idom related functions */

    // get immidiate dominator of `n`
    // return nullptr if called on `NodeStart` or `NodeStop` without any returns
    CFGNode* idom() {
        assert(node::cfg_size > 0); // `compute_idom` has been called
        if(this->cfg()) return node::dom[cfgid];
        else            panic; //return this->ctrl(); // a data node's idom is its ctrl
    }
    // get the depth in the dominator tree
    u32 idepth() {
        assert(node::cfg_size > 0); // `compute_idom` has been called
        if(this->cfg()) return node::domdepth[cfgid];
        else            panic; //return this->ctrl()->idepth();
    }
    // get the loop depth
    u32 loop_depth() {
        assert(node::cfg_size > 0); // `compute_idom` has been called
        if(this->cfg()) return node::loopdepth[cfgid];
        else            panic; //return this->ctrl()->loop_depth();
    }

    /* Less generic functions that operate on generic Node*; just helpers to call those */

    bool cfg() { return node::cfg(this); }
    Type* compute() { return node::compute(this); }
    bool eq(Node* other) { return node::eq(this, other); }
    bool glb() { return node::glb(this); }
    Node* idealize() { return node::idealize(this); }
    Node* peephole() { return node::peephole(this); }
    bool pinned() { return node::pinned(this); }

    /* including ctrl getters and setters */
    
    // get (this) data node's ctrl OR get (this) cfg node's only ctrl (if it has singular)
    CFGNode* ctrl() {
        if(this->cfg()) {
            assert(this->ctrl_size() == 1);
            return node::get_cfg_ctrl(this, 0);
        } else {
            return node::get_ctrl(this);
        }
    }
    // get (this) cfg node's ith ctrl
    CFGNode* ctrl(u32 i) { return node::get_cfg_ctrl(this, i); }
    // get (this) cfg node's number of (ctrl) inputs
    u32 ctrl_size() { return node::ctrl_size(this); }

    void set_ctrl(CFGNode* new_ctrl) {
        assert(!this->pinned());
        assert(new_ctrl->cfg());
        this->set_input(0,new_ctrl);
    }

    /* reg alloc related */

    // insert `this` immediately after `input` in the same basic block.
    void insert_after(Node* input) {
        CFGNode* cfg = input->ctrl();
        u32 i = cfg->output.index_of(input) + 1;
        // if(node::is_multinode(input->ctrl())) { // TODO in simple, ->input[0] instead
        //     // assert(i == cfg->output.size+1); // TODO this is present in Simple, (in a different way, but still)
        //     assert(i <= cfg->output.size); // TODO this is the opposite of Simple's, but Claude says this..?
        //     i = cfg->output.index_of(input->ctrl()) + 1; // TODO in simple, ->input[0] instead
        //     assert(i != cfg->output.size); // TODO this is not in Simple, but cannot hurt to have
        // }

        // Claude says this is the right thing to do instead, in my case vvv
        // if(node::is_multinode(input)) {
        //     // input itself is a multinode head; skip past its projections
        //     while(i < cfg->output.size && cfg->output[i]->nt == NodeType::Proj) i++;
        // }

        while(cfg->output[i]->nt == NodeType::Phi) i++;
        cfg->output.insert(i, this);
        this->input[0] = cfg;
    }

    // Insert this in front of use.in(uidx) with this, and insert this immediately before use in the basic block.
    void insert_before(Node* output, u32 uidx) {
        CFGNode* cfg = output->ctrl();
        u32 i;
        if(output->nt == NodeType::Phi) {
            cfg = output->ctrl()->ctrl(uidx-1); // ARGHHGHGHH
            assert(cfg->output.size > 0);
            i = cfg->output.size - 1; // TODO why -1?
        } else {
            i = cfg->output.index_of(output);
            assert(i != cfg->output.size);
        }
        cfg->output.insert(i, this);
        this->input[0] = cfg;
        if(this->input.size > 1 && this->nt == NodeType::Split)
            set_input_ordered(1, output->input[uidx]);
        output->set_input_ordered(uidx, this);
    }

    void set_input_ordered(u32 idx, Node* input) {
        // If old is dying, remove from CFG ordered
        Node* old = this->input[idx];
        if(old != nullptr && old->output.size == 1) {
            CFGNode* cfg = old->ctrl();
            if(cfg != nullptr) {
                cfg->output.remove_first_of(old);
                old->input[0] = nullptr;
            }
        }
        this->set_input(idx, input);
    }

    void remove_split() {
        assert(this->nt == NodeType::Split);
        // Unlink from the block's output list (CFG ordering)
        CFGNode* cfg = this->ctrl();
        cfg->output.remove_first_of(this);
        this->input[0] = nullptr; // detach ctrl without killing it
        // Replace all uses of this split with the value it was copying
        assert(this->input.size > 1); // TODO in Simple, there's an if statement
        this->subsume(this->input[1]);
    }

    // Preserve CFG use-ordering when killing
    // TODO not sure why it's even needed.. but i guess might as well
    void kill_ordered() {
        CFGNode* cfg = this->ctrl();
        cfg->output.remove_first_of(this);
        this->input[0] = nullptr;
        this->kill();
    }
    
};