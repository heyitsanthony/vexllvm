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

extern "C"
{
#include "valgrind/libvex_guest_amd64.h"	/* for register offsets */
}

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

/* XXX use offsetof references to valgrind */
#define GUEST_BYTEOFF_RDX	offsetof(VexGuestAMD64State, guest_RDX)
#define GUEST_BYTEOFF_XMM0	offsetof(VexGuestAMD64State, guest_XMM0)
#define GUEST_BYTEOFF_XMM1	offsetof(VexGuestAMD64State, guest_XMM1)
#define GUEST_BYTEOFF_XMM2	offsetof(VexGuestAMD64State, guest_XMM2)
#define GUEST_BYTEOFF_XMM3	offsetof(VexGuestAMD64State, guest_XMM3)

class TestSB
{
public:
	TestSB() {}
	virtual ~TestSB() {}
	virtual VexSB* getSB(void) const
	{
		VexSB			*vsb;
		std::vector<VexStmt*>	stmts;
		VexExpr*		next_expr;

		vsb = new VexSB((uint64_t)TEST_VSB_GUESTADDR, 10 /* whatever */);
		stmts.push_back(
			new VexStmtIMark(vsb, (uint64_t)TEST_VSB_GUESTADDR, 4));
		setupStmts(vsb, stmts);
		next_expr = new VexExprConstU64(NULL, 0xdeadbeef);

		/* feed statemenet vector into vexsb in lieu of proper irsb */
		vsb->load(stmts, Ijk_Ret, next_expr);

		return vsb;
	}

	virtual void prepState(GuestState* gs) const
	{
		memset(	gs->getCPUState()->getStateData(),
			0,
			gs->getCPUState()->getStateSize());
	}

	virtual bool isGoodState(GuestState* gs) const = 0;
protected:
	
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>&) const = 0;
private:
};

class TestV128to64 : public TestSB
{
public:
	TestV128to64() {}
	virtual ~TestV128to64() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st;
		st = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
		return st->guest_RDX == 0x1234123412341234;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstV128(NULL, 0x1234);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_RDX, 
				new VexExprUnopV128to64(NULL, args)));
	}
};

class TestInterleaveLO8x16 : public TestSB
{
public:
	TestInterleaveLO8x16 () {}
	virtual ~TestInterleaveLO8x16() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st;
		st = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
		return *((uint64_t*)&st->guest_XMM1) == 0xab12cd34ab12cd34;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x1234);
		args[1] = new VexExprConstV128(NULL, 0xabcd);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprBinopInterleaveLO8x16(
					NULL, args)));
	}
};

void doTest(GuestState* gs, TestSB* tsb)
{
	Function*	f;
	VexSB		*vsb;
	void*		guest_ptr;
	char		emitstr[1024];

	guest_ptr = TEST_VSB_GUESTADDR;
	sprintf(emitstr, "sb_%p", guest_ptr);

	vsb = tsb->getSB();
	f = vsb->emit(emitstr);
	f->dump();
	delete vsb;

	assert (f && "FAILED TO EMIT FUNC??");

	tsb->prepState(gs);
	doFunc(gs, f);
	if (!tsb->isGoodState(gs)) {
		gs->print(std::cerr);
		assert (tsb->isGoodState(gs));
	}
	delete tsb;
}

int main(int argc, char* argv[])
{
	GuestState*	gs;


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
	
	doTest(gs, new TestV128to64());
	doTest(gs, new TestInterleaveLO8x16());


	delete theVexHelpers;
	delete theGenLLVM;

	return 0;
}