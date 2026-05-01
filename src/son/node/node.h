#pragma once

#include "../prelude.h"

#include "static.h"
#include "node_def.h"
#include "scope.h"

namespace node {
    Node* peephole(Node*);
};

/* cfg nodes */

struct NodeStart {
    // self.input = []
    Node self;
    TypeTuple* args;

    // Consturctors
    static Node* create(Slice<Type*> args) {
        NodeStart node = NodeStart { 
            .self = Node::create(NodeType::Start),
            .args = (TypeTuple*) type::pool.from_slice(args)
        };
        return node::peephole((Node*) Node::node_arena->push(node));
    }

    // Getters
    Type* arg(u32 index) const { return args->val[index]; }
};

struct NodeStop {
    // self.input = [ret1, ret2, ...]
    Node self;

    static Node* create() {
        NodeStop node = NodeStop { 
            .self = Node::create(NodeType::Stop)
        };
        return node::peephole((Node*) Node::node_arena->push(node));
    }

    CFGNode* ctrl(u32 index) {
        return self.input[index];
    }
    u32 ctrl_size() const {
        return self.input.size;
    }
};

struct NodeRet {
    // self.input = [ctrl, data]
    Node self;

    // Constructors
    static Node* create(CFGNode* ctrl, Node* data) {
        NodeRet node = { .self = Node::create(NodeType::Ret) };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, data);
        return node::peephole(ptr);
    }
    // Getters
    CFGNode* ctrl() { return self.input[0]; }
    Node* expr() { return self.input[1]; }
};


// Control split
struct NodeIf {
    // self.input = [ctrl, condition]
    Node self;

    // Constructors
    static Node* create(Node* ctrl, Node* condition) {
        assert(ctrl != nullptr);
        NodeIf node = { 
            .self = Node::create(NodeType::If)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, condition);
        return node::peephole(ptr);
    }

    // Getters
    CFGNode* ctrl() { return self.input[0]; }
    Node* condition() { return self.input[1]; }
};

// Control merge
// Can be NodeType::Region OR NodeType::Loop
struct NodeRegion {
    // self.input = [ctrl1, ctrl2, ...]
    Node self;

    // Constructors
    // creates a region (not loop) node
    static Node* create(Node* ctrl1, Node* ctrl2) {
        assert(ctrl1 != nullptr);
        assert(ctrl2 != nullptr);
        NodeRegion node = { 
            .self = Node::create(NodeType::Region)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl1, ctrl2);
        return node::peephole(ptr);
    }
    // creates a loop node
    static Node* create_incomplete(Node* ctrl1) {
        assert(ctrl1 != nullptr);
        NodeRegion node = { 
            .self = Node::create(NodeType::Loop)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl1, nullptr);
        return node::peephole(ptr);
    }

    // Getters
    CFGNode* ctrl(u32 index) { return self.input[index]; }
    u32 ctrl_size() { return self.input.size; }
    // void set_ctrl(u32 index, Node* new_ctrl) { self.set_input(index, new_ctrl); }

    // If this region node was incomplete (aka loop in parsing), complete it by providing it the back-edge (true proj)
    void complete(Node* new_ctrl) {
        assert(this->ctrl(this->ctrl_size()-1) == nullptr);
        self.set_input(this->ctrl_size()-1, new_ctrl);
    }
    
    // Other helpers
    // return true if not yet constructed fully
    bool is_incomplete() {
        return this->ctrl(this->ctrl_size()-1) == nullptr;
    }
};

/* either cfg or data */

// self.nt == Proj OR CtrlProj
// Note that "ctrl" may not be a literal cfg node, in the future. It is for now, but if I project the struct field values...
struct NodeProj {
    // self.input = [ctrl]
    Node self;
    u32 index;

    // Constructors
    static Node* create(u32 tuple_index, Node* ctrl, bool is_cfg) {
        assert(ctrl != nullptr);
        assert(ctrl->type != nullptr);
        assert(ctrl->type->ttype == TypeT::Tuple);
        NodeProj node = { 
            .self = Node::create(is_cfg ? NodeType::CtrlProj : NodeType::Proj),
            .index = tuple_index
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl);
        return node::peephole(ptr);
    }
    // projection of a Type::Ctrl
    static CFGNode* cfg_proj(u32 tuple_index, CFGNode* ctrl) {
        return NodeProj::create(tuple_index, ctrl, true);
    }
    // projection of some data type
    static Node* data_proj(u32 tuple_index, Node* ctrl) {
        return NodeProj::create(tuple_index, ctrl, false);
    }

