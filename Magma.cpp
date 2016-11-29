#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {
    struct Magma: public FunctionPass {
        static char ID;
        Magma() : FunctionPass(ID) {}


        bool runOnFunction(Function &F) override {
            return true;
        }
    };
}
char Magma::ID = 0;
static RegisterPass<Magma> X("magma", "magma pass", false, false);
