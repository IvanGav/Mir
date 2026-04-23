#pragma once

#include "node.h"
#include "cfg.h"

namespace node {
    // get the data node's ctrl
    CFGNode* get_ctrl(Node* n) {
        assert(!n->cfg());
        // even though we don't cast to specific nodes,
        // we define any non-cfg node to have input[0] as its ctrl
        return n->input[0];
    }

    // get the cfg node's ith ctrl
    CFGNode* get_cfg_ctrl(CFGNode* n, u32 i = 0) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start: return n;
            case NodeType::Stop: {
                NodeStop* node = (NodeStop*) n;
                assert(i <= node->ctrl_size());
                return node->ctrl(i);
            }
            case NodeType::Ret: {
                NodeRet* node = (NodeRet*) n;
                assert(i == 0);
                return node->ctrl();
            }
            case NodeType::If: {
                NodeIf* node = (NodeIf*) n;
                assert(i == 0);
                return node->ctrl();
            }
            case NodeType::Loop:
            case NodeType::Region: {
                NodeRegion* node = (NodeRegion*) n;
                assert(i <= node->ctrl_size());
                return node->ctrl(i);
            }
            case NodeType::CtrlProj: {
                NodeProj* node = (NodeProj*) n;
                assert(i == 0);
                return node->ctrl();
            }
            default: unreachable;
        }
        unreachable;
    }

    // get the cfg node's number of (ctrl) inputs
    u32 ctrl_size(CFGNode* n) {
        assert(node::cfg(n));
        switch(n->nt) {
            case NodeType::Start:
                return 0;
            case NodeType::Ret:
            case NodeType::If:
            case NodeType::CtrlProj:
                return 1;
            case NodeType::Stop: {
                NodeStop* node = (NodeStop*) n;
                return node->ctrl_size();
            }
            case NodeType::Loop:
            case NodeType::Region: {
                NodeRegion* node = (NodeRegion*) n;
                return node->ctrl_size();
            }
            default: unreachable;
        }
        unreachable;
    }
}