    // Getters
    CFGNode* ctrl() {
        assert(self.input[0]->cfg()); // at some point may be false; catch if ever happens
        return self.input[0];
    }
};

/* data nodes */

struct NodeConst {
    // self.input = [ctrl]
    Node self;
    Type* val;

    // Constructors
    static Node* create(Type* value) {
        NodeConst node = { 
            .self = Node::create(NodeType::Const),
            .val = value
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(START_NODE);
        return node::peephole(ptr);
    }
    static Node* create(i64 value) { return NodeConst::create(type::pool.int_const(value)); }

    // Getters
    CFGNode* ctrl() { return self.input[0]; }
};

struct NodeBinOp {
    // self.input = [ctrl, lhs, rhs]
    Node self;
    Op op;

    // Constructors
    static Node* create(Op op, Node* lhs, Node* rhs) {
        assert(op::binary(op));
        NodeBinOp node = { 
            .self = Node::create(NodeType::BinOp),
            .op = op
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(nullptr, lhs, rhs);
        return node::peephole(ptr);
    }
    static Node* from_token(Node* lhs, Node* rhs, Token t) {
        return NodeBinOp::create(op::binary(t.val), lhs, rhs);
    }

    // Getters
    CFGNode* ctrl() { return self.input[0]; }
    Node* lhs() { return self.input[1]; }
    Node* rhs() { return self.input[2]; }

    // Methods
    void swap_lhs_rhs() {
        Node* temp = self.input[1];
        self.input[1] = self.input[2];
        self.input[2] = temp;
    }
};

struct NodeUnOp {
    // self.input = [ctrl, rhs]
    Node self;
    Op op;

    // Constructors
    static Node* create(Op op, Node* rhs) {
        assert(op::unary(op));
        NodeUnOp node = { 
            .self = Node::create(NodeType::UnOp),
            .op = op
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(nullptr, rhs);
        return node::peephole(ptr);
    }
    static Node* from_token(Node* rhs, Token t) {
        return NodeUnOp::create(op::unary(t.val), rhs);
    }

    // Getters
    CFGNode* ctrl() { return self.input[0]; }
    Node* rhs() { return self.input[1]; }
};

struct NodePhi {
    // self.input = [region(ctrl), input1, input2, ...]
    Node self;
    Str debug_var_name;

    // Constructors
    // TODO make vararg
    static Node* create(Str debug_var_name, Node* ctrl, Node* data1, Node* data2) {
        assert(ctrl != nullptr);
        NodePhi node = { 
            .self = Node::create(NodeType::Phi),
            .debug_var_name = debug_var_name
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, data1, data2);
        return node::peephole(ptr);
    }
    static Node* create(Str debug_var_name, Node* ctrl, Slice<Node*> data_list) {
        assert(ctrl != nullptr);
        NodePhi node = { 
            .self = Node::create(NodeType::Phi),
            .debug_var_name = debug_var_name
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl);
        for(u32 i = 0; i < data_list.size; i++) {
            ptr->push_input(data_list[i]);
        }
        return node::peephole(ptr);
    }
    static Node* create_incomplete(Str debug_var_name, Node* ctrl, Node* data1) {
        assert(ctrl != nullptr);
        NodePhi node = { 
            .self = Node::create(NodeType::Phi),
            .debug_var_name = debug_var_name
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, data1, nullptr);
        return node::peephole(ptr);
    }

