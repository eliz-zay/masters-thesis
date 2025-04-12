#include <vector>
#include <map>
#include <cmath>

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "BaseAnnotatedPass.cpp"

using namespace llvm;

namespace {
  class MBAPass : public BaseAnnotatedPass<MBAPass> {
  private:
    static constexpr const char *annotationName = "mba";

    // Inserts MBA: 'x > 0' => '3 - (((x - 1) >> 31) ^ 1) ^ 2 == 0'
    Value *insertXsgtZeroMBA(IRBuilder<> &builder, Value* x) const {
      Value *sub1 = builder.CreateSub(x, builder.getInt32(1));
      Value *ashr = builder.CreateAShr(sub1, builder.getInt32(31));
      Value *xor1 = builder.CreateXor(ashr, builder.getInt32(1));
      Value *xor2 = builder.CreateXor(xor1, builder.getInt32(2));
      Value *sub3 = builder.CreateSub(builder.getInt32(3), xor2);
      Value *eq0 = builder.CreateICmpEQ(sub3, builder.getInt32(0));
      return eq0;
    }

    // Inserts MBA: 'x > 0' => '(((-(1 - x) >> 16) ^ 0xCFD00FAA >> 14) & (1 << 1)) == 0'
    // -(1 - x) --> ensure that 0 corresponds to sign bit 1
    // >> 16 --> shifts sign bit to 15th position
    // ^ 0xCFD00FAA --> does nothing, does not affect sign bit
    // >> 14 --> shifts sign bit to 2nd position
    // & (1 << 1) --> makes zero all bits except for the 2nd
    Value *insertXsgtZeroMBA_V2(IRBuilder<> &builder, Value* x) const {
      Value *one = builder.getInt32(1);
      Value *negOneMinusX = builder.CreateSub(one, x);
      Value *negResult = builder.CreateNeg(negOneMinusX);
      Value *shifted16 = builder.CreateLShr(negResult, builder.getInt32(16));
      Value *xorValue = builder.getInt32(0xCFD00FAA);
      Value *xorResult = builder.CreateXor(shifted16, xorValue);
      Value *shifted14 = builder.CreateLShr(xorResult, builder.getInt32(14));
      Value *andResult = builder.CreateAnd(shifted14, builder.CreateShl(builder.getInt32(1), builder.getInt32(1)));
      Value *isEqualZero = builder.CreateICmpEQ(andResult, builder.getInt32(0));
      return isEqualZero;
    }

    int getZeroOperandIdx(ICmpInst* icmpInst) const {
      Value *op1 = icmpInst->getOperand(0);
      Value *op2 = icmpInst->getOperand(1);

      if (dyn_cast<ConstantInt>(op1) && dyn_cast<ConstantInt>(op1)->isZero())  {
        return 0;
      }
      if (dyn_cast<ConstantInt>(op2) && dyn_cast<ConstantInt>(op2)->isZero())  {
        return 1;
      }

      return -1;
    }

    bool isXsgtZero(ICmpInst* icmpInst) const {
      Value *op2 = icmpInst->getOperand(1);
      return (
        icmpInst->getPredicate() == ICmpInst::ICMP_SGT
        && dyn_cast<ConstantInt>(op2)
        && dyn_cast<ConstantInt>(op2)->isZero()
      );
    }

    PreservedAnalyses applyPass(Function &F) const override {
      LLVMContext &context = F.getContext();
      IRBuilder<> builder(context);

      std::vector<ICmpInst *> instToDelete;

      for (auto &block : F) {
        for (auto &instruction : block) {
          auto icmpInst = dyn_cast<ICmpInst>(&instruction);
          if (!icmpInst) {
            continue;
          }

          if (this->isXsgtZero(icmpInst)) {
            builder.SetInsertPoint(icmpInst);

            Value *mba = this->insertXsgtZeroMBA_V2(builder, icmpInst->getOperand(0));

            icmpInst->replaceAllUsesWith(mba);
            instToDelete.push_back(icmpInst);
          }
        }
      }

      for (auto &inst : instToDelete) {
        inst->eraseFromParent();
      }

      return PreservedAnalyses::none();
    }

  public:
    MBAPass() : BaseAnnotatedPass(MBAPass::annotationName) {}
  };
} // namespace

PassPluginLibraryInfo getMBAPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "MBAPass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](
          StringRef Name,
          FunctionPassManager &FPM,
          ArrayRef<PassBuilder::PipelineElement>
        ) {
          if (Name == "mba") {
            FPM.addPass(MBAPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getMBAPassPluginInfo();
}
