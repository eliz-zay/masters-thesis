#include <map>

#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
  class AnnotationPass : public PassInfoMixin<AnnotationPass> {
  public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &FAM) {
      LLVMContext &context = M.getContext();

      auto *annotations = M.getNamedGlobal("llvm.global.annotations");
      if (!annotations || !annotations->hasInitializer()) {
        return PreservedAnalyses::all();
      }

      auto *initializer = dyn_cast<ConstantArray>(annotations->getInitializer());
      if (!initializer) {
        return PreservedAnalyses::all();
      }

      std::map<Function *, SmallVector<Metadata *>> valueAnnotationsMap;

      for (unsigned i = 0; i < initializer->getNumOperands(); ++i) {
        auto *operand = dyn_cast<ConstantStruct>(initializer->getOperand(i));
        if (!operand || operand->getNumOperands() < 2) {
          continue;
        }

        // Get the annotated function or variable
        auto *value = dyn_cast<Value>(operand->getOperand(0)->stripPointerCasts());
        if (!value) {
          continue;
        }

        // Get the annotation string
        auto *annotationMD = dyn_cast<ConstantDataArray>(
          cast<GlobalVariable>(operand->getOperand(1)->stripPointerCasts())
              ->getInitializer());
        if (!annotationMD) {
          continue;
        }

        if (auto *F = dyn_cast<Function>(value)) {
          // Remove last element of the parsed string with ASCII code 0
          std::string annotation = annotationMD->getAsString().str();
          annotation.pop_back();

          MDNode *mdNode = MDNode::get(context, MDString::get(context, annotation));
          valueAnnotationsMap[F].push_back(mdNode);

          errs() << "[annotation] Attached annotation: " << F->getName()
                 << " -> " << annotation << "\n";
        } else {
          errs() << "[annotation] No annotation: " << value->getName() << "\n";
        }
      }

      for (auto valueAnnotations : valueAnnotationsMap) {
        auto value = valueAnnotations.first;
        auto annotations = valueAnnotations.second;

        MDNode *annotationNode = MDNode::get(value->getContext(), annotations);
        value->setMetadata("annotation", annotationNode);
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
