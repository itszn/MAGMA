#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {
    struct FmtPass : public InstVisitor<FmtPass>
    {
        FmtPass() {}

        virtual void visitCallInst(CallInst &I)
        {
            if (I.getCalledFunction()->getName() == "printf")
            {
                Value * op0 = I.getOperand(0)->stripPointerCasts();
                errs() << *op0 << "\n";

                if (isa<GlobalVariable>(op0) && isa<Constant>(op0))
                {
                    GlobalVariable * g = dyn_cast<GlobalVariable>(op0);
                    StringRef val = dyn_cast<ConstantDataSequential>(g->getInitializer())->getAsCString();
                    if (val == "%s" || val == "%s\n")
                    {
                        errs() << "blah\n";
                    }
                }
            }

        }

        virtual void visitInvokeInst(InvokeInst &I)
        {
            errs() << "invoke: " << I << "\n";
        }
    };

    struct Magma: public FunctionPass {
        static char ID;
        Magma() : FunctionPass(ID) {}


        bool runOnFunction(Function &F) override {

            FmtPass fmt;
            fmt.visit(F);

            return true;
        }
    };
}
char Magma::ID = 0;
static RegisterPass<Magma> X("magma", "magma pass", false, false);
