#pragma once

#include "../core/prelude.h"
#include "../core/str.h"
#include "../core/map.h"

static llvm::LLVMContext* llvm_context;
static llvm::Module* llvm_module;
static llvm::IRBuilder<>* llvm_builder;
static HMap<Str, llvm::Value*> named_values;

// for optimizations
static llvm::FunctionPassManager llvm_FPM;
static llvm::LoopAnalysisManager llvm_LAM;
static llvm::FunctionAnalysisManager llvm_FAM;
static llvm::CGSCCAnalysisManager llvm_CGAM;
static llvm::ModuleAnalysisManager llvm_MAM;
static llvm::PassInstrumentationCallbacks llvm_PIC;
static llvm::StandardInstrumentations* llvm_SI;

void init_llvm() {
    llvm_context = default_arena.alloc<llvm::LLVMContext>(1);
    new (llvm_context) llvm::LLVMContext();

    llvm_module = default_arena.alloc<llvm::Module>(1);
    new (llvm_module) llvm::Module("mir", *llvm_context);
    
    llvm_builder = default_arena.alloc<llvm::IRBuilder<>>(1);
    new (llvm_builder) llvm::IRBuilder<>(*llvm_context);

    // Create new pass and analysis managers.
    llvm_FPM = llvm::FunctionPassManager();
    llvm_LAM = llvm::LoopAnalysisManager();
    llvm_FAM = llvm::FunctionAnalysisManager();
    llvm_CGAM = llvm::CGSCCAnalysisManager();
    llvm_MAM = llvm::ModuleAnalysisManager();
    // llvm_PIC = llvm::PassInstrumentationCallbacks();
    llvm_SI = default_arena.alloc<llvm::StandardInstrumentations>(1);
    new (llvm_SI) llvm::StandardInstrumentations(*llvm_context, /*DebugLogging*/ true);
    llvm_SI->registerCallbacks(llvm_PIC, &llvm_MAM);

    // Add transform passes.
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    llvm_FPM.addPass(llvm::InstCombinePass());
    // Reassociate expressions.
    llvm_FPM.addPass(llvm::ReassociatePass());
    // Eliminate Common SubExpressions.
    llvm_FPM.addPass(llvm::GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    llvm_FPM.addPass(llvm::SimplifyCFGPass());

    // Register analysis passes used in these transform passes.
    llvm::PassBuilder PB; // TODO what? isn't it useless here wtf?
    PB.registerModuleAnalyses(llvm_MAM);
    PB.registerFunctionAnalyses(llvm_FAM);
    PB.crossRegisterProxies(llvm_LAM, llvm_FAM, llvm_CGAM, llvm_MAM);
}