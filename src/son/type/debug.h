#pragma once

#include "../../core/prelude.h"
#include "../../core/mem.h"
#include "../../core/slice.h"
#include "../../core/set.h"
#include "../../core/str.h"

#include "type_def.h"
#include "type_pool.h"

namespace type {
    Str to_str(TypeI ti) {
        switch (ti) {
            case TypeI::Top:    return "Top"_s;
            case TypeI::Known:  return "Known"_s;
            case TypeI::Bottom: return "Bottom"_s;
        }
        unreachable;
    }

    Str to_str(TypeT tt) {
        switch (tt) {
            case TypeT::Pure:   return "Pure"_s;
            case TypeT::Ctrl:   return "Crtl"_s;
            case TypeT::Bool:   return "Bool"_s;
            case TypeT::Int:    return "Int"_s;
            case TypeT::Float:  return "Float"_s;
            case TypeT::Tuple:  return "Tuple"_s;
            case TypeT::Ptr:    return "Ptr"_s;
            case TypeT::Mem:    return "Mem"_s;
        }
        unreachable;
    }
    Str to_str(Type* t) {
        assert(t != nullptr);
        switch (t->ttype) {
            case TypeT::Pure: {
                return type::to_str(t->tinfo);
            }
            
            case TypeT::Ctrl: {
                if(t->tinfo == TypeI::Bottom)
                    return "Ctrl"_s;
                return "XCtrl"_s;
            }

            case TypeT::Bool: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    return "Bool:Top"_s;
                else if(ty->self.tinfo == TypeI::Bottom) 
                    return "Bool:Bottom"_s;
                else if(ty->val_min == 0 && ty->val_max == 0)
                    return "false"_s;
                else if(ty->val_min == 1 && ty->val_max == 1)
                    return "true"_s;
                else if(ty->val_min == 0 && ty->val_max == 1)
                    return "Bool:any"_s;
                else
                    panic;
            }
            case TypeT::Int: {
                TypeInt* ty = reinterpret_cast<TypeInt*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    return "Int:Top"_s;
                else if(ty->self.tinfo == TypeI::Bottom) 
                    return "Int:Bottom"_s;
                else if(ty->val_min == ty->val_max)
                    return str::from_slice_of_str(ref(Vec<Str>::with("Int:"_s, str::from_int(ty->val_min)).full_slice()));
                else
                    return str::from_slice_of_str(ref(Vec<Str>::with("Int:"_s, str::from_int(ty->val_min), "..="_s, str::from_int(ty->val_max)).full_slice()));
            }

            case TypeT::Float: {
                TypeFloat* ty = reinterpret_cast<TypeFloat*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    return "Float:Top"_s;
                else if(ty->self.tinfo == TypeI::Bottom) 
                    return "Float:Bottom"_s;
                else if(ty->val_min == ty->val_max)
                    return str::from_slice_of_str(ref(Vec<Str>::with("Float:"_s, "UNIMPLEMENTED"_s).full_slice()));
                else
                    return str::from_slice_of_str(ref(Vec<Str>::with("Float:"_s, "UNIMPLEMENTED"_s, "..="_s, "UNIMPLEMENTED"_s).full_slice()));
            }

            case TypeT::Tuple: {
                // TypeTuple* ty = reinterpret_cast<TypeTuple*>(t);
                return "Tuple TODO"_s;
            }

            case TypeT::Mem:
            case TypeT::Ptr: {
                TypePtr* ty = reinterpret_cast<TypePtr*>(t);
                if(ty->self.tinfo == TypeI::Top) 
                    return "null"_s;
                else if(ty->self.tinfo == TypeI::Bottom) 
                    return "void*"_s;
                else
                    return str::from_slice_of_str(ref(Vec<Str>::with(to_str(t->ttype), ":"_s, to_str(ty->ptr)).full_slice()));
            }
        }
        unreachable;
    }
};

std::ostream& operator<<(std::ostream& os, TypeI ti) {
    return os << type::to_str(ti);
}
std::ostream& operator<<(std::ostream& os, TypeT tt) {
    return os << type::to_str(tt);
}
std::ostream& operator<<(std::ostream& os, Type* t) {
    return os << type::to_str(t);
}

/*
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
        case TypeT::Ptr:    return os << "Ptr";
        case TypeT::Mem:    return os << "Mem";
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

        case TypeT::Mem:
        case TypeT::Ptr: {
            TypePtr* ty = reinterpret_cast<TypePtr*>(t);
            os << "<" << type::to_str(t->tinfo) << ":" << t->tinfo << ty->ptr << ">";
            return os;
        }
    }
    unreachable;
}
*/