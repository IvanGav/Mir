#pragma once

#include "../core/prelude.h"
#include "../core/str.h"
#include "../core/map.h"

static llvm::LLVMContext* llvm_context;
static llvm::IRBuilder<>* llvm_builder;
static llvm::Module* llvm_module;

void init_llvm() {
    // static HMap<Str, llvm::Value*> NamedValues;
}