#include <llvm/Value.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "llvm/Target/TargetSelect.h"
#include "llvm/ExecutionEngine/JIT.h"

#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vexstmt.h"
#include "vexexpr.h"
#include "vexsb.h"
#include "vexhelpers.h"
#include "genllvm.h"
#include "guestcpustate.h"
#include "gueststate.h"

using namespace llvm;

#define TEST_VSB_GUESTADDR	((void*)0x12345678)

static ExecutionEngine	*exeEngine;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::endl;
	std::cerr << "(NO DUMPING IN TEST JIT)" << std::endl;
}

typedef uint64_t(*vexfunc_t)(void* /* guest cpu state */);

uint64_t doFunc(GuestState* gs, Function* f)
{
	/* don't forget to run it! */
	vexfunc_t func_ptr;
	
	func_ptr = (vexfunc_t)exeEngine->getPointerToFunction(f);
	assert (func_ptr != NULL && "Could not JIT");

	return func_ptr(gs->getCPUState()->getStateData());
}

class GuestStateIdent : public GuestState
{
public:
	GuestStateIdent() {}
	virtual ~GuestStateIdent() {}
	Value* addrVal2Host(Value* v) const { return v; }
	uint64_t addr2Host(guestptr_t p) const { return p; }
	guestptr_t name2guest(std::string& s) const { return 0; }
	void* getEntryPoint(void) const { return NULL; }
};

Function* getTestFunction(const char* name)
{
	VexSB			*vsb;
	Function		*f;
	std::vector<VexStmt*>	stmts;
	VexStmt*		stmt;
	VexExpr**		args;
	VexExpr*		next_expr;

	vsb = new VexSB((uint64_t)TEST_VSB_GUESTADDR, 10 /* whatever */);

	stmts.push_back(
		new VexStmtIMark(vsb, (uint64_t)TEST_VSB_GUESTADDR, 4));

	args = new VexExpr*[1];
	args[0] = new VexExprConstU32(NULL, 0xcafebabe);
	stmts.push_back(
		new VexStmtPut(vsb, 208, 
			new VexExprUnop32UtoV128(NULL, args)));

	next_expr = new VexExprConstU64(NULL, 0xdeadbeef);
	vsb->load(stmts, Ijk_Ret, next_expr);

	f = vsb->emit(name);
	f->dump();
	delete vsb;

	return f;
}

int main(int argc, char* argv[])
{
	GuestState*	gs;
	Function*	f;
	void*		guest_ptr;

	/* for the JIT */
	InitializeNativeTarget();

	gs = new GuestStateIdent();
	theGenLLVM = new GenLLVM(gs);
	theVexHelpers = new VexHelpers();

	EngineBuilder	eb(theGenLLVM->getModule());
	std::string	err_str;

	eb.setErrorStr(&err_str);
	exeEngine = eb.create();
	assert (exeEngine && "Could not make exe engine");

	theVexHelpers->bindToExeEngine(exeEngine);

	char emitstr[1024];
	guest_ptr = TEST_VSB_GUESTADDR;
	sprintf(emitstr, "sb_%p", guest_ptr);
	f = getTestFunction(emitstr);
	assert (f && "FAILED TO EMIT FUNC??");

	doFunc(gs, f);
	gs->print(std::cerr);

	delete theVexHelpers;
	delete theGenLLVM;

	return 0;
}