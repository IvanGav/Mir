#pragma once

#include "../../core/prelude.h"
#include "../../core/map.h"

#include "node_def.h"

template <typename T>
struct VariableScope {
    struct Scope {
        HMap<Str, T> names;
        Scope* next;
    };
    
    mem::Arena* arena;
    Scope* top;

    static VariableScope create(mem::Arena& arena) {
        VariableScope self { .arena = &arena, .top = nullptr };
        self.push();
        return self;
    }

    // Push a new scope to be innermost
    void push() {
        Scope* old_top = top;
        top = arena->alloc<Scope>(1);
        top->names.arena = arena;
        top->next = old_top;
    }

    // Pop the innermost scope
    void pop() {
        assert(top != nullptr);
        top = top->next;
    }

    T operator[](Str key) const {
        assert(top != nullptr);
        Scope* cur = top;
        while(cur != nullptr) {
            if(cur->names.exists(key)) {
                return cur->names[key];
            }
            cur = cur->next;
        }
        return nullptr;
    }

    T& operator[](Str key) {
        assert(top != nullptr);
        Scope* cur = top;
        while(cur != nullptr) {
            if(cur->names.exists(key)) {
                return cur->names[key];
            }
            cur = cur->next;
        }
        panic;
        // top->names.add(key, nullptr);
        // return top->names[key];
    }

    // Define a variable in the innermost scope
    void define(Str var_name, T& value) {
        assert(top != nullptr);
        top->names.add(var_name, value);
    }

    bool contains(Str key) {
        assert(top != nullptr);
        Scope* cur = top;
        while(cur != nullptr) {
            if(cur->names.exists(key)) {
                return true;
            }
            cur = cur->next;
        }
        return false;
    }

    usize top_size() {
        assert(top != nullptr);
        return top->names.size;
    }
};