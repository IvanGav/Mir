#pragma once

#include "../prelude.h"

#include "node_def.h"
#include "scope.h"

namespace node {
    Node* peephole(Node*);
};

/* Specialized Nodes */

// All nodes will have several static "constructors" and a `create` method that will take an object and put it into the node arena.
// It's not ideal, I suppose, but it'll probably look nice. First, create an object. Then put it onto arena and add inputs.
// `self.input` should not really be accessed. Only from `Node*`, when it doesn't really care about *what* the input is.
// 2 most standard "constructors" are: 
// - generated (no token provided, this node is related to an optimization or not attached to any specific source code symbol)
// - from_token (token provided, this node is created directly from some source code symbol)

struct NodeStart {
    // self.input = []
    Node self;
    TypeTuple* args;

    // Consturctors
    static Node* create(Slice<Type*> args) {
        NodeStart node = NodeStart { 
            .self = Node::empty(NodeType::Start),
            .args = (TypeTuple*) type::pool.from_slice(args)
        };
        return node::peephole((Node*) Node::node_arena->push(node));
    }

    // Getters
    Type* arg(u32 index) { return args->val[index]; }
};

struct NodeRet {
    // self.input = [ctrl, data]
    Node self;

    // Constructors
    static Node* create(Node* ctrl, Node* data, Token t = Token::empty) {
        NodeRet node = { .self = Node::create(NodeType::Ret, t) };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, data);
        return node::peephole(ptr);
    }
    // Getters
    Node* ctrl() { return self.input[0]; }
    Node* expr() { return self.input[1]; }
};

struct NodeConst {
    // self.input = [ctrl]
    Node self;
    Type* val;

    // Constructors
    static Node* create(Type* value, Node* ctrl, Token t = Token::empty) {
        NodeConst node = { 
            .self = Node::create(NodeType::Const, t),
            .val = value
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl);
        return node::peephole(ptr);
    }

    // Getters
    Node* ctrl() { return self.input[0]; }
};

struct NodeBinOp {
    // self.input = [lhs, rhs]
    Node self;
    Op op;

    // Constructors
    static Node* create(Op op, Node* lhs, Node* rhs, Token t = Token::empty) {
        assert(op::binary(op));
        NodeBinOp node = { 
            .self = Node::create(node::type_of_op(op), t),
            .op = op
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(lhs, rhs);
        return node::peephole(ptr);
    }
    static Node* from_token(Node* lhs, Node* rhs, Token t) {
        return NodeBinOp::create(op::binary(t.val), lhs, rhs, t);
    }

    // Getters
    Node* lhs() { return self.input[0]; }
    Node* rhs() { return self.input[1]; }

    void swap_lhs_rhs() {
        // this shouldn't really be done, but oh well
        Node* temp = this->lhs();
        self.input[0] = self.input[1];
        self.input[1] = temp;
    }
};

struct NodeUnOp {
    // self.input = [rhs]
    Node self;
    Op op;

    // Constructors
    static Node* create(Op op, Node* rhs, Token t = Token::empty) {
        assert(op::unary(op));
        NodeUnOp node = { 
            .self = Node::create(node::type_of_op(op), t),
            .op = op
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(rhs);
        return node::peephole(ptr);
    }
    static Node* from_token(Node* rhs, Token t) {
        return NodeUnOp::create(op::unary(t.val), rhs, t);
    }

    // Getters
    Node* rhs() { return self.input[0]; }
};

struct NodeProj {
    // self.input = [ctrl]
    Node self;
    u32 index;

    // Constructors
    static Node* create(u32 tuple_index, Node* ctrl) {
        assert(ctrl != nullptr);
        assert(ctrl->type != nullptr);
        assert(ctrl->type->ttype == TypeT::Tuple);
        NodeProj node = { 
            .self = Node::create(NodeType::Proj),
            .index = tuple_index
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl);
        return node::peephole(ptr);
    }

