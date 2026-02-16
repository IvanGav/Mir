#pragma once

#include "../../core/prelude.h"
#include "../../core/map.h"

#include "node_def.h"

template <typename T>
struct VariableScope {
    Vec<HMap<Str,T>> scopes;

    static VariableScope<T> create(mem::Arena& arena) {
        VariableScope<T> self { .scopes = Vec<HMap<Str,T>>::create(arena) };
        self.push();
        return self;
    }

    // Push a new scope to be innermost
    void push() {
        scopes.push(ref(HMap<Str,T>::empty(scopes.arena)));
    }

    // Pop the innermost scope
    void pop() {
        scopes.pop();
    }

    T operator[](Str key) const {
        for(u32 i = scopes.size-1; i >= 0; i--) {
            if(scopes[i].exists(key)) {
                return scopes[i][key];
            }
        }
        return nullptr;
    }

    T& operator[](Str key) {
        assert(scopes.size > 0);
        for(u32 i = scopes.size-1; i >= 0; i--) {
            if(scopes[i].exists(key)) {
                return scopes[i][key];
            }
        }
        panic;
    }

    // Define a variable in the innermost scope
    void define(Str var_name, T& value) {
        scopes.back().add(var_name, value);
    }

    bool contains(Str key) {
        for(u32 i = scopes.size-1; i >= 0; i--) {
            if(scopes[i].exists(key)) {
                return true;
            }
        }
        return false;
    }

    HMap<Str,T>& top() {
        return scopes.back();
    }

    usize top_size() {
        return scopes.back().size;
    }

    VariableScope<T> deep_clone() {
        VariableScope<T> c { .scopes = Vec<HMap<Str,T>>::create(*scopes.arena) };
        c.scopes = this->scopes.clone(scopes.arena);
        for(u32 i = 0; i < scopes.size; i++) {
            c.scopes[i] = scopes[i].clone(scopes.arena);
        }
        return c;
    }
};