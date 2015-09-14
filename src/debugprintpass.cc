#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/IRBuilder.h>
#include <set>
#include <iostream>

#include "debugprintpass.h"

char DebugPrintPass::ID;

using namespace llvm;

static void debug_print(const char* s, uint64_t v)
{
	std::cerr << s << ": " << v << '\n';
}

bool DebugPrintPass::runOnFunction(Function &f)
{
	Instruction* last_inst = nullptr;
	std::set<std::pair<Instruction*, Instruction*>> debug_insts;

	assert (debug_func_ty);

	for (auto& bb : f) {
	for (auto& i : bb) {
		unsigned sz = i.getType()->getPrimitiveSizeInBits();
		if (last_inst) {
			// inserting before phi node will break
			// the verifier if more than two phi nodes
			if (!dyn_cast<PHINode>(&i))
				debug_insts.insert(
					std::make_pair(last_inst, &i));
			last_inst = nullptr;
		}
		if (sz == 0 || sz > 64) {
			continue;
		}
		last_inst = &i;
	}
	}

	auto i64ty = IntegerType::get(getGlobalContext(), 64);
	auto i8ty = IntegerType::get(getGlobalContext(), 8);
	auto fptr_ty = PointerType::get(debug_func_ty, 0);
	auto str_ty = PointerType::get(i8ty, 0);

	for (auto& p : debug_insts)  {
		auto			i_print = p.first;
		auto			i_after = p.second;
		IRBuilder<>		builder(i_after);
		std::string		o;
		raw_string_ostream	ss(o);

		ss << f.getName().str() << ':';
		i_print->print(ss);

		// XXX: leaky
		auto instr_str = ss.str();
		char *jit_str = strdup(instr_str.c_str());

		/* call this */
		auto fptr = builder.CreateIntToPtr(
				ConstantInt::get(i64ty, (uint64_t)&debug_print),
				fptr_ty);

		/* pass in this string */
		auto sint = builder.CreateIntToPtr(
			ConstantInt::get(i64ty, (uint64_t)jit_str), str_ty);

		/* pass in this value */
		unsigned sz = i_print->getType()->getPrimitiveSizeInBits();
		Value	*v64 = (sz == 64) ? i_print : nullptr;

		if (!v64) {
			auto ity = IntegerType::get(getGlobalContext(), sz);
			v64 = i_print;
			if (v64->getType()->isVectorTy()) {
				v64 = builder.CreateBitCast(v64, ity);
			} else if (v64->getType()->isFloatTy()) {
				v64 = builder.CreateBitCast(v64, ity);
			}
			v64 = builder.CreateZExt(v64, i64ty);
		}

		if (v64->getType() != i64ty) {
			v64 = builder.CreateBitCast(v64, i64ty);
		}

		std::vector<Value*>	args(2);
		args[0] = sint;
		args[1] = v64;
		builder.CreateCall(fptr, args);
	}

	return !debug_insts.empty();
}

FunctionType* DebugPrintPass::getFuncType(void) const
{
	std::vector<Type*>	f_args;
	// u8* = str
	f_args.push_back(PointerType::get(
		IntegerType::get(getGlobalContext(), 8),
		0));
	// 64-bit value from instruction computation
	f_args.push_back(IntegerType::get(getGlobalContext(), 64));

	// void debug_print(u8*, u64)
	return FunctionType::get(
		Type::getVoidTy(getGlobalContext()),
		f_args,
		false);
}
