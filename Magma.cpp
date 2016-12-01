#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {

    struct NullFreeFinderPass: public FunctionPass {
        NullFreeFinderPass(char ID) : FunctionPass(ID) {}

        bool compareInstructionAsUsers(Instruction* a, Instruction* b, int limit=10) {
            if (limit == 0)
                return false;
            errs() << "comparing " << *a << " to " << *b << "\n";
            if (a==b) {
                return true;
            }
            if (a->getOpcodeName() != b->getOpcodeName())
                return false;
            if (a->getNumOperands() != b->getNumOperands())
                return false;

            for (User::op_iterator i = a->op_begin(), j = b->op_begin();
                    i != a->op_end() && j != b->op_end(); i++, j++) {
                Instruction* ii = dyn_cast<Instruction>(*i);
                Instruction* ij = dyn_cast<Instruction>(*j);

                if (ii && ij) {
                    if (!compareInstructionAsUsers(ii, ij, limit-1))
                        return false;
                } else if (*i != *j)
                    return false;

            }
            return true;

        }

        bool runOnFunction(Function &F) override {
            std::vector<Instruction*> freed;
            std::vector<Instruction*> toRemove;

            for (auto &block : F) {
                for (auto &inst : block) {
                    if (CallInst* I = dyn_cast<CallInst>(&inst)) {

                        if (I->getCalledFunction() && I->getCalledFunction()->getName() == "free") {
                            errs() << *I << "\n";
                            errs() << *I->getArgOperand(0) << "\n";
                            Value* ptr = I->getArgOperand(0);
                            if (Instruction* bitcast = dyn_cast<Instruction>(ptr)) {
                                if (Instruction* load = dyn_cast<Instruction>(bitcast->getOperand(0))) {
                                    errs() << "Freed " << *load->getOperand(0) << "\n";
                                    if (Instruction* realVal = dyn_cast<Instruction>(load->getOperand(0)))
                                        freed.push_back(realVal);
                                }
                            }
                        }
                    }
                    if (StoreInst* I = dyn_cast<StoreInst>(&inst)) {
                        errs() << *I << "\n";
                        if (Instruction* realVal = dyn_cast<Instruction>(I->getOperand(1))) {
                            for (int i=0; i<freed.size(); i++) {
                                if (compareInstructionAsUsers(freed[i], realVal)) {
                                    freed.erase(find(freed.begin(), freed.end(), freed[i]));
                                    errs() << *I->getOperand(0) << "\n";
                                    if (isa<ConstantPointerNull>(I->getOperand(0))) {
                                        errs() << "NULL\n";
                                        toRemove.push_back(I);
                                    }

                                    break;
                                }

                            }
                        }
                    }
                }
            }
            errs() << "got to here\n";
            for (int i=0; i< toRemove.size(); i++)
                toRemove[i]->eraseFromParent();
            return true;
        }
    };



    struct MemPermsPass : public InstVisitor<MemPermsPass> {
        MemPermsPass() {}

        virtual void visitCallInst(CallInst &I) {
            if (I.getCalledFunction() && (I.getCalledFunction()->getName() == "mmap" ||
                    I.getCalledFunction()->getName() == "mprotect")) {
                Value* op2 = I.getArgOperand(2);
                if (isa<ConstantInt>(op2)) {
                    I.setArgOperand(2, ConstantInt::get(op2->getType(),7));
                }
            }
        }
    };


    struct FmtPass : public InstVisitor<FmtPass> {
        FmtPass() {}

        virtual void visitCallInst(CallInst &I) {
            if (I.getCalledFunction() && I.getCalledFunction()->getName() == "printf") {
                Value * op0 = I.getArgOperand(0)->stripPointerCasts();
                errs() << *op0 << "\n";


                if (isa<GlobalVariable>(op0) && isa<Constant>(op0)) {
                    GlobalVariable * g = dyn_cast<GlobalVariable>(op0);
                    StringRef val = dyn_cast<ConstantDataSequential>(g->getInitializer())->getAsCString();


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
                        CallInst* last = CallInst::Create(I.getCalledFunction(), args_a, "");
                        ReplaceInstWithInst(&I, last);
                    }
                }
            }

        }
    };

    struct GetsPass : public InstVisitor<GetsPass>
    {
        GetsPass() {}

        virtual void visitCallInst(CallInst &I)
        {
            Type * arg_type = Type::getInt8PtrTy(I.getModule()->getContext());
            Type * arg_types[] = {arg_type};
            FunctionType * func_type = FunctionType::get(arg_type, ArrayRef<Type*>(arg_types, 1), 0);
            Constant * gets_func_const = I.getModule()->getOrInsertFunction("gets", func_type);
            Function * gets_func = cast<Function>(gets_func_const);

            Function * F = I.getCalledFunction();
            StringRef fname = F->getName();
            if (fname == "fgets")
            {
                Value * op2 = I.getArgOperand(2)->stripPointerCasts();
                LoadInst * op2inst = dyn_cast<LoadInst>(op2);
                if (op2inst->getPointerOperand()->stripPointerCasts()->getName() == "stdin")
                {
                    Value * new_args[] = {I.getArgOperand(0)};
                    CallInst * c = CallInst::Create(gets_func, ArrayRef<Value*>(new_args, 1), "");
                    ReplaceInstWithInst(&I, c);
                }
            }
            else if (fname == "read")
            {
                Value * op0 = I.getArgOperand(0)->stripPointerCasts();
                errs() << *op0 << "\n";
                if (dyn_cast<ConstantInt>(op0)->isZero() && 0)
                {
                    Value * new_args[] = {I.getArgOperand(1)};
                    CallInst * c = CallInst::Create(gets_func, ArrayRef<Value*>(new_args, 1), "");
                    errs() << "r... : " << I << "\n";
                    errs() << "read : " << *c << "\n";
                    ReplaceInstWithInst(&I, c);
                }
            }
        }
    };

    struct VolatilePass : public InstVisitor<VolatilePass>
    {
        VolatilePass() {}

        virtual void visitLoadInst(LoadInst & I) { if (I.isVolatile()) I.setVolatile(0); }
        virtual void visitStoreInst(StoreInst & I) { if (I.isVolatile()) I.setVolatile(0); }
        virtual void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) { if (I.isVolatile()) I.setVolatile(0); }
        virtual void visitMemIntrinsic(MemIntrinsic &I) { if (I.isVolatile()) I.setVolatile(0); }
    };

    struct OffByOnePass : public InstVisitor<OffByOnePass>
    {
        OffByOnePass() {}

        virtual void visitICmpInst(CmpInst & I)
        {
            CmpInst::Predicate pred = I.getPredicate();
            switch(pred)
            {
                case CmpInst::Predicate::ICMP_ULT:
                    pred = CmpInst::Predicate::ICMP_ULE;
                    break;
                case CmpInst::Predicate::ICMP_SLT:
                    pred = CmpInst::Predicate::ICMP_SLE;
                    break;
                default:
                    break;
            }

            I.setPredicate(pred);
        }
    };

    struct Magma: public FunctionPass {
        static char ID;
        Magma() : FunctionPass(ID) {}

        void remove_stack_canary(Function &F)
        {
            StringRef attr = "stack-protector-buffer-size";
            if (F.hasFnAttribute(attr)) F.addFnAttr(attr);
        }

        bool runOnFunction(Function &F) override {

            FmtPass fmt;
            //fmt.visit(F);

            MemPermsPass mpp;
            mpp.visit(F);

            GetsPass gets;
            gets.visit(F);

            VolatilePass vol;
            vol.visit(F);

            OffByOnePass off;
            off.visit(F);

            remove_stack_canary(F);

            NullFreeFinderPass nffp(ID);
            nffp.runOnFunction(F);

            return true;
        }
    };
}

char Magma::ID = 0;
static RegisterPass<Magma> X("magma", "magma pass", false, false);

