#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {


    struct FmtPass : public InstVisitor<FmtPass> {
        FmtPass() {}

        virtual void visitCallInst(CallInst &I) {
            if (I.getCalledFunction() && I.getCalledFunction()->getName() == "printf") {
                Value * op0 = I.getArgOperand(0)->stripPointerCasts();
                errs() << *op0 << "\n";


                if (isa<GlobalVariable>(op0) && isa<Constant>(op0)) {
                    GlobalVariable * g = dyn_cast<GlobalVariable>(op0);
                    StringRef val = dyn_cast<ConstantDataSequential>(g->getInitializer())->getAsCString();

                    for (User::op_iterator i = I.op_begin(); i != I.op_end(); i++) {
                        errs() << " " << **i << "\n";
                    }

                    bool found = false;
                    User::op_iterator opiter = I.op_begin()+1;

                    IRBuilder<> builder(&I);

                    size_t format;
                    while ((format = val.find("%s")) != StringRef::npos) {
                        found = true;
                        errs() << "Contains %s! At " <<format << "\n";
                        StringRef s0 = val.slice(0,format);
                        StringRef s1 = val.slice(format,format+2);
                        val = val.slice(format+2, StringRef::npos);
                        errs() << s0 << " : " << s1 << " : " << val << "\n";

                        std::vector<Value*> args0;
                        args0.push_back(builder.CreateGlobalStringPtr(s0));

                        if (s0.size()>0) {
                            for (int i=0; i<s0.size()-1; i++) {
                                if (s0[i]=='%' && s0[i+1]!='%')
                                    args0.push_back(*(opiter++));
                            }
                            ArrayRef<Value*> args0_a(args0);
                            CallInst::Create(I.getCalledFunction(), args0_a, "", &I);
                        }
                        std::vector<Value*> args1;
                        args1.push_back(*(opiter++));
                        ArrayRef<Value*> args1_a(args1);
                        CallInst::Create(I.getCalledFunction(), args1_a, "", &I);
                    }
                    // Place the last printf if it needed to be split
                    if (found && val.size()>0) {
                        std::vector<Value*> args;
                        args.push_back(builder.CreateGlobalStringPtr(val));
                        for(; opiter!=I.op_end(); opiter++)
                            args.push_back(*opiter);
                        ArrayRef<Value*> args_a(args);
                        CallInst::Create(I.getCalledFunction(), args_a, "", &I);
                        // Remove the real printf
                        I.eraseFromParent();
                    }
                }
            }

        }
    };

    struct Magma: public FunctionPass {
        static char ID;
        Magma() : FunctionPass(ID) {}


        bool runOnFunction(Function &F) override {
            errs() << "start\n";

            FmtPass fmt;
            fmt.visit(F);

            return true;
        }
    };
}
char Magma::ID = 0;
static RegisterPass<Magma> X("magma", "magma pass", false, false);
