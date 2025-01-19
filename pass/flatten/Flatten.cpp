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

    // Splits basic block by the last two instructions and returns a new block (the second part)
    void splitBlockByConditionalBranch(BasicBlock &block) const {
      // Move `icmp` instruction that preceeds terminating instruction
      auto lastInst = std::prev(block.end());
      if (lastInst != block.begin()) {
        --lastInst;
      }

      block.splitBasicBlock(lastInst, "entryBlockSplit");
    }

    std::map<BasicBlock *, int> generateCaseBlockIdxs(Function &F) const {
      // Number of created blocks: entryBlockSplit, loopStart, loopEnd, defaultSwitchBlock
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

    AllocaInst* allocateSwitchCaseVar(Function &F, IRBuilder<> &builder) const {
      LLVMContext &context = F.getContext();
      BasicBlock &entryBlock = F.front();

      builder.SetInsertPoint(entryBlock.getFirstInsertionPt());
      AllocaInst *caseVar = new AllocaInst(Type::getInt32Ty(context), 0, "caseVar", entryBlock.getFirstInsertionPt());

      return caseVar;
    }

    void initSwitchCaseVar(Function &F, IRBuilder<> &builder, AllocaInst *caseVar, int initValue) const {
      LLVMContext &context = F.getContext();
      BasicBlock &entryBlock = F.front();

      builder.SetInsertPoint(entryBlock.getTerminator());
      builder.CreateStore(ConstantInt::get(Type::getInt32Ty(context), initValue), caseVar);
    }

    SwitchLoop generateSwitchLoop(Function &F, IRBuilder<> &builder, AllocaInst *caseVar) const {
      LLVMContext &context = F.getContext();
      BasicBlock &entryBlock = F.front();

      BasicBlock *loopStart = BasicBlock::Create(context, "loopStart", &F, &entryBlock);
      BasicBlock *loopEnd = BasicBlock::Create(context, "loopEnd", &F, &entryBlock);

      // Make entryBlock point to loopStart
      entryBlock.moveBefore(loopStart);
      entryBlock.getTerminator()->eraseFromParent();
      builder.SetInsertPoint(&entryBlock);
      builder.CreateBr(loopStart);

      // Create the switch in loopStart
      builder.SetInsertPoint(loopStart);
      LoadInst *varLoad = builder.CreateLoad(caseVar->getAllocatedType(), caseVar, "caseVar");
      SwitchInst *switchInst = builder.CreateSwitch(varLoad, nullptr);

      // Create default block. `setDefaultDest` updates the `switch` terminator
      // and links loopStart with defaultSwitchBlock
      BasicBlock *defaultSwitchBlock = BasicBlock::Create(context, "defaultSwitchBlock", &F, loopEnd);
      switchInst->setDefaultDest(defaultSwitchBlock);

      // Make default block point to the loop end
      builder.SetInsertPoint(defaultSwitchBlock);
      builder.CreateBr(loopEnd);

      // Connect the end to the start of the loop
      builder.SetInsertPoint(loopEnd);
      builder.CreateBr(loopStart);

      return {switchInst, loopEnd};
    }

    void storeBlockSuccessorInCaseVar(
      LLVMContext &context, IRBuilder<> &builder, AllocaInst *caseVar, BasicBlock *block, std::map<BasicBlock *, int> blockCaseIdxs
    ) {
      if (dyn_cast<ReturnInst>(block->getTerminator())) {
        return;
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

        return;
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

          // If condition value equals case value: update caseVar, otherwise don't change caseVar (load previous value)
          LoadInst *varLoad = builder.CreateLoad(caseVar->getAllocatedType(), caseVar, "caseVar");

          Value *icmpEq = builder.CreateICmpEQ(condition, caseValue);
          Value *selectInst = builder.CreateSelect(
            icmpEq,
            ConstantInt::get(Type::getInt32Ty(context), caseIdx),
            varLoad
          );
          builder.CreateStore(selectInst, caseVar);
        }

        return;
      }

      throw "Unknown branch type";
    }

    void addBlockCase(
      LLVMContext &context, IRBuilder<> &builder, BasicBlock *block, int caseIdx, SwitchLoop switchLoop
    ) const {
      if (
        dyn_cast<BranchInst>(block->getTerminator()) ||
        dyn_cast<SwitchInst>(block->getTerminator())
      ) {
        block->getTerminator()->eraseFromParent();
        builder.SetInsertPoint(block);
        builder.CreateBr(switchLoop.loopEnd);
      }

      switchLoop.switchInst->addCase(ConstantInt::get(Type::getInt32Ty(context), caseIdx), block);
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
      BasicBlock &entryBlock = F.front();
      std::vector<Instruction *> usedOutside;

      for (auto &block: F) {
        for (auto &instruction: block) {
          // Ignore stack stack allocation instructions in the entry block
          if (isa<AllocaInst>(&instruction) && instruction.getParent() == &entryBlock) {
            continue;
          }

          if (instruction.isUsedOutsideOfBlock(&block)) {
            usedOutside.push_back(&instruction);
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
    todo: пропускать блоки с исключениями и indirectbr
     */
    PreservedAnalyses flattenCFG(Function &F, FunctionAnalysisManager &FAM) {
      if (F.size() == 1) {
        return PreservedAnalyses::all();
      }

      LLVMContext &context = F.getContext();
      IRBuilder<> builder(context);

      BasicBlock &entryBlock = F.front();
      entryBlock.setName("entryBlock");

      // If entry block ends with a switch or conditional branch, split the block in two
      auto entryTerminator = entryBlock.getTerminator();
      if (
        dyn_cast<SwitchInst>(entryTerminator) ||
        dyn_cast<BranchInst>(entryTerminator) && dyn_cast<BranchInst>(entryTerminator)->isConditional()
      ) {
        this->splitBlockByConditionalBranch(entryBlock);
      }

      // Entry block terminator will be deleted when creating an infinite loop.
      // Save the entry block successor beforehand to make it a default switch case
      BasicBlock *entryBlockSuccessor = entryBlock.getTerminator()->getSuccessor(0);

      auto caseVar = this->allocateSwitchCaseVar(F, builder);
      auto switchLoop = this->generateSwitchLoop(F, builder, caseVar);

      // ! At this point, all blocks except for the entry block are not reachable

      auto blockCaseIdxs = this->generateCaseBlockIdxs(F);

      // Initially, caseVar points to entry block successor (default case)
      this->initSwitchCaseVar(F, builder, caseVar, blockCaseIdxs[entryBlockSuccessor]);

      // Update caseVar in the end of every block based on terminating instruction
      for (auto it = blockCaseIdxs.begin(); it != blockCaseIdxs.end(); it++) {
        auto block = it->first;
        this->storeBlockSuccessorInCaseVar(context, builder, caseVar, block, blockCaseIdxs);
      }

      // Make each block a case inside a switch (except for the entry block)
      for (auto it = blockCaseIdxs.begin(); it != blockCaseIdxs.end(); it++) {
        auto block = it->first;
        auto caseIdx = it->second;
        this->addBlockCase(context, builder, block, caseIdx, switchLoop);
      }

      // Remove instructions referenced in multiple blocks.
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