    // Getters
    Node* region() { return self.input[0]; }
    Node* ctrl() { return this->region(); }
    // 0-indexed
    Node* data(u32 index) { return self.input[index+1]; }
    u32 data_size() { return self.input.size-1; }
    void set_data(u32 index, Node* new_data) {
        self.set_input(index+1, new_data);
    }
    // complete with the given data node
    void complete(Node* data2) {
        assert(this->self.input[this->data_size()] == nullptr);
        this->self.set_input(this->data_size(), data2);
    }
    // return true if not yet constructed fully
    bool is_incomplete() {
        return this->data(this->data_size()-1) == nullptr;
    }
    // return true if all data nodes' `node->nt` have the same value
    bool all_same() {
        NodeType nt = this->data(0)->nt;
        for(u32 i = 1; i < this->data_size(); i++) {
            assert(this->data(i) != nullptr); // shouldn't call on incomplete nodes
            if(this->data(i)->nt != nt) return false;
        }
        return true;
    }
    // if all data inputs are the same, return the unique input; nullptr otherwise
    Node* single_unique_input() {
        // TODO if region has dead control, delete
        Node* single = this->data(0);
        for(u32 i = 1; i < this->data_size(); i++) {
            // TODO check that this input is not dead (aka `this->region()->input[i]->type != type::pool.xctrl()`)
            // don't count self as a unique input (for example if in a loop a var is accessed but not modified)
            if(this->data(i) != (Node*)this) {
                if(single != this->data(i)) return nullptr;
            }
        }
        return single;
    }
};

struct NodeLoad {
    // self.input = [ctrl, mem (mem), base (ptr), offset (i64)]
    Node self;
    u32 mem_alias;
    Type* decl_type;

    static Node* create(u32 alias, Node* mem, Node* ptr, Node* offset) {
        NodeLoad node = {
            .self = Node::create(NodeType::Load),
            .mem_alias = alias,
            .decl_type = type::pool.int_sized(4) // TODO hardcoded
        };
        Node* nptr = (Node*) Node::node_arena->push(node);
        nptr->push_inputs(nullptr, mem, ptr, offset);
        return node::peephole(nptr);
    }

    CFGNode* ctrl() { return self.input[0]; }
    Node* mem() { return self.input[1]; }
    Node* ptr() { return self.input[2]; }
    Node* off() { return self.input[3]; }
};

struct NodeStore {
    // self.input = [ctrl, mem (mem), base (ptr), offset (i64), val]
    Node self;
    u32 mem_alias;
    Type* decl_type;

    static Node* create(u32 alias, Node* mem, Node* ptr, Node* offset, Node* val) {
        NodeStore node = {
            .self = Node::create(NodeType::Store),
            .mem_alias = alias,
            .decl_type = type::pool.int_sized(4) // TODO hardcoded
        };
        Node* nptr = (Node*) Node::node_arena->push(node);
        nptr->push_inputs(nullptr, mem, ptr, offset, val);
        return node::peephole(nptr);
    }

    CFGNode* ctrl() { return self.input[0]; }
    Node* mem() { return self.input[1]; }
    Node* ptr() { return self.input[2]; }
    Node* off() { return self.input[3]; }
    Node* val() { return self.input[4]; }
};

struct NodeAllocA {
    // self.input = [ctrl, alloc_size, init_mem]
    Node self;
    TypePtr* ptr;

    static Node* create(Type* decl_type, Node* ctrl, Node* alloc_size, Node* init_mem) {
        assert(decl_type->ttype == TypeT::Ptr);
        NodeAllocA node = {
            .self = Node::create(NodeType::AllocA),
            .ptr = (TypePtr*) decl_type
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, alloc_size, init_mem);
        return node::peephole(ptr);
    }

    CFGNode* ctrl() { return self.input[0]; }
    Node* size() { return self.input[1]; }
    Node* mem() { return self.input[2]; }
};

struct NodeScope {
    // self.input = [ctrl, ...]
    // note, self.input[i>0] can be NodeScope*; in that case it's a "sentinel"; read `# Explain` in README.md
    Node self;
    VariableScope<usize> scope; // holds var_name -> index in self.input

