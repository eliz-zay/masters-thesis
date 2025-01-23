#include <vector>
#include <map>

#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace {
  struct BogusSwitchPass : public PassInfoMixin<BogusSwitchPass> {
  private:
    const std::string annotationName = "bogus-switch";

    // A fraction of switch case blocks to duplicate (e.g. 0.7 means that 70% of switch blocks
    // will be duplicated and added as new cases)
    const double switchCaseTargetPart = 0.7;

    // A fraction of `store i32 caseValue, ptr %caseVar` instructions with case values of original blocks
    // to be replaced by similar instructions with case values of duplicated blocks.
    // A value is close to zero may lead to unreachable duplicated blocks (dead code)
    const double storeInstRemappingPart = 0.5;

    // Changes some part of `store i32 targetCaseValue, ptr %caseVar` instructions
    // to `store i32 duplicateCaseValue, ptr %caseVar` instruction.
    // Here, `caseVar` is a variable used in switch condition. `targetCaseValue` and `duplicateCaseValue` refer to
    // the original and duplicated switch case blocks respectively
    void remapCaseVarStoreInstructions(Function &F, Value *caseVar, ConstantInt *targetCaseValue, ConstantInt *duplicateCaseValue) {
      LLVMContext &context = F.getContext();
      IRBuilder<> builder(context);

      std::vector<StoreInst *> storeInstructions = {};

      for (auto &block : F) {
        for (auto &instruction : block) {
          auto storeInst = dyn_cast<StoreInst>(&instruction);
          if (
            storeInst
            && storeInst->getPointerOperand() == caseVar
            && storeInst->getValueOperand() == targetCaseValue
          ) {
            storeInstructions.push_back(storeInst);
          }
        }
      }

      const int countToRemap = floor(storeInstructions.size() * this->storeInstRemappingPart);

      errs() << "[bogus-switch] Generating duplicate case #" << duplicateCaseValue->getValue()
             << " for case #" << targetCaseValue->getValue()
             << " and replacing " << countToRemap << " references\n";

      for (int i = 0; i < countToRemap; i++) {
        auto storeInst = storeInstructions[i];
        storeInst->setOperand(0, duplicateCaseValue);
      }
    }

    // Generates a unique case value for a switch, plausible if possible
    ConstantInt *generateCaseValue(LLVMContext &context, SwitchInst *switchInst) const {
      ConstantInt *caseValue = ConstantInt::get(Type::getInt32Ty(context), switchInst->getNumCases());

      // Check if there exists a case with plausible value (total number of cases)
      if (switchInst->findCaseValue(caseValue) == switchInst->case_default()) {
        // Randomize until a unique value is found
        while (switchInst->findCaseValue(caseValue) != switchInst->case_default()) {
          caseValue = ConstantInt::get(Type::getInt32Ty(context), rand());
        }
      }

      return caseValue;
    }

    // Returns switch case variable (condition) for the specific block.
    // It is usually located in `load` instruction before the switch
    Value* getSwitchCaseVar(BasicBlock &block, SwitchInst *switchInst) const {
      Instruction *instBeforeSwitch = &*(std::prev(std::prev(block.end())));
      LoadInst *loadInstBeforeSwitch = dyn_cast<LoadInst>(instBeforeSwitch);

      if (loadInstBeforeSwitch == switchInst->getCondition()) {
        return loadInstBeforeSwitch->getPointerOperand();
      }

      return nullptr;
    }

    PreservedAnalyses duplicateSwitchBlocks(Function &F) {
      LLVMContext &context = F.getContext();

      for (auto &block : F) {
        auto switchInst = dyn_cast<SwitchInst>(block.getTerminator());
        if (!switchInst) {
          continue;
        }

        Value *caseVar = this->getSwitchCaseVar(block, switchInst);
        if (caseVar == nullptr) {
          continue;
        }

        unsigned targetCount = ceil(switchInst->getNumCases() * this->switchCaseTargetPart);

        for (
          auto switchCase = switchInst->case_begin();
          targetCount > 0;
          targetCount--, switchCase++
        ) {
          ConstantInt *targetCaseValue = switchCase->getCaseValue();
          BasicBlock *targetBlock = switchCase->getCaseSuccessor();

          ValueToValueMapTy VMap;
          BasicBlock *duplicateBlock = CloneBasicBlock(targetBlock, VMap, ".duplicate", &F);

          for (Instruction &instruction : *duplicateBlock) {
            RemapInstruction(&instruction, VMap, RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
          }

          // Add duplicated block as a switch case
          ConstantInt *duplicateCaseValue = this->generateCaseValue(context, switchInst);
          switchInst->addCase(duplicateCaseValue, duplicateBlock);

          // Make duplicated block reachable
          this->remapCaseVarStoreInstructions(F, caseVar, targetCaseValue, duplicateCaseValue);
        }
      }

      return PreservedAnalyses::none();
    }

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

      // todo
      // if (StringMD->getString() != this->annotationName) {
      //   return PreservedAnalyses::all();
      // }

      errs() << "[bogus-switch] Applying to: " << F.getName() << "\n";

      try {
        this->duplicateSwitchBlocks(F);
      } catch (const std::runtime_error& e) {
        errs() << "[bogus-switch] ERROR: " << e.what() << "\n";
      }

      return PreservedAnalyses::none();
    }
  };
} // namespace

PassPluginLibraryInfo getBogusSwitchPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "BogusSwitchPass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](
          StringRef Name,
          FunctionPassManager &FPM,
          ArrayRef<PassBuilder::PipelineElement>
        ) {
          if (Name == "bogus-switch") {
            FPM.addPass(BogusSwitchPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getBogusSwitchPassPluginInfo();
}
