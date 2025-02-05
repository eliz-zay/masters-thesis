#include <vector>
#include <map>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Attributes.h"
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
    Type *returnType;
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
      // First argument stands for return value
      for (int i = 0; i < funcInfo.argNum - 1; i++) {
        Value *srcArg = func->getArg(i);
        Value *dstArg = mergedFunc->getArg(funcInfo.argOffset + 1 + i);

        vMap[srcArg] = dstArg;
      }

      BasicBlock *lastBlock = &(mergedFunc->back());

      // func->getAttributes().removeRetAttribute(func->getContext(), Attribute::Range);

      AttributeList attrs = func->getAttributes();
      AttributeList newAttrs = attrs.removeFnAttributes(func->getContext());
      func->setAttributes(newAttrs);

      attrs = mergedFunc->getAttributes();
      newAttrs = attrs.removeFnAttributes(mergedFunc->getContext());
      func->setAttributes(newAttrs);


      SmallVector<ReturnInst *, 8> returns;
      CloneFunctionInto(mergedFunc, func, vMap, CloneFunctionChangeType::LocalChangesOnly, returns);

      auto clonedEntryBlock = lastBlock->getIterator()++;
      while (clonedEntryBlock != mergedFunc->end() && !clonedEntryBlock->hasNPredecessors(0)) {
        clonedEntryBlock++;
      }

      switchInst->addCase(ConstantInt::get(Type::getInt32Ty(context), funcInfo.caseIdx), &*clonedEntryBlock);

      IRBuilder<> builder(context);

      // set return variable and create return void
      for (auto block = clonedEntryBlock->getIterator(); block != mergedFunc->end(); block++) {
        auto returnInst = dyn_cast<ReturnInst>(block->getTerminator());
        if (!returnInst) {
          continue;
        }

        Value *returnValue = returnInst->getReturnValue();
        // void return instructions are already valid
        if (!returnValue) {
          continue;
        }

        // Copy return value to the argument with a result pointer
        builder.SetInsertPoint(returnInst);
        builder.CreateStore(returnValue, mergedFunc->getArg(funcInfo.argOffset));
        builder.CreateRetVoid();

        returnInst->eraseFromParent();
      }
    }

    MergedFunction merge(Module &M, std::vector<Function *> targetFuncs) const {
      LLVMContext &context = M.getContext();

      std::map<Function *, FunctionInfo> targetFuncsInfo;

      // Merged function args start with an integer (switch case variable)
      std::vector<Type *> argTypes = {Type::getInt32Ty(context)};
      int argOffset = 1;
      int caseIdx = 0;

      for (auto &f : targetFuncs) {
        int argNum = 1;

        argTypes.push_back(PointerType::get(f->getReturnType(), 0));

        for (auto &arg : f->args()) {
          argTypes.push_back(arg.getType());
          argNum++;
        }

        targetFuncsInfo[f] = {caseIdx, argOffset, argNum, f->getReturnType()};
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

    void updateFunctionReferences(Function *mergedFunc, Function *func, FunctionInfo &funcInfo) const {
      LLVMContext &context = mergedFunc->getContext();
      IRBuilder<> builder(context);

      int mergedArgNum = std::distance(mergedFunc->arg_begin(), mergedFunc->arg_end());
      const auto [caseIdx, argOffset, argNum, returnType] = funcInfo;

      errs() << " --- " << func->getName() << " --- \n";

      bool hasInvokeRefs = false;
      std::vector<CallInst *> callInstToDelete;

      for (auto &use : func->uses()) {
        auto user = use.getUser();

        if (dyn_cast<InvokeInst>(user)) {
          hasInvokeRefs = true;
          continue;
        }

        auto *callInst = dyn_cast<CallInst>(user);
        if (!callInst) {
          errs() << "not call\n";
          continue;
        }

        errs() << "call\n";

        builder.SetInsertPoint(callInst);

        Value *resultVar = !returnType->isVoidTy()
          ? builder.CreateAlloca(returnType, nullptr)
          : nullptr;

        std::vector<Value *> args = {ConstantInt::get(Type::getInt32Ty(context), caseIdx)};

        auto callInstArg = callInst->arg_begin();          
        for (int i = 1; i < mergedArgNum; i++) {
          // Pass pointer to return var if it's not a void function
          if (i == argOffset) {
            if (!returnType->isVoidTy()) {
              args.push_back(resultVar);
            } else {
              args.push_back(Constant::getNullValue(mergedFunc->getArg(i)->getType()));
            }
          // Add mock arguments on positions of other merged functions
          } else if (i < argOffset || i >= argOffset + argNum) {
            args.push_back(Constant::getNullValue(mergedFunc->getArg(i)->getType()));
          // Copy actual arguments
          } else {
            args.push_back(*callInstArg);
            callInstArg++;
          }
        }

        builder.CreateCall(mergedFunc, args);

        if (!returnType->isVoidTy()) {
          callInst->replaceAllUsesWith(builder.CreateLoad(returnType, resultVar));
        }

        // Delete later to avoid modifying `uses()` iterator in for loop
        callInstToDelete.push_back(callInst);
      }

      for (auto &callInst : callInstToDelete) {
        callInst->eraseFromParent();
      }

      if (!hasInvokeRefs) {
        // The function can be deleted
      }
  }

  public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) const {
      auto targetFuncs = this->getTargetFunctions(M);

      if (targetFuncs.size() < 2) {
        return PreservedAnalyses::all();
      }

      auto [mergedFunc, targetFuncsInfoMap] = this->merge(M, targetFuncs);

      for (auto &[func, funcInfo] : targetFuncsInfoMap) {
        this->updateFunctionReferences(mergedFunc, func, funcInfo);
      }

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