    // Constructors
    static NodeScope* create(mem::Arena& arena, Node* ctrl) {
        NodeScope node = NodeScope { .self = Node::create(NodeType::Scope), .scope = VariableScope<usize>::create(arena) };
        NodeScope* ptr = Node::node_arena->push(node);
        ptr->self.type = type::pool.ctrl; // maybe has to be Pure:Bottom?
        ptr->define(CTRL_STR, ctrl);
        return ptr;
    }
    // dead scopes accept any variable name lookup and return nothing (Pure:Bottom)
    // dead scope's ctrl is self
    static NodeScope* create_xctrl() {
        NodeScope node = NodeScope { .self = Node::create(NodeType::Scope) };
        NodeScope* ptr = Node::node_arena->push(node);
        ptr->self.type = type::pool.xctrl;
        return ptr;
    }

    // Getters
    CFGNode* ctrl() { assert(self.input.size > 0); return self.input[0]; }
    bool is_xctrl() { return self.type == type::pool.xctrl; }

    // Methods
    NodeScope* duplicate() {
        if(this->is_xctrl()) { return NodeScope::create_xctrl(); }
        NodeScope* dup = NodeScope::create(*scope.scopes.arena, this->ctrl());
        // Our goals are:  1) duplicate the name bindings of the ScopeNode across all stack levels  2) Make the new ScopeNode a user of all the nodes bound  3) Ensure that the order of defs is the same to allow easy merging
        dup->scope = scope.deep_clone();
        for(u32 i = 1; i < self.input.size; i++) {
            dup->self.push_input(self.input[i]);
        }
        return dup;
    }
    // the duplocated NodeScope will have all of the bindings pointing to a different scope that has the actual value
    NodeScope* duplicate_with_sentinel() {
        if(this->is_xctrl()) { return NodeScope::create_xctrl(); }
        NodeScope* dup = NodeScope::create(*scope.scopes.arena, this->ctrl());
        dup->scope = scope.deep_clone();
        for(u32 i = 1; i < self.input.size; i++) {
            dup->self.push_input((Node*)this);
        }
        return dup;
    }

    // merges `other` into `this` and kills `other`
    // note, if either scope has more layers, bring it down to the smallest of the two
    void merge(NodeScope* other) {
        assert(!self.is_dead()); assert(!other->self.is_dead());
        if(other->is_xctrl()) { return; }
        if(this->is_xctrl()) {
            self.kill();
            *this = *other;
            *other = { 0 };
            return;
        }
        if(other == this) { return; }
        while(self.input.size > other->self.input.size) { this->pop(); }
        this->update_ctrl(NodeRegion::create(this->ctrl(), other->ctrl()));
        Node* region = this->ctrl();
        // Note that we skip i==0, which is bound to '$ctrl'
        for(u32 i = 1; i < self.input.size; i++) {
            if(self.input[i] != other->self.input[i]) {
                Node* data1 = this->resolve_sentinel(i);
                Node* data2 = other->resolve_sentinel(i);
                self.set_input(i, NodePhi::create(scope.key_of(i), region, data1, data2));
            }
        }
        ((Node*)other)->kill();
        node::peephole(region);
    }

    void push() { if(this->is_xctrl()) { return; } scope.push(); }
    void pop() { if(this->is_xctrl()) { return; } self.pop_inputs(scope.top_size()); scope.pop(); }

    // If doesn't exist, return nullptr
    Node* find(Str var_name) {
        if(this->is_xctrl()) { return VOID_NODE; }
        if(!scope.contains(var_name)) return nullptr;
        u32 index = (u32) scope[var_name];
        // if a sentinel, resolve it
        // A lazy phi needs to be created on first lookup; explained in README.md `# Explain`
        Node* val = this->resolve_sentinel(index);
        return val;
    }
    Node* update(Str var_name, Node* new_value) {
        if(this->is_xctrl()) { return VOID_NODE; }
        assert(var_name != CTRL_STR);
        if(!scope.contains(var_name)) { return nullptr; }
        usize var_index = scope[var_name];
        // Node* _node = this->resolve_sentinel(var_index); // TODO not necessary; delete
        self.set_input(var_index, new_value);
        return new_value;
    }
    // TODO if shadowing, remove the old value I think
    Node* define(Str var_name, Node* new_value) {
        if(this->is_xctrl()) { return VOID_NODE; }
        // no need to `resolve_sentinel` since can only define or redefine in the top scope = cannot encounter a sentinel
        scope.define(var_name, self.input.size);
        self.push_input(new_value);
        return new_value;
    }
    void update_ctrl(Node* new_ctrl) {
        if(this->is_xctrl()) { return; }
        self.set_input(0, new_ctrl);
    }

