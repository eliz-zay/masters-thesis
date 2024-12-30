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
  struct AnnotationPass : public PassInfoMixin<AnnotationPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &FAM) {
      LLVMContext &Ctx = M.getContext();

      auto *Annotations = M.getNamedGlobal("llvm.global.annotations");
      if (!Annotations || !Annotations->hasInitializer()) {
        return PreservedAnalyses::all();
      }

      auto *Array = dyn_cast<ConstantArray>(Annotations->getInitializer());
      if (!Array) {
        return PreservedAnalyses::all();
      }

      for (unsigned i = 0; i < Array->getNumOperands(); ++i) {
        auto *Struct = dyn_cast<ConstantStruct>(Array->getOperand(i));
        if (!Struct || Struct->getNumOperands() < 2) {
          continue;
        }

        // Get the annotated function or variable
        auto *AnnotatedValue = dyn_cast<Value>(Struct->getOperand(0)->stripPointerCasts());
        if (!AnnotatedValue) {
          continue;
        }

        // Get the annotation string
        auto *AnnotationMD = dyn_cast<ConstantDataArray>(
          cast<GlobalVariable>(Struct->getOperand(1)->stripPointerCasts())
              ->getInitializer());
        if (!AnnotationMD) {
          continue;
        }

        StringRef Annotation = AnnotationMD->getAsString();

        if (auto *F = dyn_cast<Function>(AnnotatedValue)) {
          // Remove last element of the parsed string with ASCII code 0
          std::string annotation = Annotation.str();
          annotation.pop_back();

          MDNode *AnnotationNode = MDNode::get(Ctx, MDString::get(Ctx, annotation));
          F->setMetadata("annotation", AnnotationNode);

          errs() << "[annotation] Attached annotation: " << F->getName()
                 << " -> " << annotation << "\n";
        } else {
          errs() << "[annotation] No annotation: " << AnnotatedValue->getName() << "\n";
        }
      }

      return PreservedAnalyses::none();
    }
  };
} // namespace

// Register the pass with the new pass manager
llvm::PassPluginLibraryInfo getAnnotationPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "AnnotationPass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
         [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "annotation") {
            MPM.addPass(AnnotationPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getAnnotationPassPluginInfo();
}
