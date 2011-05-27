#ifndef PTIMGCHK_H
#define PTIMGCHK_H

#include <sys/types.h>
#include <sys/user.h>
#include "gueststateptimg.h"

class PTImgChk : public GuestStatePTImg
{
public:
	PTImgChk(int argc, char* const argv[], char* const envp[]);
	virtual ~PTImgChk() {}

	bool continueWithBounds(
		uint64_t start, uint64_t end,
		const VexGuestAMD64State& state);
	void printSubservient(
		std::ostream& os, 
		const VexGuestAMD64State& ref) const;
	void stackTraceSubservient(std::ostream& os);
	void printTraceStats(std::ostream& os);

protected:
	virtual void handleChild(pid_t pid);

private:
	bool doStep(
		uint64_t start, uint64_t end,
		user_regs_struct& regs);

	bool isGuestFailed(
		const VexGuestAMD64State& state,
		user_regs_struct& regs,
		user_fpregs_struct& fpregs) const;

	bool isRegMismatch(
		const VexGuestAMD64State& state,
		const user_regs_struct& regs) const;

	void printUserRegs(
		std::ostream& os, 
		user_regs_struct& regs,
		const VexGuestAMD64State& ref) const;

	void printFPRegs(
		std::ostream& os,
		user_fpregs_struct& fpregs,
		const VexGuestAMD64State& ref) const;

	const char	*binary;
	pid_t		child_pid;
	
	uint64_t	steps;
	uint64_t	blocks;
};

#endif
