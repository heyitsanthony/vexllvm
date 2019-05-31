/******************************************
 * codegen macros used in vexop*.cc files *
 ******************************************/
#ifndef VEXOP_MACROS_H
#define VEXOP_MACROS_H

#include <assert.h>

#define NONE
#define get_i(b)	IntegerType::get(getGlobalContext(), b)
#define get_f()		Type::getFloatTy(getGlobalContext())
#define get_d()		Type::getDoubleTy(getGlobalContext())
#define get_c(b, v)	ConstantInt::get(getGlobalContext(), APInt(b, v))
#define get_32i(v)	get_c(32, v)
#define get_vt(w, b)	VectorType::get(get_i(b), w)
#define get_vtf(w)	VectorType::get(builder->getFloatTy(), w)
#define get_vtd(w)	VectorType::get(builder->getDoubleTy(), w)
#define get_cvl(var, l) 						\
	ConstantVector::get(std::vector<Constant*>(			\
		var, var + l))
#define get_cv(var) get_cvl(var, sizeof(var)/sizeof(Constant*))

#define UNOP_SETUP				\
	Value		*v1;			\
	IRBuilder<>	*builder(theGenLLVM->getBuilder());	\
	v1 = args[0]->emit();			\
	(void)v1;

#define BINOP_SETUP				\
	Value		*v1, *v2;		\
	IRBuilder<>	*builder(theGenLLVM->getBuilder()); \
	v1 = args[0]->emit();			\
	v2 = args[1]->emit();			\
	(void)v1;				\
	(void)v2;

#define TRIOP_SETUP				\
	Value		*v1, *v2, *v3;		\
	IRBuilder<>	*builder(theGenLLVM->getBuilder());	\
	v1 = args[0]->emit();			\
	v2 = args[1]->emit();			\
	v3 = args[2]->emit();			\
	(void)v1;				\
	(void)v2;				\
	(void)v3;

#define QOP_SETUP				\
	Value		*v1, *v2, *v3, *v4;	\
	IRBuilder<>	*builder(theGenLLVM->getBuilder());	\
	v1 = args[0]->emit();			\
	v2 = args[1]->emit();			\
	v3 = args[2]->emit();			\
	v4 = args[3]->emit();


#define X_TO_Y_EMIT(x,y,w,z)					\
Value* VexExprUnop##x::emit(void) const				\
{								\
	UNOP_SETUP						\
	unsigned bits = v1->getType()->getPrimitiveSizeInBits();\
	if(bits < w->getPrimitiveSizeInBits()) {		\
		fprintf(stderr, "size mismatch in IR: wanted %d got %d\n", \
		 	w->getPrimitiveSizeInBits(), bits);	\
		v1 = builder->CreateBitCast(v1, get_i(bits));	\
		v1 = builder->CreateZExt(v1, w);		\
	} else if (bits > w->getPrimitiveSizeInBits()) {	\
		fprintf(stderr, "size mismatch in IR: wanted %d got %d\n", \
		 	w->getPrimitiveSizeInBits(), bits);	\
		v1 = builder->CreateBitCast(v1, get_i(bits));	\
		v1 = builder->CreateTrunc(v1, w);		\
	}							\
	v1 = builder->CreateBitCast(v1, w);			\
	return builder->y(v1, z);				\
}

/* XXX probably wrong to discard rounding info, but who cares */
#define X_TO_Y_EMIT_ROUND(x,y,w,z)		\
Value* VexExprBinop##x::emit(void) const	\
{						\
	BINOP_SETUP				\
	v2 = builder->CreateBitCast(v2, w);	\
	return builder->y(v2, z);		\
}

// (name, cast type, op)
#define BINCMP_EMIT(x,y,z)					\
Value* VexExprBinop##x::emit(void) const			\
{								\
	BINOP_SETUP						\
	v1 = builder->CreateBitCast(v1, y);			\
	v2 = builder->CreateBitCast(v2, y);			\
	Value* result = builder->CreateBitCast(			\
		get_c(y->getBitWidth(), 0), y);			\
	for(unsigned i = 0; i < y->getNumElements(); ++i) {	\
		Value* e1 = builder->CreateExtractElement(	\
			v1, get_32i(i));			\
		Value* e2 = builder->CreateExtractElement(	\
			v2, get_32i(i));			\
		Value* elem = builder->CreateSelect(		\
			builder->Create##z(e1, e2),		\
			e1, e2);				\
		result = builder->CreateInsertElement(		\
			result, elem, get_32i(i));		\
	}							\
	return result;						\
}

/* (vex op, cast-to type, LLVM instruction)*/
#define OPF0X_EMIT(x, y, z)	OPF0X_EMIT_BEGIN(x,y,z);	\
				OPF0X_EMIT_OP(x,y,z);		\
				OPF0X_EMIT_END(x,y,z)


