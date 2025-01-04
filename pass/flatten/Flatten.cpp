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

using namespace llvm;

namespace {
  struct FlattenPass : public PassInfoMixin<FlattenPass> {
  private:
    const std::string annotationName = "flatten";

    // Split basic block by the last two instructions and returns a new block (the second part)
    BasicBlock* splitBlockByConditionalBranch(BasicBlock &block) {
      // also take into account the `icmp` instruction that preceeds the `br` instruction
      auto lastInst = std::prev(block.end());
      if (lastInst != block.begin()) {
        --lastInst;
      }

      BasicBlock *split = block.splitBasicBlock(lastInst, "entryBlockSplit");
      return split;
    }

    std::vector<PHINode *> getPHINodes(Function &F) const {
      std::vector<PHINode *> nodes;

      for (auto &BB: F) {
        for (auto &inst: BB) {
          if (isa<PHINode>(&inst)) {
            nodes.push_back(cast<PHINode>(&inst));
          }
        }
      }

      return nodes;
    }

    std::vector<Instruction *> getInstructionReferencedInMultipleBlocks(Function &F) const {
      BasicBlock *EntryBasicBlock = &*F.begin();
      std::vector<Instruction *> usedOutside;

      for (auto &block: F) {
        for (auto &inst: block) {
          // in the entry block there will be a bunch of stack allocation using alloca
          // that are referenced in multiple blocks thus we need to ignore those when
          // filtering.
          if (isa<AllocaInst>(&inst) && inst.getParent() == EntryBasicBlock) {
            continue;
          }

          // check if used outside the current block.
          if (inst.isUsedOutsideOfBlock(&BB)) {
            usedOutside.push_back(&inst);
          }
        }
      }

      return usedOutside;
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

      BasicBlock &entryBlock = F.front();
      entryBlock.setName("entryBlock");

      // Split conditional branch in two
      if (BranchInst *branch = dyn_cast<BranchInst>(entryBlock.getTerminator())) {
        if (branch->isConditional()) {
          this->splitBlockByConditionalBranch(entryBlock);
        }
      }

      // if (BranchInst *branch = dyn_cast<BranchInst>(entryBlock.getTerminator())) {
      //   // BasicBlock *successorBlock = branch->getSuccessor(0);
      //   BasicBlock *successorBlock = blocks[5];

      //   // Remove the old terminator (branch)
      //   branch->eraseFromParent();

      //   // Insert a new conditional or unconditional branch to the end of entryBlock
      //   builder.SetInsertPoint(entryBlock);
      //   builder.CreateBr(successorBlock);
      // }


      // CREATE SWITCH AND LOOP

      builder.SetInsertPoint(entryBlock.getFirstInsertionPt());
      AllocaInst *nextVar = new AllocaInst(Type::getInt32Ty(context), 0, "next", entryBlock.getFirstInsertionPt());
      builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), 0), nextVar);

      BasicBlock *loopStart = BasicBlock::Create(context, "loopStart", &F, &entryBlock);
      BasicBlock *loopEnd = BasicBlock::Create(context, "loopEnd", &F, &entryBlock);

      // make entryBlock point to loopStart instead of entryBlockSplit
      entryBlock.moveBefore(loopStart);
      entryBlock.getTerminator()->eraseFromParent();
      builder.SetInsertPoint(&entryBlock);
      builder.CreateBr(loopStart);

      // create the switch in loopStart
      builder.SetInsertPoint(loopStart);
      LoadInst *loadNextVar = builder.CreateLoad(nextVar->getAllocatedType(), nextVar, "next");
      SwitchInst *switchInst = builder.CreateSwitch(loadNextVar, nullptr);

      // create default block. `setDefaultDest` updates the `switch` terminator
      // and links loopStart with defaultSwitchBlock
      BasicBlock *defaultSwitchBlock = BasicBlock::Create(context, "defaultSwitchBlock", &F, loopEnd);
      switchInst->setDefaultDest(defaultSwitchBlock);

      // make default block point to the loop end
      builder.SetInsertPoint(defaultSwitchBlock);
      builder.CreateBr(loopEnd);

      // connect the end to the start of the loop
      builder.SetInsertPoint(loopEnd);
      builder.CreateBr(loopStart);

      // At this point, all blocks except the entryBlock are not reachable
      // USE BLOCKS AS SWITCH CASES

      std::map<BasicBlock *, int> blockIdxs;

      for (auto [blockIter, i] = std::tuple{F.begin(), 0}; blockIter != F.end(); blockIter++, i++) {
        blockIdxs[&*blockIter] = i;
      }

      // remove instructions referenced in multiple blocks.
      // `DemoteRegToStack` replaces them with a slot in the stack frame
      for (auto &inst: getInstructionReferencedInMultipleBlocks(F)) {
        DemoteRegToStack(*inst);
      }

      // phi nodes are dependent on predecessors and their parent nodes cannot be simply replaced.
      // `DemotePHIToStack` replaces `phi` instruction with a slot in the stack frame
      for (auto &phiNode: getPHINodes(F)) {
        DemotePHIToStack(phiNode);
      }

      // Number of created blocks: entryBlock, loopStart, loopEnd, defaultSwitchBlock
      const int generatedBlocksNum = 4;

      for (auto it = blockIdxs.begin(); it != blockIdxs.end(); it++) {
        if (it->second < generatedBlocksNum) {
          errs() << it->first->getName() << "\n";
          continue;
        }

        auto block = it->first;

        builder.SetInsertPoint(block->getTerminator());
        builder.SetInsertPoint(block);

        if (
          dyn_cast<BranchInst>(block->getTerminator()) ||
          dyn_cast<SwitchInst>(block->getTerminator())
        ) {
          block->getTerminator()->eraseFromParent();
          builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), 4), nextVar);
          builder.CreateBr(loopEnd);
        }


        switchInst->addCase(ConstantInt::get(Type::getInt32Ty(context), switchInst->getNumCases()), block);
        block->moveBefore(defaultSwitchBlock);
      }

      // UPDATE BLOCK TERMINATORS (`next` variable and branch to loopStart)

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
