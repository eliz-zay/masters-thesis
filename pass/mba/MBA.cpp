#include <vector>
#include <cstdlib>

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "BaseAnnotatedPass.cpp"

using namespace llvm;

namespace {
  class MBAPass : public BaseAnnotatedPass<MBAPass> {
  private:
    static constexpr const char *annotationName = "mba";

    // Inserts MBA: 'x > 0' => '(3 - ((x >> 31) ^ 1) ^ 2 == 0) && x != 0'
    Value *insertXsgtZeroMBA_v1(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x > 0: v1\n";

      Type *xType = x->getType();

      Value *shiftedX = builder.CreateLShr(x, ConstantInt::get(xType, 31));
      Value *xorResult = builder.CreateXor(shiftedX, ConstantInt::get(xType, 1));
      Value *subResult = builder.CreateSub(ConstantInt::get(xType, 3), xorResult);
      Value *finalXor = builder.CreateXor(subResult, ConstantInt::get(xType, 2));
      Value *isFinalZero = builder.CreateICmpEQ(finalXor, Constant::getNullValue(xType));

      Value *isNonZeroX = builder.CreateICmpNE(x, Constant::getNullValue(xType));
      Value *finalResult = builder.CreateAnd(isFinalZero, isNonZeroX);

      return finalResult;
    }

    // Inserts MBA: 'x > 0' => '(((x >> 16) ^ 0xCFD00FAA >> 14) & (1 << 1)) == 0) && x != 0'
    // >> 16 --> shifts sign bit to 15th position
    // ^ 0xCFD00FAA --> does nothing, does not affect sign bit
    // >> 14 --> shifts sign bit to 2nd position
    // & (1 << 1) --> makes zero all bits except for the 2nd, with sign bit
    Value *insertXsgtZeroMBA_v2(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x > 0: v2\n";

      Type *xType = x->getType();
      int shift;
      if (xType->isIntegerTy(32)) {
        shift = 16;
      } else if (xType->isIntegerTy(64)) {
        shift = 48;
      } else {
        errs() << "[" << this->annotationName << "] Unknown operand type: " << xType << "\n";
        return nullptr;
      }

      Value *shifted16 = builder.CreateLShr(x, ConstantInt::get(xType, 16));
      Value *xorConst = ConstantInt::get(xType, 0xCFD00FAA);
      Value *xorResult = builder.CreateXor(shifted16, xorConst);
      Value *shifted14 = builder.CreateLShr(xorResult, ConstantInt::get(xType, 14));
      Value *bitMask = builder.CreateShl(ConstantInt::get(xType, 1), ConstantInt::get(xType, 1));
      Value *andResult = builder.CreateAnd(shifted14, bitMask);
      Value *cmp = builder.CreateICmpEQ(andResult, ConstantInt::get(xType, 0));

      Value *isNonZeroX = builder.CreateICmpNE(x, Constant::getNullValue(xType));
      Value *finalResult = builder.CreateAnd(cmp, isNonZeroX);

      return finalResult;
    }

    // Inserts MBA: x = 0 => 56 ^ x ^ 72 = 112
    // 56 ^ 72 equal 112, thus every bit of x must be zero
    Value *insertXeqZeroMBA_v1(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x == 0: v1\n";

      Type *xType = x->getType();

      Value *const56 = ConstantInt::get(xType, 56);
      Value *const72 = ConstantInt::get(xType, 72);
      Value *const112 = ConstantInt::get(xType, 112);

      Value *xor1 = builder.CreateXor(const56, x);
      Value *xor2 = builder.CreateXor(xor1, const72);
      Value *cmp = builder.CreateICmpEQ(xor2, const112);

      return cmp;
    }

