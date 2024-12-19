#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
  struct SkeletonPass : public PassInfoMixin<SkeletonPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      if (auto *MD = F.getMetadata("annotation")) {
        errs() << "[skeleton] Found annotation: " << F.getName() << "\n";
      } else {
        errs() << "[skeleton] No annotation: " << F.getName() << "\n";
      }

      return PreservedAnalyses::all();
    }
  };
} // namespace

// Register the pass with the new pass manager
llvm::PassPluginLibraryInfo getSkeletonPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "SkeletonPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "skeleton") {
                    FPM.addPass(SkeletonPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getSkeletonPassPluginInfo();
}
