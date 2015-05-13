#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include "Sugar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#include "jitengine.h"
#include "vexstmt.h"
#include "vexexpr.h"
#include "vexsb.h"
#include "vexhelpers.h"
#include "genllvm.h"
#include "guestcpustate.h"
#include "guestabi.h"
#include "guest.h"

extern "C"
{
#include "valgrind/libvex_guest_amd64.h"	/* for register offsets */
}

using namespace llvm;

#define TEST_VSB_GUESTADDR	guest_ptr(0x12345678)

static std::unique_ptr<JITEngine> jitEngine;

void dumpIRSBs(void)
{
	std::cerr << "DUMPING LOGS" << std::endl;
	std::cerr << "(NO DUMPING IN TEST JIT)" << std::endl;
}

typedef uint64_t(*vexfunc_t)(void* /* guest cpu state */);

uint64_t doFunc(Guest& gs, Function* f)
{
	/* don't forget to run it! */
	vexfunc_t func_ptr;
	
	func_ptr = (vexfunc_t)jitEngine->getPointerToFunction(
		f, theGenLLVM->takeModule());
	assert (func_ptr != NULL && "Could not JIT");

	return func_ptr(gs.getCPUState()->getStateData());
}

class GuestIdent : public Guest
{
public:
	GuestIdent() : Guest("ident")
	{
		cpu_state = GuestCPUState::create(Arch::X86_64);
		mem = new GuestMem();
		abi = GuestABI::create(this);
	}
	virtual ~GuestIdent() {}
	guest_ptr getEntryPoint(void) const { return guest_ptr(0); }
	virtual Arch::Arch getArch() const { return Arch::getHostArch(); }
};

/* XXX use offsetof references to valgrind */
#define GUEST_BYTEOFF_RDX	offsetof(VexGuestAMD64State, guest_RDX)
#define GUEST_BYTEOFF_YMM0	offsetof(VexGuestAMD64State, guest_YMM0)
#define GUEST_BYTEOFF_YMM1	offsetof(VexGuestAMD64State, guest_YMM1)
#define GUEST_BYTEOFF_YMM2	offsetof(VexGuestAMD64State, guest_YMM2)
#define GUEST_BYTEOFF_YMM3	offsetof(VexGuestAMD64State, guest_YMM3)

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

		vsb = new VexSB(TEST_VSB_GUESTADDR, 0, NULL);
		stmts.push_back(
			new VexStmtIMark(vsb, TEST_VSB_GUESTADDR, 4));
		setupStmts(vsb, stmts);
		next_expr = new VexExprConstU64(NULL, 0xdeadbeef);

		/* feed statemenet vector into vexsb in lieu of proper irsb */
		vsb->load(stmts, Ijk_Ret, next_expr);

		return vsb;
	}

	virtual void prepState(Guest& gs) const
	{
		memset(	gs.getCPUState()->getStateData(),
			0,
			gs.getCPUState()->getStateSize());

		getVexState(&gs)->guest_DFLAG = 1;
	}

	virtual bool isGoodState(Guest& gs) const = 0;
	const std::string& getName(void) const { return test_name; }
protected:
	VexGuestAMD64State* getVexState(Guest* gs) const
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
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return st->guest_RDX == 0xffff;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstV128(NULL, 0x1003);
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
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return *((uint64_t*)&st->guest_YMM1) == 0xffff;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x0001);
		args[1] = new VexExprConstV128(NULL, 0x1003);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinopOrV128(NULL, args)));
	}
};


class TestAndV128 : public TestSB
{
public:
	TestAndV128() : TestSB("AndV128") {}
	virtual ~TestAndV128() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return *((uint64_t*)&st->guest_YMM1) == 0xff;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x1001);
		args[1] = new VexExprConstV128(NULL, 0x1013);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinopAndV128(NULL, args)));
	}
};


class TestCmpGT8Sx16 : public TestSB
{
public:
	TestCmpGT8Sx16() : TestSB("CmpGT8Sx16") {}
	virtual ~TestCmpGT8Sx16() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return *((uint64_t*)&st->guest_YMM1) == 0xff00;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x1111);
		args[1] = new VexExprConstV128(NULL, 0x0003);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinopCmpGT8Sx16(NULL, args)));
	}
};
class TestInterleaveLO64x2 : public TestSB
{
public:
	TestInterleaveLO64x2 () : TestSB("ILO64x2") {}
	virtual ~TestInterleaveLO64x2() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return	*((uint64_t*)&st->guest_YMM1+1) == 0xff &&
			*((uint64_t*)&st->guest_YMM1) == 0xff00;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x1);
		args[1] = new VexExprConstV128(NULL, 0x2);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinopInterleaveLO64x2(
					NULL, args)));
	}
};


class TestInterleaveLO8x16 : public TestSB
{
public:
	TestInterleaveLO8x16 () : TestSB("ILO8x16") {}
	virtual ~TestInterleaveLO8x16() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return *((uint64_t*)&st->guest_YMM1) == 0xff0000ff00;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x0001);
		args[1] = new VexExprConstV128(NULL, 0x0004);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinopInterleaveLO8x16(
					NULL, args)));
	}
};

class TestSub8x16 : public TestSB
{
public:
	TestSub8x16() : TestSB("sub8x16") {}
	virtual ~TestSub8x16() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return *((uint64_t*)&st->guest_YMM1) == 0xff0000;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x5);
		args[1] = new VexExprConstV128(NULL, 0x1);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinopSub8x16(NULL, args)));
	}
};