    // Getters
    Node* ctrl() { return self.input[0]; }
};

// Control split
struct NodeIf {
    // self.input = [ctrl, condition]
    Node self;

    // Constructors
    static Node* create(Node* ctrl, Node* condition, Token t = Token::empty) {
        assert(ctrl != nullptr);
        NodeIf node = { 
            .self = Node::create(NodeType::If, t)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, condition);
        return node::peephole(ptr);
    }

    // Getters
    Node* ctrl() { return self.input[0]; }
    Node* condition() { return self.input[1]; }
};

// Control merge
struct NodeRegion {
    // self.input = [ctrl1, ctrl2, ...]
    Node self;

    // Constructors
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
    static Node* create_incomplete(Node* ctrl1) {
        assert(ctrl1 != nullptr);
        NodeRegion node = { 
            .self = Node::create(NodeType::Region)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl1, nullptr);
        return node::peephole(ptr);
    }

    // Getters
    Node* ctrl(u32 index) { return self.input[index]; }
    u32 ctrl_size() { return self.input.size; }
    // void set_ctrl(u32 index, Node* new_ctrl) { self.set_input(index, new_ctrl); }

    // If this region node was incomplete (aka loop in parsing), complete it by providing it the back-edge (true proj)
    void complete(Node* new_ctrl) {
        assert(this->ctrl(this->ctrl_size()-1) == nullptr);
        self.set_input(this->ctrl_size()-1, new_ctrl);
    }
    
    // Other helpers
    // return true if not yet constructed fully
    bool incomplete() {
        return this->ctrl(this->ctrl_size()-1) == nullptr;
    }
};

struct NodePhi {
    // self.input = [region, input1, input2, ...]
    Node self;

    // Constructors
    // TODO make vararg
    static Node* create(Node* ctrl, Node* data1, Node* data2) {
        assert(ctrl != nullptr);
        NodePhi node = { 
            .self = Node::create(NodeType::Phi)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, data1, data2);
        return node::peephole(ptr);
    }
    static Node* create(Node* ctrl, Slice<Node*> data_list) {
        assert(ctrl != nullptr);
        NodePhi node = { 
            .self = Node::create(NodeType::Phi)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl);
        for(u32 i = 0; i < data_list.size; i++) {
            ptr->push_input(data_list[i]);
        }
        return node::peephole(ptr);
    }
    static Node* create_incomplete(Node* ctrl, Node* data1) {
        assert(ctrl != nullptr);
        NodePhi node = { 
            .self = Node::create(NodeType::Phi)
        };
        Node* ptr = (Node*) Node::node_arena->push(node);
        ptr->push_inputs(ctrl, data1, nullptr);
        return node::peephole(ptr);
    }

    // Getters
    Node* region() { return self.input[0]; }
    // 0-indexed
    Node* data(u32 index) { return self.input[index+1]; }
    u32 data_size() { return self.input.size-1; }
    void set_data(u32 index, Node* new_data) {
        self.set_input(index+1, new_data);
    }
    // complete with the given data node
    void complete(Node* data2) {
        assert(this->self.input[2] == nullptr);
        this->self.set_input(2, data2);
    }
    // return true if not yet constructed fully
    bool incomplete() {
        return this->data(this->data_size()-1) == nullptr;
    }
    // return true if all data nodes' `node->nt` have the same value
    bool all_same() {
        NodeType nt = this->data(0)->nt;
        for(u32 i = 1; i < this->data_size(); i++) {
            if(this->data(i)->nt != nt) return false;
        }
        return true;
    }
};

struct NodeStop {

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
        ptr->self.type = type::pool.bottom();
        ptr->define(CTRL_STR, ctrl);
        return ptr;
    }

    // Getters
    Node* ctrl() { assert(self.input.size > 0); return self.input[0]; }

