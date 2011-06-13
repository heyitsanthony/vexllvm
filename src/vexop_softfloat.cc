#include <llvm/Intrinsics.h>
#include "genllvm.h"
#include "vexop.h"
#include "vexop_macros.h"
#include "vexhelpers.h"

using namespace llvm;

void vexop_setup_fp(VexHelpers* vh)
{
	vh->loadUserMod("softfloat.bc");
}

BINOP_UNIMPL(Add32F0x4)
BINOP_UNIMPL(Add32Fx2)
BINOP_UNIMPL(Add32Fx4)
BINOP_UNIMPL(Add64F0x2)
BINOP_UNIMPL(Add64Fx2)
BINOP_UNIMPL(CmpEQ32F0x4)
BINOP_UNIMPL(CmpEQ32Fx2)
BINOP_UNIMPL(CmpEQ32Fx4)
BINOP_UNIMPL(CmpEQ64F0x2)
BINOP_UNIMPL(CmpEQ64Fx2)
BINOP_UNIMPL(CmpF64)
BINOP_UNIMPL(CmpGE32Fx2)
BINOP_UNIMPL(CmpGE32Fx4)
BINOP_UNIMPL(CmpGT32Fx2)
BINOP_UNIMPL(CmpGT32Fx4)
BINOP_UNIMPL(CmpLE32F0x4)
BINOP_UNIMPL(CmpLE32Fx4)
BINOP_UNIMPL(CmpLE64F0x2)
BINOP_UNIMPL(CmpLE64Fx2)
BINOP_UNIMPL(CmpLT32F0x4)
BINOP_UNIMPL(CmpLT32Fx4)
BINOP_UNIMPL(CmpLT64F0x2)
BINOP_UNIMPL(CmpLT64Fx2)
BINOP_UNIMPL(CmpUN32F0x4)
BINOP_UNIMPL(CmpUN32Fx4)
BINOP_UNIMPL(CmpUN64F0x2)
BINOP_UNIMPL(CmpUN64Fx2)
BINOP_UNIMPL(Div32F0x4)
BINOP_UNIMPL(Div32Fx4)
BINOP_UNIMPL(Div64F0x2)
BINOP_UNIMPL(Div64Fx2)
BINOP_UNIMPL(F64toF32)
BINOP_UNIMPL(F64toI32S)
BINOP_UNIMPL(F64toI32U)
BINOP_UNIMPL(F64toI64S)
BINOP_UNIMPL(I64StoF64)
BINOP_UNIMPL(Max32F0x4)
BINOP_UNIMPL(Max32Fx2)
BINOP_UNIMPL(Max32Fx4)
BINOP_UNIMPL(Max64F0x2)
BINOP_UNIMPL(Max64Fx2)
BINOP_UNIMPL(Min32F0x4)
BINOP_UNIMPL(Min32Fx2)
BINOP_UNIMPL(Min32Fx4)
BINOP_UNIMPL(Min64F0x2)
BINOP_UNIMPL(Min64Fx2)
BINOP_UNIMPL(Mul32F0x4)
BINOP_UNIMPL(Mul32Fx2)
BINOP_UNIMPL(Mul32Fx4)
BINOP_UNIMPL(Mul64F0x2)
BINOP_UNIMPL(Mul64Fx2)
BINOP_UNIMPL(RoundF32toInt)
BINOP_UNIMPL(RoundF64toInt)
BINOP_UNIMPL(Sub32F0x4)
BINOP_UNIMPL(Sub32Fx2)
BINOP_UNIMPL(Sub32Fx4)
BINOP_UNIMPL(Sub64F0x2)
BINOP_UNIMPL(Sub64Fx2)
TRIOP_UNIMPL(AddF32)
TRIOP_UNIMPL(AddF64)
TRIOP_UNIMPL(DivF32)
TRIOP_UNIMPL(DivF64)
TRIOP_UNIMPL(MulF32)
TRIOP_UNIMPL(MulF64)
TRIOP_UNIMPL(PRemC3210F64)
TRIOP_UNIMPL(PRemF64)
TRIOP_UNIMPL(SubF32)
TRIOP_UNIMPL(SubF64)
UNOP_UNIMPL(F32toF64)
UNOP_UNIMPL(I32StoF64)
UNOP_UNIMPL(NegF32)
UNOP_UNIMPL(NegF64)
UNOP_UNIMPL(RSqrt32F0x4)
UNOP_UNIMPL(RSqrt32Fx4)
UNOP_UNIMPL(RSqrt64F0x2)
UNOP_UNIMPL(RSqrt64Fx2)
UNOP_UNIMPL(Recip32F0x4)
UNOP_UNIMPL(Recip32Fx4)
UNOP_UNIMPL(Recip64F0x2)
UNOP_UNIMPL(Recip64Fx2)
UNOP_UNIMPL(Sqrt32F0x4)
UNOP_UNIMPL(Sqrt32Fx4)
UNOP_UNIMPL(Sqrt64F0x2)
UNOP_UNIMPL(Sqrt64Fx2)