class Test64HLtoV128 : public TestSB
{
public:
	Test64HLtoV128() : TestSB("64HLtoV128") {}
	virtual ~Test64HLtoV128() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return (((uint64_t*)&st->guest_YMM1)[1] == 0x12345678abcdef01) && 
			(((uint64_t*)&st->guest_YMM1)[0] == 0x11);
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstU64(NULL, 0x12345678abcdef01);
		args[1] = new VexExprConstU64(NULL, 0x11);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM1, 
				new VexExprBinop64HLtoV128(NULL, args)));
	}
};

class Test32UtoV128 : public TestSB
{
public:
	Test32UtoV128() : TestSB("32UtoV128") {}
	virtual ~Test32UtoV128() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return ((uint64_t*)&st->guest_YMM1)[0] == 0x12345678;
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
				GUEST_BYTEOFF_YMM1, 
				new VexExprUnop32UtoV128(NULL, args)));
	}
};

class Test32HLto64 : public TestSB
{
public:
	Test32HLto64() : TestSB("32HLto64") {}
	virtual ~Test32HLto64() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return st->guest_RDX == 0x1234567812345678;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstU32(NULL, 0x12345678);
		args[1] = new VexExprConstU32(NULL, 0x12345678);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_RDX, 
				new VexExprBinop32HLto64(NULL, args)));
	}
};

class Test64HIto32 : public TestSB
{
public:
	Test64HIto32() : TestSB("64HIto32") {}
	virtual ~Test64HIto32() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return st->guest_RDX == 0xffeeddcc;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstU64(NULL, 0xffeeddcc12345678);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_RDX, 
				new VexExprUnop64HIto32(NULL, args)));
	}
};

class Test128HIto64 : public TestSB
{
public:
	Test128HIto64() : TestSB("128HIto64") {}
	virtual ~Test128HIto64() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return st->guest_RDX == 0xffffffff00000000;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[1];
		args[0] = new VexExprConstV128(NULL, 0xf001);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_RDX, 
				new VexExprUnop128HIto64(NULL, args)));
	}
};

class TestDivModU64to32 : public TestSB
{
public:
	TestDivModU64to32() : TestSB("DivModU64to32") {}
	virtual ~TestDivModU64to32() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		return ((st->guest_RDX & 0xffffffff) == (0xf63d4e2e/1011)) &&
			((st->guest_RDX >> 32) == (0xf63d4e2e % 1011));

	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstU64(NULL, 0xf63d4e2e);
		args[1] = new VexExprConstU32(NULL, 1011);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_RDX, 
				new VexExprBinopDivModU64to32(NULL, args)));
	}
};

class TestDivModU128to64 : public TestSB
{
public:
	TestDivModU128to64() : TestSB("DivModU128to64") {}
	virtual ~TestDivModU128to64() {}
	virtual bool isGoodState(Guest& gs) const
	{
		VexGuestAMD64State	*st = getVexState(&gs);
		uint64_t		*xmm = (uint64_t*)&st->guest_YMM0;
		return xmm[0] == 0x1fe00000000 && xmm[1] == 0 /* no rem */;
	}
protected:
	virtual void setupStmts(VexSB* vsb, std::vector<VexStmt*>& stmts) const 
	{
		VexExpr**		args;
		args = new VexExpr*[2];
		args[0] = new VexExprConstV128(NULL, 0x1000);
		args[1] = new VexExprConstU64(NULL, 0x8000000000000000);
		stmts.push_back(
			new VexStmtPut(
				vsb,
				GUEST_BYTEOFF_YMM0, 
				new VexExprBinopDivModU128to64(NULL, args)));
	}
};


void doTest(Guest& gs, TestSB* tsb)
{
	Function*	f;
	VexSB		*vsb;
	guest_ptr	guest;
	char		emitstr[1024];

	guest = TEST_VSB_GUESTADDR;
	sprintf(emitstr, "sb_%p", (void*)guest.o);

	vsb = tsb->getSB();
	f = vsb->emit(emitstr);
	std::cerr << "TESTING: " << tsb->getName() << std::endl;
	f->dump();
	delete vsb;

	assert (f && "FAILED TO EMIT FUNC??");

	tsb->prepState(gs);
	doFunc(gs, f);
	if (!tsb->isGoodState(gs)) {
		gs.print(std::cerr);
		assert (tsb->isGoodState(gs));
	}
	gs.print(std::cerr);
	delete tsb;
}

int main(int argc, char* argv[])
{
	GuestIdent	gs;

	jitEngine = std::make_unique<JITEngine>();
	theGenLLVM = std::make_unique<GenLLVM>(gs);
	theVexHelpers = VexHelpers::create(Arch::getHostArch());

#if 1
	theVexHelpers->moveToJITEngine(*jitEngine);
	doTest(gs, new TestV128to64());
	doTest(gs, new TestCmpGT8Sx16());
	doTest(gs, new TestAndV128());
	doTest(gs, new TestOrV128());
	doTest(gs, new TestInterleaveLO8x16());
	doTest(gs, new TestInterleaveLO64x2());
	doTest(gs, new TestSub8x16());
	doTest(gs, new Test64HLtoV128());
	doTest(gs, new Test32HLto64());
	doTest(gs, new Test64HIto32());
	doTest(gs, new Test128HIto64());
	doTest(gs, new Test32UtoV128());
	doTest(gs, new TestDivModU128to64 ());
	doTest(gs, new TestDivModU64to32());

	std::cout << "JIT OK." << std::endl;
#endif
	return 0;
}
