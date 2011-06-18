#ifndef AMD64CPUSTATE_H
#define AMD64CPUSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscallparams.h"
#include <sys/user.h>
#include <assert.h>
#include <map>
#include "guestcpustate.h"

class AMD64CPUState : public GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;

	AMD64CPUState();
	~AMD64CPUState();
	const llvm::Type* getTy(void) const { return guestCtxTy; }
	unsigned int byteOffset2ElemIdx(unsigned int off) const;
	void* getStateData(void) { return state_data; }
	const void* getStateData(void) const { return state_data; }
	unsigned int getStateSize(void) { return state_byte_c; }
	void setStackPtr(void*);
	void* getStackPtr(void) const;
	SyscallParams getSyscallParams(void) const;
	void setSyscallResult(uint64_t ret);
	uint64_t getExitCode(void) const;
	/* byte offset into state data for exit type byte */
	unsigned int getExitTypeOffset(void) const { return state_byte_c; }

	void setExitType(GuestExitType et) { *exit_type = (uint8_t)et; }
	GuestExitType getExitType(void) { return (GuestExitType)*exit_type; }

	void setFuncArg(uintptr_t arg_val, unsigned int arg_num);
	void setRegs(const user_regs_struct& regs, const user_fpregs_struct& fpregs);

	void print(std::ostream& os) const;

	GuestTLS* getTLS(void) { return tls; }
	const GuestTLS* getTLS(void) const { return tls; }
	void setTLS(GuestTLS* tls);

	void setFSBase(uintptr_t base);
	uintptr_t getFSBase() const;

	const char* off2Name(unsigned int off) const;
protected:
	llvm::Type* mkFromFields(struct guest_ctx_field* f, byte2elem_map&);
	void mkRegCtx(void);
private:
	byte2elem_map	off2ElemMap;
	llvm::Type	*guestCtxTy;
	uint8_t		*state_data;	/* amd64 guest + exit_type */
	uint8_t		*exit_type;	/* ptr into state data */

	GuestTLS	*tls;
	unsigned int	state_byte_c;
};

#endif
