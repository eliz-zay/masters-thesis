#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
  struct SkeletonPass : public PassInfoMixin<SkeletonPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &FAM) {
      // Get annotations of global functions and variables
      auto *Annotations = M.getNamedGlobal("llvm.global.annotations");
      if (!Annotations || !Annotations->hasInitializer()) {
        return PreservedAnalyses::all();
      }

      auto *Array = dyn_cast<ConstantArray>(Annotations->getInitializer());
      if (!Array) {
        return PreservedAnalyses::all();
      }

      for (unsigned i = 0; i < Array->getNumOperands(); ++i) {
        errs() << "Processing attribute" << "\n";

        auto *Struct = dyn_cast<ConstantStruct>(Array->getOperand(i));
        if (!Struct || Struct->getNumOperands() < 2) {
          continue;
        }

        // Get the annotated function or variable
        auto *AnnotatedValue = dyn_cast<Function>(Struct->getOperand(0)->stripPointerCasts());
        if (!AnnotatedValue) {
          continue; // Skip non-function annotations
        }

        // Get the annotation string
        auto *AnnotationMD = dyn_cast<ConstantDataArray>(
          cast<GlobalVariable>(Struct->getOperand(1)->stripPointerCasts())
              ->getInitializer());
        if (!AnnotationMD) {
          continue;
        }

        StringRef Annotation = AnnotationMD->getAsString();

        errs()
          << "Function: " << AnnotatedValue->getName()
          << ", annotation: " << Annotation.data()
          << "\n";
      }

      return PreservedAnalyses::all();
    }
  };
} // namespace

// Register the pass with the new pass manager
llvm::PassPluginLibraryInfo getSkeletonPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "SkeletonPass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
         [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "skeleton") {
            MPM.addPass(SkeletonPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getSkeletonPassPluginInfo();
}
