#ifndef GUESTABI_H
#define GUESTABI_H

class Guest;

class GuestABI
{
public:
	virtual ~GuestABI() {}
	virtual SyscallParams getSyscallParams(void) const = 0;
	virtual void setSyscallResult(uint64_t ret) = 0;
	virtual uint64_t getExitCode(void) const = 0;
	virtual uint64_t getSyscallResult(void) const = 0;
	static GuestABI* create(Guest& g);

	static bool use_linux_sysenter;

protected:
	GuestABI(Guest& _g) : g(_g) {}
	Guest	&g;
};

class RegStrABI : public GuestABI
{
public:
	RegStrABI(Guest& g_,
		const char** sc_regs,
		const char* sc_ret,
		const char* exit_reg,
		bool	_is_32bit);
	virtual ~RegStrABI(void) {}

	virtual SyscallParams getSyscallParams(void) const;
	virtual void setSyscallResult(uint64_t ret);
	virtual uint64_t getExitCode(void) const;
	virtual uint64_t getSyscallResult(void) const;
private:
	unsigned	sc_reg_off[8]; /* [0] = sc_num, [1] = arg0, ... */
	unsigned	scret_reg_off;
	unsigned	exit_reg_off;
	bool		is_32bit;
};

#endif
