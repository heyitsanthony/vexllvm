#ifndef PTIMGARM_H
#define PTIMGARM_H

#include <sys/user.h>
#include "ptimgarch.h"

struct VexGuestARMState;

class PTImgARM : public PTImgArch
{
public:
	bool isMatch(void) const;
	bool doStep(guest_ptr start, guest_ptr end, bool& hit_syscall);
	uintptr_t getSysCallResult() const;

private:
	const VexGuestARMState& getVexState(void) const;
	struct user_regs	shadow_reg_cache;
};

#endif