    // if this->self.input[i] is a sentinel (points to a ScopeNode), insert a lazy phi in that scope and return the newly created phi
    // else return the current value
    Node* resolve_sentinel(u32 i) {
        if(this->is_xctrl()) { return VOID_NODE; }
        Node* old = self.input[i];
        if(old->nt != NodeType::Scope) return old; // not a sentinel; do nothing
        NodeScope* sentinel = (NodeScope*)old;
        Node* maybe_phi = sentinel->self.input[i]; // TODO maybe do resolve_sentinel here??? I don't think so, but maybe
        Node* phi; // to be assigned
        if(maybe_phi->nt == NodeType::Phi && sentinel->ctrl() == ((NodePhi*)maybe_phi)->region()) {
            // lazy phi was already created
            phi = sentinel->self.input[i];
        } else {
            // create lazy phi
            phi = NodePhi::create_incomplete(scope.key_of(i), sentinel->ctrl(), sentinel->resolve_sentinel(i));
            sentinel->self.set_input(i, phi);
        }
        self.set_input(i, phi);
        return phi;
    }

    // Merge the backedge scope into this loop head scope
    // We set the second input to the phi from the back edge (loop body)
    // `this` = original, untouched scope; must be the sentinel for both other scopes
    // `back` = "continue" scope (killed after the function returns)
    // `exit` = "break" scope
    void end_loop(NodeScope* back, NodeScope* exit) {
        // TODO problem when back or exit is xctrl
        assert(!self.is_dead() && !this->is_xctrl()); // head must be alive
        assert(back != this && back != exit && exit != this);
        NodeRegion* cur_ctrl = (NodeRegion*)this->ctrl(); // technically unsafe
        assert(cur_ctrl->self.nt == NodeType::Loop && cur_ctrl->is_incomplete());
        cur_ctrl->complete(back->ctrl());
        for(u32 i = 1; i < self.input.size; i++) {
            if(back->self.input[i] != (Node*)this) {
                // will be a lazy phi OR something else if assigned but never accessed inside the loop
                if(self.input[i]->nt == NodeType::Phi) {
                    NodePhi* phi = (NodePhi*)self.input[i]; // technically might be unsafe, but should always be true
                    assert(phi->region() == (Node*)cur_ctrl && phi->is_incomplete());
                    phi->complete(back->self.input[i]);
                } else {
                    assert(back->self.input[i] != nullptr);
                    self.set_input(i, back->self.input[i]);
                }
            }
            if(exit->self.input[i] == (Node*)this) // Replace a lazy-phi on the exit path too
                exit->self.set_input(i, self.input[i]);
        }
        back->self.kill(); // Loop backedge is dead

        // Now one-time do a useless phi removal
        for(u32 i = 1; i < self.input.size; i++) {
            if(self.input[i]->nt == NodeType::Phi) {
                Node* phi = self.input[i];
                Node* in = node::peephole(phi);
                if(in != phi) {
                    phi->subsume(in);
                    self.set_input(i, in); // Set the update back into Scope
                    // TODO uhh, what about the `exit` scope?
                }
            }
        }
    }
};

// ________________- x86 nodes -__________________
// Instead of adding inputs normally, they do `this->input.push(...)`.
// Sometimes include x86 nodes, sometimes include ideal nodes as inputs. Most ideal 
// nodes that are not ctrl should actually be exchanged for x86 nodes later.
// It's assumed that the caller (aka ideal->x86 translator) will later link them manually.

// operator that takes in 2 registers
struct x86NodeOpR {
    // self.input = [ctrl, lhs, rhs]
    Node self;

    // Constructors
    // given a well formed BinOp node, inherit its inputs and other properties; when given a logical binop, make a Cmp op
    static Node* create(Node* op) {
        assert(op->nt == NodeType::BinOp);
        x86NodeOpR self = { Node::create(node::x86_op_r(op->op())) };
        Node* nptr = (Node*) Node::node_arena->push(self);
        nptr->copy_inputs(op); // will copy [ctrl, lhs, rhs]
        nptr->type = op->type;
        return nptr;
    }
};

// operator that takes in a register and an immidiate
struct x86NodeOpI {
    // self.input = [ctrl, lhs]
    Node self;
    i32 imm;

