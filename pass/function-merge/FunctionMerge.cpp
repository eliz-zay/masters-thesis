#include <vector>
#include <map>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "BaseAnnotatedPass.cpp"

using namespace llvm;

namespace {
  struct FunctionInfo {
    int caseIdx;
    int argOffset;
    int argNum;
  };

  struct MergedFunction {
    Function *mergedFunc;
    std::map<Function *, FunctionInfo> targetFuncs;
  };

  class FunctionMergePass : public PassInfoMixin<FunctionMergePass> {
  private:
    static constexpr const char *annotationName = "function-merge";

    std::vector<Function *> getTargetFunctions(Module &M) const {
      std::vector<Function *> annotatedFunctions;

      for (Function &F : M) {
        auto *md = F.getMetadata("annotation");
        if (!md) {
          continue;
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
          continue;
        }

        if (
          F.isVarArg()
          || F.isDeclaration()
          || F.isIntrinsic()
          || F.getLinkage() != GlobalValue::LinkageTypes::InternalLinkage
          || !F.getReturnType()->isVoidTy()
        ) {
          continue;
        }

        errs() << "[" << this->annotationName << "] Applying to: " << F.getName() << "\n";
        annotatedFunctions.push_back(&F);
      }

      return annotatedFunctions;
    }

    Function* createVoidFunction(Module &M, std::vector<Type *> &argTypes) const {
      LLVMContext &context = M.getContext();

      FunctionType *funcType = FunctionType::get(Type::getVoidTy(context), argTypes, false);

      Function *newFunc = Function::Create(funcType, GlobalValue::LinkageTypes::InternalLinkage, "merged", M);

      return newFunc;
    }

    SwitchInst* createSwitchCase(Function *mergedFunc) const {
      LLVMContext &context = mergedFunc->getContext();

      BasicBlock *entryBlock = BasicBlock::Create(context, "entry", mergedFunc);

      IRBuilder<> builder(entryBlock);

      SwitchInst *switchInst = builder.CreateSwitch(mergedFunc->getArg(0), nullptr);

      BasicBlock *defaultBlock = BasicBlock::Create(context, "defaultSwitchBlock", mergedFunc);
      ReturnInst::Create(context, defaultBlock);
      switchInst->setDefaultDest(defaultBlock);

      return switchInst;
    }

    void addCase(
      Function *mergedFunc,
      SwitchInst *switchInst,
      Function *func,
      FunctionInfo &funcInfo
    ) const {
      LLVMContext &context = mergedFunc->getContext();

      // Argument references during function cloning are updated according to the `vMap` argument mapping
      ValueToValueMapTy vMap;
      for (int i = 0; i < funcInfo.argNum; i++) {
        Value *srcArg = func->getArg(i);
        Value *dstArg = mergedFunc->getArg(funcInfo.argOffset + i);

        vMap[srcArg] = dstArg;
      }

      BasicBlock *lastBlock = &(mergedFunc->back());

      SmallVector<ReturnInst *, 8> returns;
      CloneFunctionInto(mergedFunc, func, vMap, CloneFunctionChangeType::LocalChangesOnly, returns);

      auto clonedEntryBlock = lastBlock->getIterator()++;
      while (clonedEntryBlock != mergedFunc->end() && !clonedEntryBlock->hasNPredecessors(0)) {
        ++clonedEntryBlock;
      }

      switchInst->addCase(ConstantInt::get(Type::getInt32Ty(context), funcInfo.caseIdx), &*clonedEntryBlock);
    }

    MergedFunction merge(Module &M, std::vector<Function *> targetFuncs) const {
      LLVMContext &context = M.getContext();

      std::map<Function *, FunctionInfo> targetFuncsInfo;

      // Merged function args start with an integer (switch case variable)
      std::vector<Type *> argTypes = {Type::getInt32Ty(context)};
      int argOffset = 1;
      int caseIdx = 0;

      for (auto &f : targetFuncs) {
        int argNum = 0;
        for (auto &arg : f->args()) {
          argTypes.push_back(arg.getType());
          argNum++;
        }

        targetFuncsInfo[f] = {caseIdx, argOffset, argNum};
        argOffset += argNum;
        caseIdx++;
      }

      auto mergedFunc = this->createVoidFunction(M, argTypes);
      auto switchInst = this->createSwitchCase(mergedFunc);

      for (auto &f : targetFuncs) {
        auto info = targetFuncsInfo[f];
        this->addCase(mergedFunc, switchInst, f, info);
      }

      return {mergedFunc, targetFuncsInfo};
    }

    void updateFunctionReferences(Function *mergedFunc, std::map<Function *, FunctionInfo> &targetFuncs) const {
      LLVMContext &context = mergedFunc->getContext();
      IRBuilder<> builder(context);

      int mergedArgNum = std::distance(mergedFunc->arg_begin(), mergedFunc->arg_end());

      for (auto &[targetFunc, targetFuncInfo] : targetFuncs) {
        const int argOffset = targetFuncInfo.argOffset;
        const int argNum = targetFuncInfo.argNum;
        errs() << " *** " << targetFunc->getName() << " *** \n";

        for (auto &use : targetFunc->uses()) {
          auto user = use.getUser();

          auto *callInst = dyn_cast<CallInst>(user);
          if (!callInst) {
            errs() << "not call\n";
            continue;
          }

          errs() << "call\n";

          builder.SetInsertPoint(callInst);

          std::vector<Value *> args = {ConstantInt::get(Type::getInt32Ty(context), targetFuncInfo.caseIdx)};

          auto callInstArg = callInst->arg_begin();
          
          for (int i = 1; i < mergedArgNum; i++) {
            if (i < argOffset || i >= argOffset + argNum) {
              args.push_back(Constant::getNullValue(mergedFunc->getArg(i)->getType()));
            } else {
              args.push_back(*callInstArg);
              callInstArg++;
            }
          }

          builder.CreateCall(mergedFunc, args);

          // call->replaceAllUsesWith(builder.CreateLoad(result->getAllocatedType(), result));

          // callInst->eraseFromParent();
        }
      }
    }

  public:
    /*
      todo: process non-void functions
    */
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) const {
      auto targetFuncs = this->getTargetFunctions(M);

      if (targetFuncs.size() < 2) {
        return PreservedAnalyses::all();
      }

      auto [mergedFunc, targetFuncsInfo] = this->merge(M, targetFuncs);

      this->updateFunctionReferences(mergedFunc, targetFuncsInfo);

      return PreservedAnalyses::none();
    }
  };
} // namespace

PassPluginLibraryInfo getFunctionMergePassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "FunctionMergePass",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](
          StringRef Name,
          ModulePassManager &MPM,
          ArrayRef<PassBuilder::PipelineElement>
        ) {
          if (Name == "function-merge") {
            MPM.addPass(FunctionMergePass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getFunctionMergePassPluginInfo();
}
