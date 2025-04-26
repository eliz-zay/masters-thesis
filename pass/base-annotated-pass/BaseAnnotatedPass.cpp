#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;

template<class T>
class BaseAnnotatedPass : public PassInfoMixin<T> {
private:
  const std::string annotationName;

  virtual PreservedAnalyses applyPass(Function &F) const = 0;

public:
  BaseAnnotatedPass(const std::string &annotationName): annotationName(annotationName) {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    auto *md = F.getMetadata("annotation");
    if (!md) {
      return PreservedAnalyses::all();
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
      return PreservedAnalyses::all();
    }

    errs() << "[" << this->annotationName << "] Applying to: " << F.getName() << "\n";

    try {
      this->applyPass(F);
    } catch (const std::runtime_error& e) {
      errs() << "[" << this->annotationName << "] ERROR: " << e.what() << "\n";
      throw e;
    }

    return PreservedAnalyses::none();
  }
};
