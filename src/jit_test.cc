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
	guestptr_t name2guest(const char*) const { return 0; }
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
	TestSB(const std::string& s) : test_name(s) {}
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
	const std::string& getName(void) const { return test_name; }
protected:
	VexGuestAMD64State* getVexState(GuestState* gs) const
	{
		return (VexGuestAMD64State*)gs->getCPUState()->getStateData();
	}

	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>&) const = 0;
private:
	std::string	test_name;
};

class TestV128to64 : public TestSB
{
public:
	TestV128to64() : TestSB("V128to64") {}
	virtual ~TestV128to64() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st = getVexState(gs);
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

class TestOrV128 : public TestSB
{
public:
	TestOrV128() : TestSB("OrV128") {}
	virtual ~TestOrV128() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st = getVexState(gs);
		return *((uint64_t*)&st->guest_XMM1) == 0x3333333333333333;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x1111);
		args[1] = new VexExprConstV128(NULL, 0x2222);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprBinopOrV128(NULL, args)));
	}
};


class TestAndV128 : public TestSB
{
public:
	TestAndV128() : TestSB("AndV128") {}
	virtual ~TestAndV128() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st = getVexState(gs);
		return *((uint64_t*)&st->guest_XMM1) == 0x1111111111111111;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x3333);
		args[1] = new VexExprConstV128(NULL, 0x5555);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprBinopAndV128(NULL, args)));
	}
};


class TestCmpGT8Sx16 : public TestSB
{
public:
	TestCmpGT8Sx16() : TestSB("CmpGT8Sx16") {}
	virtual ~TestCmpGT8Sx16() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st = getVexState(gs);
		return *((uint64_t*)&st->guest_XMM1) == 0x00ff00ff00ff00ff;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x3360);
		args[1] = new VexExprConstV128(NULL, 0x5455);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprBinopCmpGT8Sx16(NULL, args)));
	}
};

class TestInterleaveLO8x16 : public TestSB
{
public:
	TestInterleaveLO8x16 () : TestSB("ILO8x16") {}
	virtual ~TestInterleaveLO8x16() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st = getVexState(gs);
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

class TestSub8x16 : public TestSB
{
public:
	TestSub8x16() : TestSB("sub8x16") {}
	virtual ~TestSub8x16() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st = getVexState(gs);
		return *((uint64_t*)&st->guest_XMM1) == 0x4411441144114411;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x5533);
		args[1] = new VexExprConstV128(NULL, 0x1122);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprBinopSub8x16(NULL, args)));
	}
};

class Test64HLtoV128 : public TestSB
{
public:
	Test64HLtoV128() : TestSB("64HLtoV128") {}
	virtual ~Test64HLtoV128() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st;
		st = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
		return ((uint64_t*)&st->guest_XMM1)[0] == 0x12345678abcdef01;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstU64(NULL, 0x12345678abcdef01);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprUnop64HLtoV128(NULL, args)));
	}
};

class Test32UtoV128 : public TestSB
{
public:
	Test32UtoV128() : TestSB("32UtoV128") {}
	virtual ~Test32UtoV128() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st;
		st = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
		return ((uint64_t*)&st->guest_XMM1)[0] == 0x12345678;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstU32(NULL, 0x12345678);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_XMM1, 
				new VexExprUnop32UtoV128(NULL, args)));
	}
};

class Test32HLto64 : public TestSB
{
public:
	Test32HLto64() : TestSB("32HLto64") {}
	virtual ~Test32HLto64() {}
	virtual bool isGoodState(GuestState* gs) const
	{
		VexGuestAMD64State	*st;
		st = (VexGuestAMD64State*)gs->getCPUState()->getStateData();
		return st->guest_RDX == 0x1234567812345678;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstU32(NULL, 0x12345678);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_RDX, 
				new VexExprUnop32HLto64(NULL, args)));
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
	std::cerr << "TESTING: " << tsb->getName() << std::endl;
	f->dump();
	delete vsb;

	assert (f && "FAILED TO EMIT FUNC??");

	tsb->prepState(gs);
	doFunc(gs, f);
	if (!tsb->isGoodState(gs)) {
		gs->print(std::cerr);
		assert (tsb->isGoodState(gs));
	}
	gs->print(std::cerr);
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
	
	doTest(gs, new TestCmpGT8Sx16());
	doTest(gs, new TestAndV128());
	doTest(gs, new TestOrV128());
	doTest(gs, new TestV128to64());
	doTest(gs, new TestInterleaveLO8x16());
	doTest(gs, new TestSub8x16());
	doTest(gs, new Test64HLtoV128());
	doTest(gs, new Test32HLto64());
	doTest(gs, new Test32UtoV128());

	delete theVexHelpers;
	delete theGenLLVM;

	std::cout << "JIT OK." << std::endl;

	return 0;
}