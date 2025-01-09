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
  struct SwitchLoop {
    SwitchInst *switchInst;
    BasicBlock *loopEnd;
  };

  struct FlattenPass : public PassInfoMixin<FlattenPass> {
  private:
    const std::string annotationName = "flatten";

    // Split basic block by the last two instructions and returns a new block (the second part)
    void splitBlockByConditionalBranch(BasicBlock &block) const {
      // also take into account the `icmp` instruction that preceeds the `br` instruction
      auto lastInst = std::prev(block.end());
      if (lastInst != block.begin()) {
        --lastInst;
      }

      block.splitBasicBlock(lastInst, "entryBlockSplit");
    }

    std::map<BasicBlock *, int> generateCaseBlockIdxs(Function &F) const {
      // Number of created blocks: entryBlock, loopStart, loopEnd, defaultSwitchBlock
      const int generatedBlocksNum = 4;

      std::map<BasicBlock *, int> caseBlockIdxs;

      int i = 0;
      auto blockIt = F.begin();
      std::advance(blockIt, generatedBlocksNum);

      for (; blockIt != F.end(); blockIt++, i++) {
        caseBlockIdxs[&*blockIt] = i;
      }

      return caseBlockIdxs;
    }

    AllocaInst* initializeSwitchCaseVar(Function &F, IRBuilder<> &builder) {
      LLVMContext &context = F.getContext();
      BasicBlock &entryBlock = F.front();

      builder.SetInsertPoint(entryBlock.getFirstInsertionPt());
      AllocaInst *caseVar = new AllocaInst(Type::getInt32Ty(context), 0, "caseVar", entryBlock.getFirstInsertionPt());
      builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), 0), caseVar);

      return caseVar;
    }

    SwitchLoop generateSwitchLoop(Function &F, IRBuilder<> &builder, AllocaInst *caseVar) const {
      LLVMContext &context = F.getContext();
      BasicBlock &entryBlock = F.front();

      BasicBlock *loopStart = BasicBlock::Create(context, "loopStart", &F, &entryBlock);
      BasicBlock *loopEnd = BasicBlock::Create(context, "loopEnd", &F, &entryBlock);

      // make entryBlock point to loopStart instead of entryBlockSplit
      entryBlock.moveBefore(loopStart);
      entryBlock.getTerminator()->eraseFromParent();
      builder.SetInsertPoint(&entryBlock);
      builder.CreateBr(loopStart);

      // create the switch in loopStart
      builder.SetInsertPoint(loopStart);
      LoadInst *varLoad = builder.CreateLoad(caseVar->getAllocatedType(), caseVar, "caseVar");
      SwitchInst *switchInst = builder.CreateSwitch(varLoad, nullptr);

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

      return {switchInst, loopEnd};
    }

    std::vector<PHINode *> getPHINodes(Function &F) const {
      std::vector<PHINode *> nodes;

      for (auto &block: F) {
        for (auto &inst: block) {
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
          if (inst.isUsedOutsideOfBlock(&block)) {
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
     */
    PreservedAnalyses flattenCFG(Function &F, FunctionAnalysisManager &FAM) {
      LLVMContext &context = F.getContext();
      IRBuilder<> builder(context);

      BasicBlock &entryBlock = F.front();
      entryBlock.setName("entryBlock");

      auto it = F.begin();
      it++;
      (*it).setName("if-else_1");
      it++;
      (*it).setName("while_%3");
      it++;
      (*it).setName("switch");

      // Split conditional branch in two
      if (BranchInst *branch = dyn_cast<BranchInst>(entryBlock.getTerminator())) {
        if (branch->isConditional()) {
          this->splitBlockByConditionalBranch(entryBlock);
        }
      }

      auto caseVar = this->initializeSwitchCaseVar(F, builder);
      auto [switchInst, loopEnd] = this->generateSwitchLoop(F, builder, caseVar);

      // At this point, all blocks except the entryBlock are not reachable

      auto blockCaseIdxs = this->generateCaseBlockIdxs(F);

      for (auto it = blockCaseIdxs.begin(); it != blockCaseIdxs.end(); it++) {
        auto block = it->first;

        if (dyn_cast<ReturnInst>(block->getTerminator())) {
          continue;
        }

        builder.SetInsertPoint(block->getTerminator());
        
        if (auto branch = dyn_cast<BranchInst>(block->getTerminator())) {
          if (branch->isUnconditional()) {
            auto successor = branch->getSuccessor(0);
            auto caseIdx = blockCaseIdxs[successor];

            builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), caseIdx), caseVar);
          } else {
            auto successorTrue = branch->getSuccessor(0);
            auto successorFalse = branch->getSuccessor(1);

            auto trueCaseIdx = blockCaseIdxs[successorTrue];
            auto falseCaseIdx = blockCaseIdxs[successorFalse];

            Value *selectInst = builder.CreateSelect(
              branch->getCondition(),
              ConstantInt::get(Type::getInt32Ty(context), trueCaseIdx),
              ConstantInt::get(Type::getInt32Ty(context), falseCaseIdx),
              "casevar_branch"
            );
            builder.CreateStore(selectInst, caseVar);
          }

          continue;
        }

        if (auto blockSwitchInst = dyn_cast<SwitchInst>(block->getTerminator())) {
          Value *condition = blockSwitchInst->getCondition();

          // In LLVM switch representation, the default switch case points to the next block
          // in condition if no case is matched, even if there os no explicit default case
          auto defaultBlockIdx = blockCaseIdxs[blockSwitchInst->case_default()->getCaseSuccessor()];
          builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), defaultBlockIdx), caseVar);

          for (auto &switchCase : blockSwitchInst->cases()) {
            ConstantInt *caseValue = switchCase.getCaseValue();
            BasicBlock *caseBlock = switchCase.getCaseSuccessor();
            int caseIdx = blockCaseIdxs[caseBlock];

            Value *icmpEq = builder.CreateICmpEQ(condition, caseValue, "case_eq");
            Value *selectInst = builder.CreateSelect(
              icmpEq,
              ConstantInt::get(Type::getInt32Ty(context), caseIdx),
              ConstantInt::get(Type::getInt32Ty(context), defaultBlockIdx),
              "casevar_case"
            );
            builder.CreateStore(selectInst, caseVar);
          }

          continue;
        }

        throw "Unknown branch type";
      }

      // упорядочить создание кейсов по индексам, для красоты
      for (auto it = blockCaseIdxs.begin(); it != blockCaseIdxs.end(); it++) {
        auto block = it->first;
        auto caseIdx = it->second;

        if (
          dyn_cast<BranchInst>(block->getTerminator()) ||
          dyn_cast<SwitchInst>(block->getTerminator())
        ) {
          block->getTerminator()->eraseFromParent();
          builder.SetInsertPoint(block);
          builder.CreateBr(loopEnd);
        }

        switchInst->addCase(ConstantInt::get(Type::getInt32Ty(context), caseIdx), block);
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

      return PreservedAnalyses::all();
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
