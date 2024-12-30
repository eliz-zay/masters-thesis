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
  struct FlattenPass : public PassInfoMixin<FlattenPass> {
  private:
    const std::string annotationName = "flatten";

  public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      auto *MD = F.getMetadata("annotation");
      if (!MD) {
        return PreservedAnalyses::all();
      }

      auto *StringMD = dyn_cast<MDString>(MD->getOperand(0));
      if (!StringMD) {
        return PreservedAnalyses::all();
      }

      if (StringMD->getString() != this->annotationName) {
        return PreservedAnalyses::all();
      }

      errs() << "[flatten] Applying to: " << F.getName() << "\n";

      this->flattenCFG(F, FAM);

      return PreservedAnalyses::all();
    }

    PreservedAnalyses flattenCFG(Function &F, FunctionAnalysisManager &FAM) {
      

      return PreservedAnalyses::all();
    }
  };
} // namespace

// Register the pass with the new pass manager
llvm::PassPluginLibraryInfo getFlattenPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "FlattenPass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](
          StringRef Name,
          FunctionPassManager &FPM,
          ArrayRef<PassBuilder::PipelineElement>
        ) {
          if (Name == "flatten") {
            FPM.addPass(FlattenPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getFlattenPassPluginInfo();
}
