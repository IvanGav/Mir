#pragma once

// 1-1 translated from Simple ch8. I just need it for testing.

#include "../son/node.h"
#include "../core/bit_set.h"

#define LOOP_CACHE_SIZE 16

struct Evaluator {

    HMap<Node*, u64> cache_values = HMap<Node*, u64>::create();
    Vec<u64> loop_phi_cache = Vec<u64>::create();
    bool timeout = false;

    // static

    static long create_and_run(Node* start, long parameter, int loops) {
        Evaluator e {};
        u64 res = e.evaluate(start, parameter, loops);
        if(e.timeout) printe("Evaluator Runtime Error", "Timeout during evaluation");
        return res;
    }

    // given a ctrl node, return it's output ctrl
    static Node* find_control(Node* control) {
        assert(node::cfg(control));
        for(Node* output : control->output) {
            if(node::cfg(output)) return output;
        }
        return nullptr;
    }

    static Node* find_projection(Node* node, u32 i) {
        for(Node* output : node->output) {
            if(output->nt == NodeType::Proj && ((NodeProj*)output)->index == i) return output;
        }
        return nullptr;
    }

    // methods

    u64 get_value(Node* node) {
        if(cache_values.exists(node)) { return cache_values[node]; }
        switch(node->nt) {
            case NodeType::Const: {
                NodeConst* c_node = (NodeConst*) node;
                Type* c_type = c_node->val;
                assert(c_type->ttype == TypeT::Int);
                TypeInt* int_type = (TypeInt*) c_type;
                return int_type->val();
            }
            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Mul:
            case NodeType::Div:
            case NodeType::Mod: {
                NodeBinOp* binop_node = (NodeBinOp*) node;
                return op::apply(binop_node->op, Evaluator::get_value(binop_node->lhs()), Evaluator::get_value(binop_node->rhs()));
            }
            case NodeType::Neg: {
                NodeUnOp* unop_node = (NodeUnOp*) node;
                return op::apply(unop_node->op, Evaluator::get_value(unop_node->rhs()));
            }

            default: todo;
        }
    }

    void latch_loop_phis(Node* region, Node* prev) {
        u32 prev_i = region->input.index_of(prev);
        assert(prev_i > 0); // TODO why?
        u32 i = 0;
        for(Node* output : region->output) {
            if(output->nt == NodeType::Phi) {
                u64 value = this->get_value(output->input[prev_i]);
                loop_phi_cache.push(value); // TODO in the original, it uses lpc[i] = value; i++; why?
            }
        }
        i = 0; // TODO why not in a single loop?
        for(Node* output : region->output) {
            if(output->nt == NodeType::Phi) {
                cache_values.add(output, loop_phi_cache[i]);
                i++;
            }
        }
    }

    /**
     * Calculate the values of phis of the region and caches the values. The phis are not allowed to depend on other phis of the region.
     */
    void latch_phis(Node* region, Node* prev) {
        u32 prev_i = region->input.index_of(prev);
        assert(prev_i > 0); // TODO why?
        for(Node* output : region->output) {
            if(output->nt == NodeType::Phi) {
                u64 value = this->get_value(output->input[prev_i]);
                cache_values.add(output, value);
            }
        }
    }

    /**
     * Run the graph until either a return is found or the number of loop iterations are done.
     */
    u64 evaluate(Node* start, u64 parameter, u32 loops) {
        Node* parameter1 = this->find_projection(start, 1);
        if(parameter1 != nullptr) cache_values.add(parameter1, parameter);
        Node* control = this->find_projection(start, 0);
        Node* prev = start;
        while(control != nullptr) {
            Node* next;
            switch(control->nt) {
                case NodeType::Region: {
                    NodeRegion* region = (NodeRegion*) control;
                    if(!region->loop && region->ctrl(1) != prev) {
                        if(loops == 0) { timeout = true; return 0; }
                        loops--;
                        this->latch_loop_phis((Node*)region, prev);
                    } else {
                        this->latch_phis((Node*)region, prev);
                    }
                    next = this->find_control((Node*)region);
                    break;
                }
                case NodeType::If: {
                    NodeIf* ifnode = (NodeIf*) control;
                    next = Evaluator::find_projection(control, this->get_value(ifnode->condition()) != 0 ? 0 : 1);
                    break;
                }
                case NodeType::Ret: {
                    NodeRet* ret = (NodeRet*) control;
                    return this->get_value(ret->expr());
                }
                case NodeType::Proj: {
                    next = this->find_control(control);
                    break;
                }
                default: printe("Evaluator Runtime Error: Unexpected Node", control);
            }
            prev = control;
            control = next;
        }
        printe("Evaluator Runtime Error", "Fallthrough");
        return 0;
    }
};
