#ifndef MEMLOG_H
#define MEMLOG_H

#include <string.h>

namespace llvm {
	class StructType;
	class Value;
	class ConstantInt;
	class IntegerType;
	class PointerType;
	class VectorType;
}
class MemLog {
public:
	MemLog() { clear(); }
	
	/* accessors for the logged data */
	void clear() { memset(this, 0, sizeof(*this)); }	
	bool wasWritten() const { return address != NULL; }
	char* getAddress() const { return address; }
	unsigned long getSize() const { return size; }
	const char* getData() const { return &data[0]; }

	/* interface for llvm code gen to record stored */
	static const llvm::StructType* getType();
	static void recordStore(llvm::Value* log_v, llvm::Value* addr_v, 
		llvm::Value* data_v);

	/* x86 specific thing... */
	static const unsigned int MAX_STORE_SIZE = 128;
private:

	char 		*address;
	unsigned long 	size;
	char		data[MAX_STORE_SIZE / 8];

	/* code generation cache of llvm things */
	static const llvm::StructType* type;
	static const llvm::PointerType* addr_type;
	static const llvm::IntegerType* size_type;
	static const llvm::VectorType* data_type;
	static const unsigned int address_index = 0;
	static const unsigned int size_index = 1;
	static const unsigned int data_index = 2;
};
#endif