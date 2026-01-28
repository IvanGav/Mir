#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

std::ostream& operator<<(std::ostream& os, TypeI ti) {
    switch (ti) {
        case TypeI::Top:    return os << "Top";
        case TypeI::Known:  return os << "Known";
        case TypeI::Bottom: return os << "Bottom";
    }
    unreachable;
}

std::ostream& operator<<(std::ostream& os, TypeT tt) {
    switch (tt) {
        case TypeT::Pure:   return os << "Pure";
        case TypeT::Ctrl:   return os << "Crtl";
        case TypeT::Bool:   return os << "Bool";
        case TypeT::Int:    return os << "Int";
        case TypeT::Float:  return os << "Float";
        case TypeT::Tuple:  return os << "Tuple";
    }
    unreachable;
}

std::ostream& operator<<(std::ostream& os, Type* t) {
    assert(t != nullptr);
    switch (t->ttype) {
        case TypeT::Pure: {
            os << "<Pure:" << t->tinfo << ">";
            return os;
        }
        case TypeT::Ctrl: {
            os << "<Ctrl:" << t->tinfo << ">";
            return os;
        }

        case TypeT::Bool: {
            TypeInt* ty = reinterpret_cast<TypeInt*>(t);
            if(ty->self.tinfo == TypeI::Top) 
                os << "<Bool:Top>";
            else if(ty->self.tinfo == TypeI::Bottom) 
                os << "<Bool:Bottom>";
            else if(ty->val_min == 0 && ty->val_max == 0)
                os << "false";
            else if(ty->val_min == 1 && ty->val_max == 1)
                os << "true";
            else if(ty->val_min == 0 && ty->val_max == 1)
                os << "<Bool:[false..true]>";
            else
                panic;
            return os;
        }
        case TypeT::Int: {
            TypeInt* ty = reinterpret_cast<TypeInt*>(t);
            if(ty->self.tinfo == TypeI::Top) 
                os << "<Int:Top>";
            else if(ty->self.tinfo == TypeI::Bottom) 
                os << "<Int:Bottom>";
            else if(ty->val_min == ty->val_max)
                os << "<Int:" << ty->val_min << ">";
            else
                os << "<Int:[" << ty->val_min << ".." << ty->val_max << "]>";
            return os;
        }

        case TypeT::Float: {
            TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
            if(ty->self.tinfo == TypeI::Top) 
                os << "<Float:Top>";
            else if(ty->self.tinfo == TypeI::Bottom) 
                os << "<Float:Bottom>";
            else if(ty->val_min == ty->val_max)
                os << ty->val_min;
            else
                os << "<Float:[" << ty->val_min << ".." << ty->val_max << "]>";
            return os;
        }

        case TypeT::Tuple: {
            TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
            os << "<Tuple:" << t->tinfo << "[";
            for(usize i = 0; i < ty->val.size; i++) {
                os << ty->val[i] << ",";
            }
            os << "]>";
            return os;
        }
    }
    unreachable;
}