#include "genllvm.h"
#include "memlog.h"
#include <iostream>

using namespace llvm;

const StructType*	MemLog::type = NULL;
const PointerType*	MemLog::addr_type = NULL;
const IntegerType*	MemLog::size_type = NULL;
const VectorType*	MemLog::data_type = NULL;

const StructType* MemLog::getType(void)
{
	if(type)
		return type;
	LLVMContext& gctx(getGlobalContext());

	addr_type = PointerType::get(
		IntegerType::get(gctx, 8), 0);
	size_type = IntegerType::get(gctx,
		sizeof(unsigned long) * 8);
	data_type = VectorType::get(
		IntegerType::get(gctx, 8), MAX_STORE_SIZE / 8 );

	/* add all fields to types vector from structure */
	std::vector<const Type*> types;

	types.push_back(addr_type);
	types.push_back(size_type);
	types.push_back(data_type);

	type = StructType::get(gctx, types, "GuestMem::Log");
	return type;
}

void MemLog::recordStore(
	Value* log_v, Value* addr_v, Value* data_v, unsigned int mem_slot)
{
	IRBuilder<>	*builder;
	Value		*log_entry;
	LLVMContext	&gctx(getGlobalContext());
	unsigned	bits;

	builder  = theGenLLVM->getBuilder();
	log_entry = UndefValue::get(type);
	log_entry = builder->CreateInsertValue(
		log_entry,
		builder->CreateIntToPtr(addr_v, addr_type),
		address_index);

	bits = data_v->getType()->getPrimitiveSizeInBits();
	log_entry = builder->CreateInsertValue(
		log_entry,
		ConstantInt::get(
			gctx, APInt(sizeof(unsigned long) * 8, 	bits / 8)),
		size_index,
		"logEntryA");

	data_v = builder->CreateBitCast(
		data_v,  IntegerType::get(gctx, bits));
	data_v = builder->CreateZExt(
		data_v, IntegerType::get(gctx, MAX_STORE_SIZE));
	log_entry = builder->CreateInsertValue(
		log_entry,
		builder->CreateBitCast(data_v, data_type),
		data_index, "logEntryV");

	StoreInst* si = builder->CreateStore(log_entry, log_v);
	si->setAlignment(8);
}