    // Methods
    NodeScope* duplicate() {
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
        NodeScope* dup = NodeScope::create(*scope.scopes.arena, this->ctrl());
        // Our goals are:  1) duplicate the name bindings of the ScopeNode across all stack levels  2) Make the new ScopeNode a user of all the nodes bound  3) Ensure that the order of defs is the same to allow easy merging
        dup->scope = scope.deep_clone();
        for(u32 i = 1; i < self.input.size; i++) {
            dup->self.push_input((Node*)this);
        }
        return dup;
    }

    // Return the NodeRegion that connects
    // note, kills the `other` scope and transforms the given scope
    // note, if either scope has more layers, bring it down to the smallest of the two
    Node* merge(NodeScope* other) {
        while(self.input.size > other->self.input.size) { this->pop(); }
        assert(self.input.size == other->self.input.size);
        this->update_ctrl(NodeRegion::create(this->ctrl(), other->ctrl()));
        Node* region = this->ctrl();
        // Note that we skip i==0, which is bound to '$ctrl'
        for(u32 i = 1; i < self.input.size; i++) {
            if(self.input[i] != other->self.input[i]) {
                Node* data1 = this->resolve_sentinel(i); //self.input[i];
                Node* data2 = other->resolve_sentinel(i); //other->self.input[i];
                self.set_input(i, NodePhi::create(region, data1, data2));
            }
        }
        ((Node*)other)->kill();
        return region;
    }

    void push() { scope.push(); }
    void pop() { self.pop_inputs(scope.top_size()); scope.pop(); }

    // If doesn't exist, return nullptr
    Node* find(Str var_name) {
        if(!scope.contains(var_name)) return nullptr;
        u32 index = (u32) scope[var_name];
        // if a sentinel, resolve it
        // A lazy phi needs to be created on first lookup; explained in README.md `# Explain`
        Node* val = this->resolve_sentinel(index);
        return val;
    }
    Node* update(Str var_name, Node* new_value) {
        assert(var_name != CTRL_STR);
        if(!scope.contains(var_name)) { return nullptr; }
        usize var_index = scope[var_name];
        // Node* _node = this->resolve_sentinel(var_index); // TODO not necessary; delete
        self.set_input(var_index, new_value);
        return new_value;
    }
    // TODO if shadowing, remove the old value I think
    Node* define(Str var_name, Node* new_value) {
        // no need to `resolve_sentinel` since can only define or redefine in the top scope = cannot encounter a sentinel
        // assert(var_name != CTRL_STR);
        scope.define(var_name, self.input.size);
        self.push_input(new_value);
        return new_value;
    }
    Node* update_ctrl(Node* new_value) {
        self.set_input(0, new_value);
        return new_value;
    }

    // if this->self.input[i] is a sentinel (points to a ScopeNode), insert a lazy phi in that scope and return the newly created phi
    // else return the current value
    Node* resolve_sentinel(u32 i) {
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
            // TODO check if correct because I have a feeling like it's not-quite-correct and will cause me 21 headaches
            phi = NodePhi::create_incomplete(sentinel->ctrl(), sentinel->resolve_sentinel(i));
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
        NodeRegion* cur_ctrl = (NodeRegion*)this->ctrl(); // technically unsafe
        assert(cur_ctrl->self.nt == NodeType::Region && cur_ctrl->incomplete());
        cur_ctrl->complete(back->ctrl());
        for(u32 i = 1; i < self.input.size; i++) {
            if(back->self.input[i] != (Node*)this) {
                // will be a lazy phi
                NodePhi* phi = (NodePhi*)self.input[i]; // technically might be unsafe, but should always be true
                assert(phi->self.nt == NodeType::Phi);
                assert(phi->region() == (Node*)cur_ctrl && phi->incomplete());
                phi->complete(back->self.input[i]);
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

Node* START_NODE;
NodeScope* SCOPE_NODE;
NodeScope* BREAK_SCOPE_NODE;
NodeScope* CONTINUE_SCOPE_NODE;