#pragma once

#include "../son/node.h"

namespace compile {

    void dot_declare_node(Node* n, HSet<Node*>& visited, Vec<u8>& output) {
        if(visited.has(n)) return;
        visited.add(n);
        for(u32 i = 0; i < n->output.size; i++) {
            compile::dot_declare_node(n->output[i], visited, output);
        }
        switch(n->nt) {
            case NodeType::Scope: {
                Slice<Str> s = Vec<Str>::with(str::from_int(n->uid), " [label=\"scope_node\"];"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Start: {
                NodeStart* node = (NodeStart*) n;
                Vec<Str> s = Vec<Str>::with(str::from_int(n->uid), " [label=\""_s);
                for(u32 i = 0; i < node->args->val.size; i++) {
                    s.push(ref(type::to_str(node->args->val[i])));
                    s.push(", "_s);
                }
                s.push(ref("\"];\n"_s));
                output.push_slice(str::from_slice_of_str(ref(s.full_slice())));
                break;
            }

            case NodeType::Stop: {
                Str uid = str::from_int(n->uid);
                Slice<Str> s = Vec<Str>::with(uid, " [label=\"stop\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Ret: {
                Str uid = str::from_int(n->uid);
                Slice<Str> s = Vec<Str>::with(uid, " [label=\"return\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }
            
            case NodeType::Const: {
                NodeConst* node = (NodeConst*) n;
                Slice<Str> s = Vec<Str>::with(str::from_int(n->uid), " [label=\""_s, type::to_str(node->val), "\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }
            
            case NodeType::CtrlProj:
            case NodeType::Proj: {
                NodeProj* node = (NodeProj*) n;
                Str uid = str::from_int(n->uid);
                Slice<Str> s = Vec<Str>::with(uid, " [label=\"["_s, str::from_int(node->index), "]\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::If: {
                Str uid = str::from_int(n->uid);
                Slice<Str> s = Vec<Str>::with(uid, " [label=\"if\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Region: {
                NodeRegion* node = (NodeRegion*) n;
                Str uid = str::from_int(n->uid);
                Vec<Str> s = Vec<Str>::with(uid, " [label=\""_s, node->loop ? "loop"_s : "region"_s, "\"];\n"_s);
                output.push_slice(str::from_slice_of_str(ref(s.full_slice())));
                break;
            }

            case NodeType::Phi: {
                NodePhi* node = (NodePhi*) n;
                Str uid = str::from_int(n->uid);
                Vec<Str> s = Vec<Str>::with(uid, " [label=\"phi_"_s, node->debug_var_name, "\"];\n"_s);
                output.push_slice(str::from_slice_of_str(ref(s.full_slice())));
                break;
            }
            
            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Mul:
            case NodeType::Div:
            case NodeType::Mod:
            case NodeType::Eq:
            case NodeType::Neq:
            case NodeType::Less:
            case NodeType::Greater:
            case NodeType::LessEq:
            case NodeType::GreaterEq: {
                NodeBinOp* node = (NodeBinOp*) n;
                Str uid = str::from_int(n->uid);
                Slice<Str> s = Vec<Str>::with(uid, " [label=\""_s, op::symbol(node->op), "\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Neg: {
                NodeUnOp* node = (NodeUnOp*) n;
                Str uid = str::from_int(n->uid);
                Slice<Str> s = Vec<Str>::with(uid, " [label=\""_s, op::symbol(node->op), "\"];\n"_s).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Load: {
                Str uid = str::from_int(n->uid);
                output.push_slice(str::cat(uid, " [label=\""_s, "load"_s, "\"];\n"_s));
                break;
            }
            
            case NodeType::Store: {
                Str uid = str::from_int(n->uid);
                output.push_slice(str::cat(uid, " [label=\""_s, "store"_s, "\"];\n"_s));
                break;
            }

            case NodeType::AllocA: {
                Str uid = str::from_int(n->uid);
                output.push_slice(str::cat(uid, " [label=\""_s, "alloca"_s, "\"];\n"_s));
                break;
            }
            
            case NodeType::Undefined:
                printe("call compile to dot on undefined node", n);
                panic;
        }
        return;
    }

    void dot_add_edges(Node* n, HSet<Node*>& visited, Vec<u8>& output) {
        if(visited.has(n)) return;
        visited.add(n);
        for(u32 i = 0; i < n->output.size; i++) {
            compile::dot_add_edges(n->output[i], visited, output);
        }
        switch(n->nt) {
            case NodeType::Scope: {
                break;
            }

            case NodeType::Start: {
                break;
            }

            case NodeType::Stop: {
                NodeStop* node = (NodeStop*) n;
                Str uid = str::from_int(n->uid);
                Vec<Str> s = Vec<Str>::with();
                for(u32 i = 0; i < node->ctrl_size(); i++) {
                    if(node->ctrl(i) == nullptr) { panic; }
                    s.push(str::from_int(node->ctrl(i)->uid));
                    s.push(" -> "_s);
                    s.push(uid);
                    s.push(" [style=dotted];\n"_s);
                }
                output.push_slice(str::from_slice_of_str(ref(s.full_slice())));
                break;
            }

            case NodeType::Ret: {
                NodeRet* node = (NodeRet*) n;
                Str uid = str::from_int(n->uid);
                Str ctrl_uid = str::from_int(node->ctrl()->uid);
                Str expr_uid = str::from_int(node->expr()->uid);
                Slice<Str> s = Vec<Str>::with(
                    ctrl_uid, " -> "_s, uid, " [style=dotted];\n"_s,
                    expr_uid, " -> "_s, uid, ";\n"_s
                ).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }
            
            case NodeType::Const: {
                break;
            }
            
            case NodeType::CtrlProj: {
                NodeProj* node = (NodeProj*) n;
                Str uid = str::from_int(n->uid);
                Str src_uid = str::from_int(node->ctrl()->uid);
                Slice<Str> s = Vec<Str>::with(
                    src_uid, " -> "_s, uid, " [style=dotted];\n"_s
                ).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Proj: {
                NodeProj* node = (NodeProj*) n;
                Str uid = str::from_int(n->uid);
                Str src_uid = str::from_int(node->ctrl()->uid);
                Slice<Str> s = Vec<Str>::with(
                    src_uid, " -> "_s, uid, ";\n"_s
                ).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::If: {
                NodeIf* node = (NodeIf*) n;
                Str uid = str::from_int(n->uid);
                Str ctrl_uid = str::from_int(node->ctrl()->uid);
                Str condition_uid = str::from_int(node->condition()->uid);
                Slice<Str> s = Vec<Str>::with(
                    ctrl_uid, " -> "_s, uid, " [style=dotted];\n"_s,
                    condition_uid, " -> "_s, uid, ";\n"_s
                ).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Region: {
                NodeRegion* node = (NodeRegion*) n;
                Str uid = str::from_int(n->uid);
                Vec<Str> s = Vec<Str>::with();
                for(u32 i = 0; i < node->ctrl_size(); i++) {
                    if(node->ctrl(i) == nullptr) { continue; }
                    s.push(ref(str::from_int(node->ctrl(i)->uid)));
                    s.push(ref(" -> "_s));
                    s.push(ref(uid));
                    s.push(ref(" [style=dotted];\n"_s));
                }
                output.push_slice(str::from_slice_of_str(ref(s.full_slice())));
                break;
            }

            case NodeType::Phi: {
                NodePhi* node = (NodePhi*) n;
                Str uid = str::from_int(n->uid);
                Str ctrl_uid = str::from_int(node->region()->uid);
                Vec<Str> s = Vec<Str>::with(
                    ctrl_uid, " -> "_s, uid, " [style=dotted];\n"_s
                );
                for(u32 i = 0; i < node->data_size(); i++) {
                    if(node->data(i) == nullptr) { continue; }
                    s.push(ref(str::from_int(node->data(i)->uid)));
                    s.push(ref(" -> "_s));
                    s.push(ref(uid));
                    s.push(ref(";\n"_s));
                }
                output.push_slice(str::from_slice_of_str(ref(s.full_slice())));
                break;
            }
            
            case NodeType::Add:
            case NodeType::Sub:
            case NodeType::Mul:
            case NodeType::Div:
            case NodeType::Mod:
            case NodeType::Eq:
            case NodeType::Neq:
            case NodeType::Less:
            case NodeType::Greater:
            case NodeType::LessEq:
            case NodeType::GreaterEq: {
                NodeBinOp* node = (NodeBinOp*) n;
                Str uid = str::from_int(n->uid);
                Str lhs_uid = str::from_int(node->lhs()->uid);
                Str rhs_uid = str::from_int(node->rhs()->uid);
                Slice<Str> s = Vec<Str>::with(
                    lhs_uid, " -> "_s, uid, " [label=\"lhs\"];\n"_s,
                    rhs_uid, " -> "_s, uid, " [label=\"rhs\"];\n"_s
                ).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Neg: {
                NodeUnOp* node = (NodeUnOp*) n;
                Str uid = str::from_int(n->uid);
                Str rhs_uid = str::from_int(node->rhs()->uid);
                Slice<Str> s = Vec<Str>::with(
                    rhs_uid, " -> "_s, uid, ";\n"_s
                ).full_slice();
                output.push_slice(str::from_slice_of_str(s));
                break;
            }

            case NodeType::Load: {
                NodeLoad* node = (NodeLoad*) n;
                Str uid = str::from_int(n->uid);
                output.push_slice(str::cat(
                    str::from_int(node->mem()->uid), " -> "_s, uid, " [label=\"mem\"];\n"_s,
                    str::from_int(node->ptr()->uid), " -> "_s, uid, " [label=\"ptr\"];\n"_s,
                    str::from_int(node->off()->uid), " -> "_s, uid, " [label=\"off\"];\n"_s
                ));
                break;
            }
            
            case NodeType::Store: {
                NodeStore* node = (NodeStore*) n;
                Str uid = str::from_int(n->uid);
                output.push_slice(str::cat(
                    str::from_int(node->mem()->uid), " -> "_s, uid, " [label=\"mem\"];\n"_s,
                    str::from_int(node->ptr()->uid), " -> "_s, uid, " [label=\"ptr\"];\n"_s,
                    str::from_int(node->off()->uid), " -> "_s, uid, " [label=\"off\"];\n"_s,
                    str::from_int(node->val()->uid), " -> "_s, uid, " [label=\"val\"];\n"_s
                ));
                break;
            }

            case NodeType::AllocA: {
                NodeAllocA* node = (NodeAllocA*) n;
                Str uid = str::from_int(n->uid);
                output.push_slice(str::cat(
                    str::from_int(node->ctrl()->uid), " -> "_s, uid, " [label=\"ptr\"];\n"_s,
                    str::from_int(node->mem()->uid), " -> "_s, uid, " [label=\"mem\"];\n"_s,
                    str::from_int(node->size()->uid), " -> "_s, uid, " [label=\"size\"];\n"_s
                ));
                break;
            }
            
            case NodeType::Undefined:
                printe("call compile to dot on undefined node", n);
                panic;
        }
        return;
    }
    
    Str dot(Node* start) {
        Vec<u8> output = Vec<u8>::create();
        output.push_slice("digraph son_graph {\n"_s);
        {
            HSet<Node*> visited = HSet<Node*>::create(default_arena);
            compile::dot_declare_node(start, visited, output);
        }
        {
            HSet<Node*> visited = HSet<Node*>::create(default_arena);
            compile::dot_add_edges(start, visited, output);
        }
        output.push_slice("}\n"_s);
        return output.full_slice();
    }
}