    // Constructors
    // given a well formed BinOp node, inherit its inputs and other properties; when given a logical binop, make a Cmp op
    static Node* create(Node* op) {
        assert(op->nt == NodeType::BinOp);
        NodeBinOp* binop = (NodeBinOp*)op;
        assert(binop->rhs()->nt == NodeType::Const);
        x86NodeOpI self = { .self = Node::create(node::x86_op_i(binop->op)), .imm = ((TypeInt*)((NodeConst*)binop->rhs())->val)->val() };
        Node* nptr = (Node*) Node::node_arena->push(self);
        nptr->copy_inputs(op); // will copy [ctrl, lhs, rhs]
        nptr->pop_input(); // will pop rhs since it's a const that's been recorded as self.imm
        nptr->type = op->type;
        return nptr;
    }
};

// operator that takes in a register and memory
// [ptr + index << scale + off]
struct x86NodeOpM {
    // self.input = [ctrl, lhs]
    Node self;
    i32 off;
    u8 scale;

    // Constructors
    // given a well formed BinOp node, inherit its inputs and other properties; when given a logical binop, make a Cmp op
    static Node* create(Node* op) {
        todo;
        assert(op->nt == NodeType::BinOp);
        NodeBinOp* binop = (NodeBinOp*)op;
        assert(binop->rhs()->nt == NodeType::Const);
        x86NodeOpM self = { .self = Node::create(node::x86_op_m(binop->op)) };
        Node* nptr = (Node*) Node::node_arena->push(self);
        nptr->copy_inputs(op); // will copy [ctrl, lhs, rhs]
        nptr->pop_input(); // will pop rhs since it's a const that's been recorded as self.imm
        nptr->type = op->type;
        return nptr;
    }
};

struct x86NodeMov {
    // self.input = [ctrl]
    Node self;
    i64 imm;

    // Constructors
    // move immidiate up to 64 bit
    static Node* imm(NodeConst* imm) {
        assert(imm->self.nt == NodeType::Const);
        assert(imm->self.type->ttype == TypeT::Int);
        x86NodeMov self = { .self = Node::create(NodeType::x86MovI), .imm = ((TypeInt*)(imm)->val)->val() };
        Node* nptr = (Node*) Node::node_arena->push(self);
        nptr->copy_inputs((Node*)imm); // will copy [ctrl]
        nptr->type = imm->self.type;
        return nptr;
    }
};

// Conditional jump
struct x86NodeJmp {
    // self.input = [ctrl, cond]
    Node self;
    NodeType op; // jump on what operation; will be `NodeType::x86SetXXX` if `cond` is `x86CmpX` or `NodeType::Undefined` if `cond` is a `x86SetXXX` by itself

    // Constructors
    static Node* create(NodeIf* n) {
        assert(n->self.nt == NodeType::If);
        x86NodeJmp self = { .self = Node::create(NodeType::x86Jump) };
        Node* nptr = (Node*) Node::node_arena->push(self);
        nptr->copy_inputs((Node*)n); // will copy [ctrl, cond]
        nptr->type = n->self.type;
        return nptr;
    }
};

// Use the output of a comparison without a jump
struct x86NodeSet {
    // self.input = [ctrl, cmp]
    Node self;

    // Constructors
    static Node* create(Node* cmp, NodeBinOp* op) {
        assert(cmp->nt == NodeType::x86CmpR || cmp->nt == NodeType::x86CmpI || cmp->nt == NodeType::x86CmpM);// || cmp->nt == NodeType::x86CmpMI);
        x86NodeSet self = { .self = Node::create(node::x86_set_op(op->op)) };
        Node* nptr = (Node*) Node::node_arena->push(self);
        nptr->input.push(op->ctrl());
        nptr->input.push(cmp);
        nptr->type = cmp->type; // TODO maybe `op` instead?
        return nptr;
    }
};
