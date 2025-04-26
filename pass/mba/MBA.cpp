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

    // x > 0 => (3 - ((x >> 31) ^ 1) ^ 2 == 0) && x != 0
    Value *insertXsgtZero_v1(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x > 0: v1\n";

      Type *xType = x->getType();
      int shift;
      if (xType->isIntegerTy(32)) {
        shift = 31;
      } else if (xType->isIntegerTy(64)) {
        shift = 63;
      } else {
        errs() << "[" << this->annotationName << "] Unknown operand type: " << xType << "\n";
        return nullptr;
      }

      Value *shiftedX = builder.CreateLShr(x, ConstantInt::get(xType, shift));
      Value *xorResult = builder.CreateXor(shiftedX, ConstantInt::get(xType, 1));
      Value *subResult = builder.CreateSub(ConstantInt::get(xType, 3), xorResult);
      Value *finalXor = builder.CreateXor(subResult, ConstantInt::get(xType, 2));
      Value *isFinalZero = builder.CreateICmpEQ(finalXor, Constant::getNullValue(xType));

      Value *isNonZeroX = builder.CreateICmpNE(x, Constant::getNullValue(xType));
      Value *finalResult = builder.CreateAnd(isFinalZero, isNonZeroX);

      return finalResult;
    }

    // x > 0 => (((x >> 16) ^ 0xCFD00FAA >> 14) & (1 << 1)) == 0) && x != 0
    // >> 16 --> shifts sign bit to 15th position
    // ^ 0xCFD00FAA --> does nothing, does not affect sign bit
    // >> 14 --> shifts sign bit to 2nd position
    // & (1 << 1) --> makes zero all bits except for the 2nd, with sign bit
    Value *insertXsgtZero_v2(IRBuilder<> &builder, Value* x) const {
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

      Value *shifted = builder.CreateLShr(x, ConstantInt::get(xType, shift));
      Value *xorConst = ConstantInt::get(xType, 0xCFD00FAA);
      Value *xorResult = builder.CreateXor(shifted, xorConst);
      Value *shifted14 = builder.CreateLShr(xorResult, ConstantInt::get(xType, 14));
      Value *bitMask = builder.CreateShl(ConstantInt::get(xType, 1), ConstantInt::get(xType, 1));
      Value *andResult = builder.CreateAnd(shifted14, bitMask);
      Value *cmp = builder.CreateICmpEQ(andResult, ConstantInt::get(xType, 0));

      Value *isNonZeroX = builder.CreateICmpNE(x, Constant::getNullValue(xType));
      Value *finalResult = builder.CreateAnd(cmp, isNonZeroX);

      return finalResult;
    }

    // x == 0 => 56 ^ x ^ 72 = 112
    // 56 ^ 72 equal 112, thus every bit of x must be zero
    Value *insertXeqZero_v1(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x = 0: v1\n";

      Type *xType = x->getType();

      Value *const56 = ConstantInt::get(xType, 56);
      Value *const72 = ConstantInt::get(xType, 72);
      Value *const112 = ConstantInt::get(xType, 112);

      Value *xor1 = builder.CreateXor(const56, x);
      Value *xor2 = builder.CreateXor(xor1, const72);
      Value *cmp = builder.CreateICmpEQ(xor2, const112);

      return cmp;
    }

    // x == 0 => 76 ^ ~(x ^ ~x) ^ 40 ^ x == 100
    // 76 ^ 40 = 100
    // ^ ~(x ^ ~x) makes no changes
    // ^ x checks that every bit of x is zero
    Value *insertXeqZero_v2(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x = 0: v2\n";

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

    // x == 0 => ((x >> 6) < 5001) && (x >= 0) && (((x << 2) ^ 3) - 3 == 0)
    // ((x << 2) ^ 3) - 3 == 0 - checks that every bit of (x << 2) is zero
    // x >= 0 - checks that left bit is zero
    // (x >> 6) < 5001 - checks that second left bit is zero, otherwise x is much
    //                   larger that 5001
    Value *insertXeqZero_v3(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x = 0: v3\n";

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

    // x == 0 => (x << 1) ^ x == 0
    // for 100...0 left shift is compensated by x
    Value *insertXeqZero_v4(IRBuilder<> &builder, Value* x) const {
      errs() << "[" << this->annotationName << "] x = 0: v4\n";

      Type *xType = x->getType();

      Value *const0 = ConstantInt::get(xType, 0);

      Value *shl = builder.CreateShl(x, 1);
      Value *xor1 = builder.CreateXor(shl, x);
      Value *cond2 = builder.CreateICmpEQ(xor1, const0);

      return cond2;
    }

    // x + y => (x & y) + (y | x)
    Value *insertXaddY_v1(IRBuilder<> &builder, Value* x, Value* y) const {
      errs() << "[" << this->annotationName << "] x + y: v1\n";

      Value *andXY = builder.CreateAnd(x, y);
      Value *orXY = builder.CreateOr(y, x);
      Value *sum = builder.CreateAdd(andXY, orXY);

      return sum;
    }

    // x + y => ((y | x) & (y | y)) + x
    Value *insertXaddY_v2(IRBuilder<> &builder, Value* x, Value* y) const {
      errs() << "[" << this->annotationName << "] x + y: v2\n";

      Value *orYX = builder.CreateOr(y, x);
      Value *orYY = builder.CreateOr(y, y);
      Value *andResult = builder.CreateAnd(orYX, orYY);
      Value *sum = builder.CreateAdd(andResult, x);

      return sum;
    }

    // x + y => (~(y | y) ^ y ^ ~x) + y
    Value *insertXaddY_v3(IRBuilder<> &builder, Value* x, Value* y) const {
      errs() << "[" << this->annotationName << "] x + y: v3\n";

      Value *orYY = builder.CreateOr(y, y);
      Value *notOrYY = builder.CreateNot(orYY);
      Value *notX = builder.CreateNot(x);
      Value *xor1 = builder.CreateXor(notOrYY, y);
      Value *xor2 = builder.CreateXor(xor1, notX);
      Value *sum = builder.CreateAdd(xor2, y);
      
      return sum;
    }

    // x + y => y + ((y & x ^ ~y) & (x ^ y ^ y))
    Value *insertXaddY_v4(IRBuilder<> &builder, Value* x, Value* y) const {
      errs() << "[" << this->annotationName << "] x + y: v4\n";

      Value *notY = builder.CreateNot(y);
      Value *andYX = builder.CreateAnd(y, x);
      Value *xor1 = builder.CreateXor(andYX, notY);
      Value *xor2 = builder.CreateXor(x, y);
      Value *xor3 = builder.CreateXor(xor2, y);
      Value *andFinal = builder.CreateAnd(xor1, xor3);
      Value *sum = builder.CreateAdd(y, andFinal);

      return sum;
    }

    // x + y => (~y ^ x ^ y & y ^ (x | x) ^ ~(y & x | x ^ x)) + (~x ^ ~x | y | x | x | x)
    Value *insertXaddY_v5(IRBuilder<> &builder, Value* x, Value* y) const {
      errs() << "[" << this->annotationName << "] x + y: v5\n";

      Value *notY = builder.CreateNot(y);
      Value *notX = builder.CreateNot(x);

      Value *andYY = builder.CreateAnd(y, y);
      Value *xor1 = builder.CreateXor(notY, x);
      Value *xor2 = builder.CreateXor(xor1, andYY);

      Value *orXX = builder.CreateOr(x, x);
      Value *andYX = builder.CreateAnd(y, x);
      Value *xorXX = builder.CreateXor(x, x);
      Value *orInner = builder.CreateOr(andYX, xorXX);
      Value *notOrInner = builder.CreateNot(orInner);
      Value *xor3 = builder.CreateXor(orXX, notOrInner);

      Value *left = builder.CreateXor(xor2, xor3);

      Value *notX_x2 = builder.CreateXor(notX, notX);
      Value *or1 = builder.CreateOr(notX_x2, y);
      Value *or2 = builder.CreateOr(or1, x);
      Value *or3 = builder.CreateOr(or2, x);
      Value *or4 = builder.CreateOr(or3, x);

      Value *sum = builder.CreateAdd(left, or4);

      return sum;
    }

    // x + y => (x | (~x | ~x) & (y ^ y | y ^ y) ^ x & (~x ^ (y | x))) + y
    Value *insertXaddY_v6(IRBuilder<> &builder, Value* x, Value* y) const {
      errs() << "[" << this->annotationName << "] x + y: v6\n";

      Value *notX = builder.CreateNot(x);
      Value *notX_or = builder.CreateOr(notX, notX);
      Value *xorYY_1 = builder.CreateXor(y, y);
      Value *xorYY_2 = builder.CreateXor(y, y);
      Value *orYY = builder.CreateOr(xorYY_1, xorYY_2);
      Value *and1 = builder.CreateAnd(notX_or, orYY);
      Value *xor1 = builder.CreateXor(and1, x);

      Value *orYX = builder.CreateOr(y, x);
      Value *xorNX_orYX = builder.CreateXor(notX, orYX);
      Value *and2 = builder.CreateAnd(x, xorNX_orYX);

      Value *orLeft = builder.CreateOr(x, xor1);
      Value *finalLeft = builder.CreateOr(orLeft, and2);

      Value *sum = builder.CreateAdd(finalLeft, y);

      return sum;
    }

    Value *pickAndInsertXsgtZero(IRBuilder<> &builder, Value *x) const {
      switch (rand() % 2) {
        case 0:
          return this->insertXsgtZero_v1(builder, x);
        case 1:
          return this->insertXsgtZero_v2(builder, x);
      }
    }

    Value *pickAndInsertXeqZero(IRBuilder<> &builder, Value *x) const {
      switch (rand() % 4) {
        case 0:
          return this->insertXeqZero_v1(builder, x);
        case 1:
          return this->insertXeqZero_v2(builder, x);
        case 2:
          return this->insertXeqZero_v3(builder, x);
        case 3:
          return this->insertXeqZero_v4(builder, x);
      }
    }

    Value *pickAndInsertXaddY(IRBuilder<> &builder, Value *x, Value *y) const {
      switch (rand() % 6) {
        case 0:
          return this->insertXaddY_v1(builder, x, y);
        case 1:
          return this->insertXaddY_v2(builder, x, y);
        case 2:
          return this->insertXaddY_v3(builder, x, y);
        case 3:
          return this->insertXaddY_v4(builder, x, y);
        case 4:
          return this->insertXaddY_v5(builder, x, y);
        case 5:
          return this->insertXaddY_v6(builder, x, y);
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

    bool isXaddY(Instruction &inst) const {
      return inst.getOpcode() == Instruction::Add;
    }

    PreservedAnalyses applyPass(Function &F) const override {
      LLVMContext &context = F.getContext();
      IRBuilder<> builder(context);

      std::vector<Instruction *> instToDelete;

      for (auto &block : F) {
        for (auto &instruction : block) {
          builder.SetInsertPoint(&instruction);

          Value *mba = nullptr;
          
          if (auto icmpInst = dyn_cast<ICmpInst>(&instruction)) {
            if (this->isXsgtZero(icmpInst)) {
              mba = this->pickAndInsertXsgtZero(builder, icmpInst->getOperand(0));
            } else if (this->isXeqZero(icmpInst)) {
              mba = this->pickAndInsertXeqZero(builder, icmpInst->getOperand(0));
            }
          } else if (this->isXaddY(instruction)) {
            mba = this->pickAndInsertXaddY(builder, instruction.getOperand(0), instruction.getOperand(1));
          }

          if (mba != nullptr) {
            instruction.replaceAllUsesWith(mba);
            instToDelete.push_back(&instruction);
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
