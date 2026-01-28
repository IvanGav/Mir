#pragma once

#include "../../core/prelude.h"

#include "node_def.h"
#include "scope.h"

/* Specialized Nodes */

// All nodes will have several static "constructors" and a `create` method that will take an object and put it into the node arena.
// It's not ideal, I suppose, but it'll probably look nice. First, create an object. Then put it onto arena and add inputs.
// `self.input` should not really be accessed. Only from `Node*`, when it doesn't really care about *what* the input is.
// 2 most standard "constructors" are: 
// - generated (no token provided, this node is related to an optimization or not attached to any specific source code symbol)
// - from_token (token provided, this node is created directly from some source code symbol)

struct NodeStart {
    Node self;
    TypeTuple* args;
    // Constructors
    static NodeStart generated(TypeTuple* args) {
        return NodeStart { .self = Node::empty(NodeType::Start), .args = args };
    }
    static NodeStart with_args(Slice<Type*> args) {
        return NodeStart::generated(type::pool.get_tuple(TypeTuple { .self = Type::known(TypeT::Tuple), .val = args }));
    }
    NodeStart* create() {
        self.type = (Type*) args;
        return Node::node_arena->push(*this);
    }
    // Getters
    Type* arg(u32 index) { return ((TypeTuple*)this->args)->val[index]; }
};

struct NodeRet {
    // self.input = [ctrl, data]
    Node self;
    // Constructors
    static NodeRet generated() {
        return NodeRet { .self=Node::empty(NodeType::Ret) };
    }
    static NodeRet from_token(Token t) {
        return NodeRet { .self=Node::from_token(NodeType::Ret, t) };
    }
    NodeRet* create(Node* ctrl, Node* data) {
        NodeRet* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(ctrl, data);
        return ptr;
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
    static NodeConst generated(Type* value) {
        assert(value != nullptr);
        return NodeConst { .self=Node::empty(NodeType::Const), .val=value };
    }
    static NodeConst from_token(Type* value, Token t) {
        assert(value != nullptr);
        return NodeConst { .self=Node::from_token(NodeType::Const, t), .val=value };
    }
    NodeConst* create(Node* ctrl) {
        NodeConst* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(ctrl);
        return ptr;
    }
    // Getters
    Node* ctrl() { return self.input[0]; }
};

struct NodeBinOp {
    // self.input = [lhs, rhs]
    Node self;
    Op op;
    // Constructors
    static NodeBinOp generated(Op oper) {
        assert(op::binary(oper));
        return NodeBinOp { .self=Node::empty(node::type_of_op(oper)), .op=oper };
    }
    static NodeBinOp from_token(Token t, Op oper) {
        assert(op::binary(oper));
        return NodeBinOp { .self=Node::from_token(node::type_of_op(oper), t), .op=oper };
    }
    NodeBinOp* create(Node* lhs, Node* rhs) {
        NodeBinOp* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(lhs, rhs);
        return ptr;
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
    static NodeUnOp generated(Op oper) {
        assert(op::unary(oper));
        return NodeUnOp { .self=Node::empty(node::type_of_op(oper)), .op=oper };
    }
    static NodeUnOp from_token(Token t, Op oper) {
        assert(op::unary(oper));
        return NodeUnOp { .self=Node::from_token(node::type_of_op(oper), t), .op=oper };
    }
    NodeUnOp* create(Node* rhs) {
        NodeUnOp* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(rhs);
        return ptr;
    }
    // Getters
    Node* rhs() { return self.input[0]; }
};

struct NodeScope {
    // self.input = [ctrl, ...]
    Node self;
    VariableScope<usize> scope; // holds var_name -> index in self.input
    // Constructors
    static NodeScope with_arena(mem::Arena& arena) {
        NodeScope n = NodeScope { .self = Node::empty(NodeType::Scope), .scope = VariableScope<usize>::create(arena) };
        n.self.type = type::pool.bottom(); // NodeScope is a bit special and starts out as bottom type; I don't think it matters, but Simple has this so...
        return n;
    }
    NodeScope* create(Node* ctrl) {
        assert(ctrl != nullptr);
        NodeScope* ptr = Node::node_arena->push(*this);
        // ptr->self.push_inputs(ctrl);
        ptr->define("$ctrl"_s, ctrl);
        return ptr;
    }
    // Getters
    Node* ctrl() { return self.input[0]; }

    /* Methods */

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
    // Node* define_empty(Str var_name) {
    //     scope.define(var_name);
    //     return nullptr;
    // }
};

struct NodeProj {
    // self.input = [ctrl]
    Node self;
    u32 index;
    // Constructors
    static NodeProj generated(u32 tuple_index) {
        return NodeProj { .self = Node::empty(NodeType::Proj), .index = tuple_index };
    }
    NodeProj* create(Node* ctrl) {
        assert(ctrl != nullptr);
        assert(ctrl->type->ttype == TypeT::Tuple);
        NodeProj* ptr = Node::node_arena->push(*this);
        ptr->self.push_inputs(ctrl);
        return ptr;
    }
    // Getters
    Node* ctrl() { return self.input[0]; }
};

// TODO remove this garbage crutch
Node* START_NODE;
NodeScope* SCOPE_NODE;