#define OPF0X_EMIT_BEGIN(x, y, z)			\
Value* VexExprBinop##x::emit(void) const		\
{	\
	Value	*lo_op_lhs, *lo_op_rhs, *result;	\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateBitCast(v2, y);		\
	lo_op_lhs = builder->CreateExtractElement(v1, get_32i(0));	\
	lo_op_rhs = builder->CreateExtractElement(v2, get_32i(0));

#define OPF0X_EMIT_BEGIN_AB(x, y, z)	\
	OPF0X_EMIT_BEGIN(x,y,z)		\
	Value	*a, *b;			\
	a = lo_op_lhs;			\
	b = lo_op_rhs;

#define OPF0X_EMIT_OP(x,y,z)	\
	result = builder->Create##z(lo_op_lhs, lo_op_rhs); \

#define OPF0X_EMIT_END(x,y,z)						\
	return builder->CreateInsertElement(v1, result, get_32i(0));	\
}

#define OPF0X_CMP_EMIT(x, y, z)						\
	OPF0X_EMIT_BEGIN_AB(x,y,z)					\
	result = builder->Create##z(a, b);				\
	result = builder->CreateSExt(result, 				\
		get_i(y->getScalarType()->getPrimitiveSizeInBits()));	\
	result = builder->CreateBitCast(result, y->getScalarType());	\
	OPF0X_EMIT_END(x,y,z)

#define OPF0X_SEL_EMIT(x, y, z)						\
	OPF0X_EMIT_BEGIN_AB(x,y,z)					\
	result = builder->CreateSelect(builder->Create##z(a, b), a, b);	\
	OPF0X_EMIT_END(x,y,z)

#define OPF0X_RSQ(x, y, z, a, b)					\
Value* VexExprUnop##x::emit(void) const					\
{									\
	Value		*a1;						\
	UNOP_SETUP							\
	v1 = builder->CreateBitCast(v1, y);				\
	a1 = builder->CreateExtractElement(v1, get_32i(0));		\
	a								\
	b								\
	return builder->CreateInsertElement(v1, a1, get_32i(0));	\
}

#define F0XINT(y,z)							\
	Function	*f;						\
	std::vector<llvm::Type*> call_args;				\
	call_args.push_back(y->getScalarType());			\
	f = Intrinsic::getDeclaration(					\
		&theGenLLVM->getModule(),				\
		Intrinsic::z, ArrayRef<Type*>(call_args));		\
	assert (f != NULL);						\
	a1 = builder->CreateCall(f, a1);

#define F0XRCP(o) a1 = builder->CreateFDiv(o, a1);

#define EMIT_HELPER_UNOP(x,y)			\
Value* VexExprUnop##x::emit(void) const	\
{						\
	IRBuilder<>     *builder = theGenLLVM->getBuilder();	\
	llvm::Function	*f;	\
	llvm::Value	*v;	\
	v = args[0]->emit();	\
	f = theVexHelpers->getCallHelper(y);	\
	assert (f != NULL);	\
	return builder->CreateCall(f, v);	\
}

#define EMIT_HELPER_BINOP(x,y,z)		\
Value* VexExprBinop##x::emit(void) const	\
{						\
	llvm::Function	*f;			\
	BINOP_SETUP				\
	f = theVexHelpers->getCallHelper(y);	\
	assert (f != NULL);			\
	std::vector<Value*>	v_(2);		\
	v_[0] = builder->CreateBitCast(v1, z);	\
	v_[1] = builder->CreateBitCast(v2, z);	\
	return builder->CreateCall(f, v_);	\
}

#define OPV_CMP_T_EMIT(x, y, z, w)			\
Value* VexExprBinop##x::emit(void) const		\
{							\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateBitCast(v2, y);		\
	return builder->CreateSExt(			\
		builder->Create##z(v1, v2),		\
		w);					\
}

#define OPV_CMP_EMIT(x, y, z)	OPV_CMP_T_EMIT(x, y, z, y)

#define DBL_0 ConstantFP::get(get_d(), 0.0)
#define FLT_0 ConstantFP::get(get_f(), 0.0)
#define DBL_1 ConstantFP::get(get_d(), 1.0)
#define FLT_1 ConstantFP::get(get_f(), 1.0)

#define UNOP_EMIT(x,y)				\
Value* VexExprUnop##x::emit(void) const	\
{						\
	UNOP_SETUP				\
	return builder->y(v1);			\
}

#define XHI_TO_Y_EMIT(x,y,w,z)				\
Value* VexExprUnop##x::emit(void) const			\
{							\
	UNOP_SETUP					\
	v1 = builder->CreateBitCast(v1, w);		\
	return builder->CreateTrunc(			\
		builder->CreateLShr(v1, get_c(y*2,y)),	\
		z);					\
}							\

#define OPV_EMIT(x, y, z)			\
Value* VexExprBinop##x::emit(void) const	\
{	\
	BINOP_SETUP					\
	v1 = builder->CreateBitCast(v1, y);		\
	v2 = builder->CreateBitCast(v2, y);		\
	return builder->Create##z(v1, v2);		\
}

#define UNOP_UNIMPL(x)	\
Value* VexExprUnop##x::emit(void) const { assert (0 == 1 && "UNIMPL"); }
#define BINOP_UNIMPL(x)	\
Value* VexExprBinop##x::emit(void) const { assert (0 == 1 && "UNIMPL"); }
#define TRIOP_UNIMPL(x)	\
Value* VexExprTriop##x::emit(void) const { assert (0 == 1 && "UNIMPL"); }
#define QOP_UNIMPL(x)	\
Value* VexExprQop##x::emit(void) const { assert (0 == 1 && "UNIMPL"); }


#endif
