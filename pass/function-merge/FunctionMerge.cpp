#include <vector>
#include <map>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "BaseAnnotatedPass.cpp"

using namespace llvm;

namespace {
  struct FunctionInfo {
    // Function *function;
    int caseIdx;
    int paramOffset;
  };

  struct MergedFunctions {
    Function *mergedFunction;
    std::map<Function *, FunctionInfo> targetFunctions;
  };

  class FunctionMergePass : public PassInfoMixin<FunctionMergePass> {
  private:
    static constexpr const char *annotationName = "function-merge";

    PreservedAnalyses applyPass(Function &F) const {
      LLVMContext &context = F.getContext();


      return PreservedAnalyses::none();
    }

    std::vector<Function *> getTargetFunctions(Module &M) const {
      std::vector<Function *> annotatedFunctions;

      for (Function &F : M) {
        auto *md = F.getMetadata("annotation");
        if (!md) {
          continue;
        }

        bool annotationFound = false;

        for (auto &mdOperand : md->operands()) {
          auto *mdNode = dyn_cast<MDNode>(mdOperand);
          if (!mdNode) {
            continue;
          }

          auto *mdString = dyn_cast<MDString>(mdNode->getOperand(0));
          if (!mdString) {
            continue;
          }

          if (mdString->getString() == this->annotationName) {
            annotationFound = true;
            break;
          }
        }

        if (!annotationFound) {
          continue;
        }

        if (
          F.isVarArg()
          || F.isDeclaration()
          || F.isIntrinsic()
          || F.getLinkage() != GlobalValue::LinkageTypes::InternalLinkage
          || !F.getReturnType()->isVoidTy()
        ) {
          continue;
        }

        errs() << "[" << this->annotationName << "] Applying to: " << F.getName() << "\n";
        annotatedFunctions.push_back(&F);
      }

      return annotatedFunctions;
    }

    MergedFunctions merge(std::vector<Function *> targetFunctions) const {

    }

  public:
    /*
      todo: process non-void functions
    */
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) const {
      auto targetFunctions = this->getTargetFunctions(M);

      if (targetFunctions.size() < 2) {
        return PreservedAnalyses::all();
      }

      /**
        void merged(int fidx, f1_1, f1_2, ..., fn_k) {
          switch (fidx) {
            case m: {
              doo_stuff_with(fm_1, ..., fm_k);
              break;
            }
            ...
          }
        }
       */
      // auto mergedFunctions = this->merge(targetFunctions);

      // this->updateFunctionReferences(mergedFunctions);

      return PreservedAnalyses::none();
    }
  };
} // namespace

PassPluginLibraryInfo getFunctionMergePassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "FunctionMergePass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](
          StringRef Name,
          ModulePassManager &MPM,
          ArrayRef<PassBuilder::PipelineElement>
        ) {
          if (Name == "function-merge") {
            MPM.addPass(FunctionMergePass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getFunctionMergePassPluginInfo();
}
