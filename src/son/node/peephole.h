#pragma once

#include "node.h"
#include "compute.h"
#include "idealize.h"

namespace node {
    Node* peephole(Node* n) {
        assert(n != nullptr);
        assert(n->nt != NodeType::Undefined);
        n->type = node::compute(n); // compute n's best known type

        #ifdef NOOPTS
        return n;
        #endif

        Node* idealized = node::idealize(n);
        
        // no better representation
        if(idealized == nullptr) {
            return n;
        }

        // better representation found
        // Note that some peepholes modify inputs of a node, but leave the input node `n` valid and return it
        if(n != idealized && n->is_unused()) {
            // TODO do I need idealized.keep() and idealized.unkeep()??
            n->kill();
        }
        return idealized;
    }
}