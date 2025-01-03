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

using namespace llvm;

namespace {
  struct FlattenPass : public PassInfoMixin<FlattenPass> {
  private:
    const std::string annotationName = "flatten";

    // Split basic block by the last two instructions and returns a new block (the second part)
    BasicBlock* splitBlockByConditionalBranch(BasicBlock *block) {
      // also take into account the `icmp` instruction that preceeds the `br` instruction
      auto lastInst = std::prev(block->end());
      if (lastInst != block->begin()) {
        --lastInst;
      }

      BasicBlock *split = block->splitBasicBlock(lastInst, "entryBlockSplit");
      return split;
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

      if (StringMD->getString() != this->annotationName) {
        return PreservedAnalyses::all();
      }

      errs() << "[flatten] Applying to: " << F.getName() << "\n";

      this->flattenCFG(F, FAM);

      return PreservedAnalyses::all();
    }

    /*
    ! пока плющим только блоки с branch
    todo: пропускать блоки с исключениями и indirectbr
    todo: обрабатывать switch

    условная или безусловная branch инструкция заменяется на:
    * next = *next-block*
    * br *initial-block*

    для выхода отдельный кейс

    * создать entry блок с переменной next и switch
    * пронумеровать существующие блоки
    * переделать terminating branch в формат с новыми блоками
     */
    PreservedAnalyses flattenCFG(Function &F, FunctionAnalysisManager &FAM) {
      LLVMContext &context = F.getContext();
      IRBuilder<> builder(context);

      std::vector<BasicBlock *> blocks;

      for (auto &block: F) {
        blocks.emplace_back(&block);
      }

      BasicBlock *entryBlock = blocks.front();
      entryBlock->setName("entryBlock");

      // Split conditional branch in two
      if (BranchInst *branch = dyn_cast<BranchInst>(entryBlock->getTerminator())) {
        if (branch->isConditional()) {
          BasicBlock *split = this->splitBlockByConditionalBranch(entryBlock);
          blocks.insert(std::next(blocks.begin()), split);
        }
      }

      // if (BranchInst *branch = dyn_cast<BranchInst>(entryBlock->getTerminator())) {
      //   // BasicBlock *successorBlock = branch->getSuccessor(0);
      //   BasicBlock *successorBlock = blocks[5];

      //   // Remove the old terminator (branch)
      //   branch->eraseFromParent();

      //   // Insert a new conditional or unconditional branch to the end of entryBlock
      //   builder.SetInsertPoint(entryBlock);
      //   builder.CreateBr(successorBlock);
      // }


      // CREATE SWITCH AND LOOP

      builder.SetInsertPoint(entryBlock->getFirstInsertionPt());
      AllocaInst *nextVar = new AllocaInst(Type::getInt32Ty(context), 0, "next", entryBlock->getFirstInsertionPt());
      builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), 0), nextVar);

      BasicBlock *loopStart = BasicBlock::Create(context, "loopStart", &F, entryBlock);
      BasicBlock *loopEnd = BasicBlock::Create(context, "loopEnd", &F, entryBlock);

      // make entryBlock point to loopStart instead of entryBlockSplit
      entryBlock->moveBefore(loopStart);
      entryBlock->getTerminator()->eraseFromParent();
      builder.SetInsertPoint(entryBlock);
      builder.CreateBr(loopStart);

      // create the switch in loopStart
      builder.SetInsertPoint(loopStart);
      LoadInst *loadNextVar = builder.CreateLoad(nextVar->getAllocatedType(), nextVar, "next");
      SwitchInst *switchStatement = builder.CreateSwitch(loadNextVar, nullptr);

      // create default block. `setDefaultDest` updates the `switch` terminator
      // and links loopStart with defaultSwitchBlock
      BasicBlock *defaultSwitchBlock = BasicBlock::Create(context, "defaultSwitchBlock", &F, loopEnd);
      switchStatement->setDefaultDest(defaultSwitchBlock);

      // make default block point to the loop end
      builder.SetInsertPoint(defaultSwitchBlock);
      builder.CreateBr(loopEnd);

      // connect the end to the start of the loop
      builder.SetInsertPoint(loopEnd);
      builder.CreateBr(loopStart);

      // At this point, all blocks except the entryBlock are not reachable







      // // we erase the first basic block since its already taken care of and won't
      // // be part of the switch.
      // blocks.erase(blocks.begin());

      // // for each of the original basic blocks put them in the switch
      // // we need to do this separately, so we can then reference the cases directly.
      // for (auto &BB: blocks) {
      //   switchStmt->addCase(ConstantInt::get(Type::getInt32Ty(context), switchStmt->getNumCases()), BB);
      //   BB->moveBefore(defaultSwitchBasicBlock);
      // }

      // // setup lookup table.
      // constexpr int32_t extraNumberCount = 3;

      // // Create an array of i32 with the desired size.
      // ArrayType *casesArrayType = ArrayType::get(
      //   Type::getInt32Ty(context),
      //   switchStmt->getNumCases() + extraNumberCount
      // );

      // builder.SetInsertPoint(&*entryBlock->getFirstInsertionPt());
      // AllocaInst *LookupTable = builder.CreateAlloca(
      //   casesArrayType,
      //   nullptr,
      //   "lookupTable"
      // );

      // // populate the lookup table
      // std::vector<int32_t> lookupTableValues;
      // for (int32_t idx = 0; idx < int32_t(switchStmt->getNumCases() + extraNumberCount); ++idx) {
      //   int32_t val = (idx + 1) - extraNumberCount;
      //   lookupTableValues.push_back(val);
      //   Value *EP = builder.CreateInBoundsGEP(
      //     casesArrayType,
      //     LookupTable,
      //     {ConstantInt::get(Type::getInt32Ty(context), 0), ConstantInt::get(Type::getInt32Ty(context), idx)}
      //   );
      //   builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), val), EP);
      // }

      // builder.CreateStore(
      //   builder.CreateLoad(
      //     Type::getInt32Ty(context),
      //     builder.CreateInBoundsGEP(
      //       casesArrayType,
      //       LookupTable,
      //       {
      //         ConstantInt::get(Type::getInt32Ty(context), 0),
      //         ConstantInt::get(Type::getInt32Ty(context), 0)
      //       }
      //     )),
      //   loadDispatcherVal->getPointerOperand()
      // );

      return PreservedAnalyses::all();





      for (BasicBlock &block : F) {
        errs() << "\nblock\n";

        // If the basic block has a conditional branch, we need to flatten it
        BranchInst *branch = dyn_cast<BranchInst>(block.getTerminator());
        if (!branch) {
          return PreservedAnalyses::all();
        }

        if (branch->isConditional()) {
          this->flattenBranch(branch);
        }

      }

      return PreservedAnalyses::all();
    }

    void flattenBranch(BranchInst *branch) {
      errs() << "flattening branch\n";
    }
  };
} // namespace

// Register the pass with the new pass manager
PassPluginLibraryInfo getFlattenPassPluginInfo() {
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

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getFlattenPassPluginInfo();
}
