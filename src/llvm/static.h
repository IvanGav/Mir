#pragma once

#include "../core/prelude.h"
#include "../core/str.h"
#include "../core/map.h"

static llvm::LLVMContext* llvm_context;
static llvm::Module* llvm_module;
static llvm::IRBuilder<>* llvm_builder;
static HMap<Str, llvm::Value*> named_values;

void init_llvm() {
    llvm_context = default_arena.alloc<llvm::LLVMContext>(1);
    new (llvm_context) llvm::LLVMContext();

    llvm_module = default_arena.alloc<llvm::Module>(1);
    new (llvm_module) llvm::Module("mir", *llvm_context);
    
    llvm_builder = default_arena.alloc<llvm::IRBuilder<>>(1);
    new (llvm_builder) llvm::IRBuilder<>(*llvm_context);
}