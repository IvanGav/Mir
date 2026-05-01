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
    bool is_load(Node* n);
    Node* mem_of_load(Node* n);
    u32 mem_alias_of_load(Node* n);
};

enum class NodeType {
    Undefined = 0,
    Scope,

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

    // x86; I'm sorry that they're here.. I just don't have the time to come up with a neater solution
    // R/I/M = Register/Immidiate/Memory
    
    // Jump; Control!!!!
    x86Jump, // generic conditional jump; what operation is decided by `this->op`
    // x86JumpZero, 
    // x86JumpNZero,
    // x86JumpOne, 
    // x86JumpNOne,
    // x86JumpEq, 
    // x86JumpNEq, 
    // x86JumpG, 
    // x86JumpGEq, 
    // x86JumpL, 
    // x86JumpLEq,

    // Set (use result of Cmp)
    x86SetEq, 
    x86SetNEq, 
    x86SetG, 
    x86SetGEq, 
    x86SetL, 
    x86SetLEq, // x86JumpZero, x86JumpNZero, x86JumpOne, x86JumpNOne,

    // Arithmetic
    x86AddR,
    x86AddI,
    x86AddM,
    x86SubR,
    x86SubI,
    x86SubM,
    x86MulR,
    x86MulI,
    x86MulM,
    x86DivR,
    x86DivM,

    // Compare
    x86CmpR,
    x86CmpI,
    x86CmpM,
    // x86CmpMI,

    // Stack
    x86Push, 
    x86Pop,

    // Memory
    x86Lea, // Load Effective Address
    x86Load,
    x86Store,

    // Other
    x86MovR,
    x86MovI,
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
    bool is_load() { return node::is_load(this); }
    Node* mem() { return node::mem_of_load(this); }
    u32 mem_alias() { return node::mem_alias_of_load(this); }

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
    
};

namespace node {
    NodeType x86_op_r(Op op) {
        switch(op) {
            case Op::Add: return NodeType::x86AddR;
            case Op::Sub: return NodeType::x86SubR;
            case Op::Mul: return NodeType::x86MulR;
            case Op::Div: return NodeType::x86DivR;
            case Op::Eq: case Op::Neq: case Op::Greater: case Op::GreaterEq: case Op::Less: case Op::LessEq: return NodeType::x86CmpR;
            default: todo;
        }
    }
    NodeType x86_op_i(Op op) {
        switch(op) {
            case Op::Add: return NodeType::x86AddI;
            case Op::Sub: return NodeType::x86SubI;
            case Op::Mul: return NodeType::x86MulI;
            // case Op::Div: return NodeType::x86DivI;
            case Op::Eq: case Op::Neq: case Op::Greater: case Op::GreaterEq: case Op::Less: case Op::LessEq: return NodeType::x86CmpI;
            default: todo;
        }
    }
    NodeType x86_op_m(Op op) {
        switch(op) {
            case Op::Add: return NodeType::x86AddM;
            case Op::Sub: return NodeType::x86SubM;
            case Op::Mul: return NodeType::x86MulM;
            case Op::Div: return NodeType::x86DivM;
            case Op::Eq: case Op::Neq: case Op::Greater: case Op::GreaterEq: case Op::Less: case Op::LessEq: return NodeType::x86CmpM;
            default: todo;
        }
    }
    NodeType x86_set_op(Op op) {
        switch(op) {
            case Op::Eq: NodeType::x86SetEq;
            case Op::Neq: NodeType::x86SetNEq;
            case Op::Greater: NodeType::x86SetG;
            case Op::GreaterEq: NodeType::x86SetGEq;
            case Op::Less: NodeType::x86SetL;
            case Op::LessEq: NodeType::x86SetLEq;
            default: todo;
        }
    }
};