    // x = 0 => 76 ^ ~(x ^ ~x) ^ 40 ^ x == 100
    // 76 ^ 40 = 100
    // ^ ~(x ^ ~x) makes no changes
    // ^ x checks that every bit of x is zero
    Value *insertXeqZeroMBA_v2(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x == 0: v2\n";

      Type *xType = x->getType();

      Value *const76 = ConstantInt::get(xType, 76);
      Value *const40 = ConstantInt::get(xType, 40);
      Value *const100 = ConstantInt::get(xType, 100);

      Value *notX = builder.CreateNot(x);
      Value *xorInner = builder.CreateXor(x, notX);
      Value *notXor = builder.CreateNot(xorInner);
      Value *xor1 = builder.CreateXor(const76, notXor);
      Value *xor2 = builder.CreateXor(xor1, const40);
      Value *xor3 = builder.CreateXor(xor2, x);
      Value *cmp = builder.CreateICmpEQ(xor3, const100);

      return cmp;
    }

    // x = 0 => ((x >> 6) < 5001) && (x >= 0) && (((x << 2) ^ 3) - 3 == 0)
    // ((x << 2) ^ 3) - 3 == 0 - checks that every bit of (x << 2) is zero
    // x >= 0 - checks that left bit is zero
    // (x >> 6) < 5001 - checks that second left bit is zero, otherwise x is much
    //                   larger that 5001
    Value *insertXeqZeroMBA_v3(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x == 0: v3\n";

      Type *xType = x->getType();

      Value *const5001 = ConstantInt::get(xType, 5001);
      Value *const0 = ConstantInt::get(xType, 0);
      Value *const2 = ConstantInt::get(xType, 2);
      Value *const3 = ConstantInt::get(xType, 3);

      Value *shr = builder.CreateLShr(x, 6);
      Value *cond1 = builder.CreateICmpULT(shr, const5001);

      Value *cond2 = builder.CreateICmpSGE(x, const0);

      Value *shl = builder.CreateShl(x, const2);
      Value *xor1 = builder.CreateXor(shl, const3);
      Value *sub = builder.CreateSub(xor1, const3);
      Value *cond3 = builder.CreateICmpEQ(sub, const0);

      Value *andCond = builder.CreateAnd(cond1, builder.CreateAnd(cond2, cond3));

      return andCond;
    }

    Value *pickAndInsertXsgtZeroMBA(IRBuilder<> &builder, Value *x) const {
      switch (rand() % 2) {
        case 0:
          return this->insertXsgtZeroMBA_v1(builder, x);
        case 1:
          return this->insertXsgtZeroMBA_v2(builder, x);
      }
    }

    Value *pickAndInsertXeqZeroMBA(IRBuilder<> &builder, Value *x) const {
      switch (rand() % 3) {
        case 0:
          return this->insertXeqZeroMBA_v1(builder, x);
        case 1:
          return this->insertXeqZeroMBA_v2(builder, x);
        case 2:
          return this->insertXeqZeroMBA_v3(builder, x);
      }
    }

    bool isXsgtZero(ICmpInst* icmpInst) const {
      Value *op2 = icmpInst->getOperand(1);
      return (
        icmpInst->getPredicate() == ICmpInst::ICMP_SGT
        && dyn_cast<ConstantInt>(op2)
        && dyn_cast<ConstantInt>(op2)->isZero()
      );
    }

    bool isXeqZero(ICmpInst* icmpInst) const {
      Value *op2 = icmpInst->getOperand(1);
      return (
        icmpInst->getPredicate() == ICmpInst::ICMP_EQ
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

          builder.SetInsertPoint(icmpInst);

          if (this->isXsgtZero(icmpInst)) {
            Value *mba = this->pickAndInsertXsgtZeroMBA(builder, icmpInst->getOperand(0));
            
            if (mba != nullptr) {
              icmpInst->replaceAllUsesWith(mba);
              instToDelete.push_back(icmpInst);
            }

            continue;
          }

          if (this->isXeqZero(icmpInst)) {
            Value *mba = this->pickAndInsertXeqZeroMBA(builder, icmpInst->getOperand(0));

            if (mba != nullptr) {
              icmpInst->replaceAllUsesWith(mba);
              instToDelete.push_back(icmpInst);
            }

            continue;
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
