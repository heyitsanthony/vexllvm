#ifndef GUESTCPUSTATE_H
#define GUESTCPUSTATE_H

#include <iostream>
#include <stdint.h>
#include "syscall/syscallparams.h"
#include <sys/user.h>
#include <assert.h>
#include <map>
#include <functional>

#include "arch.h"
#include "guestptr.h"

struct guest_ctx_field
{
	unsigned int	f_len;
	unsigned int	f_count;
	const char*	f_name;
};

#define CASE_OFF2NAME_4(s, m)	\
	case offsetof(s, m):	\
	case 1+offsetof(s, m):	\
	case 2+offsetof(s, m):	\
	case 3+offsetof(s, m):

#define CASE_OFF2NAME_8(s, m)	\
	CASE_OFF2NAME_4(s,m)	\
	case 4+offsetof(s, m):	\
	case 5+offsetof(s, m):	\
	case 6+offsetof(s, m):	\
	case 7+offsetof(s, m):

class GuestCPUState;

typedef std::function<GuestCPUState*(void)> make_guestcpustate_t;

class GuestCPUState
{
public:
typedef std::map<unsigned int, unsigned int> byte2elem_map;
typedef std::map<std::string, unsigned int> reg2byte_map;
	GuestCPUState();
	virtual ~GuestCPUState() {}

	static void registerCPU(Arch::Arch, make_guestcpustate_t);
	static GuestCPUState *create(Arch::Arch);

	unsigned int byteOffset2ElemIdx(unsigned int off) const;

	void* getStateData(void) { return state_data; }
	const void* getStateData(void) const { return state_data; }
	unsigned int getStateSize(void) const { return state_byte_c+1; }
	uint8_t* copyStateData(void) const;
	virtual uint8_t* copyOutStateData(void);

	virtual void setStackPtr(guest_ptr) = 0;
	virtual guest_ptr getStackPtr(void) const = 0;
	virtual void setPC(guest_ptr) = 0;
	virtual guest_ptr getPC(void) const = 0;

	virtual unsigned int getFuncArgOff(unsigned int arg_num) const
	{ assert (0 == 1 && "STUB"); return 0; }
	virtual unsigned int getRetOff(void) const
	{ assert (0 == 1 && "STUB"); return 0; }
	virtual unsigned int getStackRegOff(void) const
	{ assert (0 == 1 && "STUB"); return 0; }
	virtual unsigned int getPCOff(void) const
	{ assert (0 == 1 && "STUB"); return 0; }

	virtual void resetSyscall(void)
	{ assert (0 == 1 && "STUB"); }

	void print(std::ostream& os) const;
	virtual void print(std::ostream& os, const void* regctx) const = 0;

	virtual const char* off2Name(unsigned int off) const = 0;
	unsigned name2Off(const char* name) const;

	virtual bool load(const char* fname);
	virtual bool save(const char* fname);

	/* returns byte offset into raw cpu data that maps to
	 * corresponding gdb register file offset; returns -1
	 * if exceeds bounds */
	virtual int cpu2gdb(int gdb_off) const
	{ assert (0 ==1 && "STUB"); return -1; }

	virtual const struct guest_ctx_field* getFields(void) const = 0;
	unsigned getFieldsSize(const struct guest_ctx_field* f);
	uint64_t getReg(const char* name, unsigned bits, int off=0) const;
	void setReg(const char* name, unsigned bits, uint64_t v, int off=0);

protected:
	byte2elem_map	off2ElemMap;
	reg2byte_map	reg2OffMap;
	uint8_t		*state_data;
	unsigned int	state_byte_c;

private:
	static std::map<Arch::Arch, make_guestcpustate_t> makers;
};

#endif
