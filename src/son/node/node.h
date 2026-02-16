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
        return NodeBinOp::create(op::from_str(t.val), lhs, rhs, t);
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

    // Getters
    Node* rhs() { return self.input[0]; }
};

struct NodeScope {
    // self.input = [ctrl, ...]
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
    Node* ctrl() { return self.input[0]; }

    // Methods
    NodeScope* duplicate() {
        NodeScope* dup = NodeScope::create(*scope.scopes.arena, nullptr);

        // Our goals are:
        // 1) duplicate the name bindings of the ScopeNode across all stack levels
        // 2) Make the new ScopeNode a user of all the nodes bound
        // 3) Ensure that the order of defs is the same to allow easy merging
        
        dup->scope = scope.deep_clone();
        for(u32 i = 0; i < self.input.size; i++) {
            dup->self.push_input(self.input[i]);
        }
        return dup;
    }

    void push() { scope.push(); }
    void pop() { self.pop_inputs(scope.top_size()); scope.pop(); }
    // If doesn't exist, return nullptr
    Node* find(Str var_name) { if(!scope.contains(var_name)) return nullptr; return self.input[scope[var_name]]; }
    Node* update(Str var_name, Node* new_value) {
        if(!scope.contains(var_name)) { return nullptr; }
        usize var_index = scope[var_name];
        self.set_input(var_index, new_value);
        return new_value;
    }
    // TODO if shadowing, remove the old value I think
    Node* define(Str var_name, Node* new_value) {
        scope.define(var_name, self.input.size);
        self.push_input(new_value);
        return new_value;
    }
};

struct NodeProj {
    // self.input = [ctrl]
    Node self;
    u32 index;

    // Constructors
    static Node* create(u32 tuple_index, Node* ctrl) {
        assert(ctrl != nullptr);
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
struct NodeIf { // TODO
    // self.input = [ctrl, condition]
    Node self;
    // Constructors
    static NodeIf generated() {
        return NodeIf { .self=Node::empty(NodeType::If) };
    }
    static NodeIf from_token(Token t) {
        return NodeIf { .self=Node::from_token(NodeType::If, t) };
    }
    NodeIf* create(Node* ctrl, Node* condition) {
        NodeIf* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(ctrl);
        ptr->self.push_inputs(condition);
        return ptr;
    }
    // Getters
    Node* ctrl() { return self.input[0]; }
    Node* condition() { return self.input[1]; }
};

// Control merge
struct NodeRegion { // TODO
    // self.input = [ctrl, ctrl2, ...]
    Node self;
    // Constructors
    static NodeRegion generated() {
        return NodeRegion { .self=Node::empty(NodeType::Region) };
    }
    NodeRegion* create(Node* ctrl, Node* ctrl2) {
        NodeRegion* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(ctrl);
        ptr->self.push_inputs(ctrl2);
        return ptr;
    }
    // Getters
    Node* ctrl(u32 index) { return self.input[index]; }
};

struct NodePhi { // TODO
    // self.input = [region, input1, input2, ...]
    Node self;
    // Constructors
    static NodePhi generated() {
        return NodePhi { .self=Node::empty(NodeType::Phi) };
    }
    NodePhi* create(Node* ctrl, Node* data1, Node* data2) {
        NodePhi* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(ctrl);
        ptr->self.push_inputs(data1);
        ptr->self.push_inputs(data2);
        return ptr;
    }
    // Getters
    Node* region() { return self.input[0]; }
    // 0-indexed
    Node* data(u32 index) { return self.input[index+1]; }
};

struct NodeStop {

};

// TODO remove this garbage crutch
Node* START_NODE;
NodeScope* SCOPE_NODE;