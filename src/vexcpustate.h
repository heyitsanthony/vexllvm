#ifndef VEXCPUSTATE_H
#define VEXCPUSTATE_H

#include "guestcpustate.h"
#include "arch.h"

/* types of exit states we need to worry about on return */
enum GuestExitType {
	GE_IGNORE = 0, 	/* use regular control path */
	GE_SIGTRAP = 1,
	GE_SIGSEGV = 2,
	GE_SIGBUS = 3,
	GE_EMWARN = 4,
	GE_SYSCALL = 5,
	GE_CALL = 6,
	GE_RETURN = 7,
	GE_YIELD = 8,
	GE_INT = 9,
	/* XXX ADD MORE */ };

class VexCPUState : public GuestCPUState
{
public:
	VexCPUState(const guest_ctx_field *f)
		: GuestCPUState(f)
		, exit_type(nullptr) {}

	static VexCPUState *create(Arch::Arch);

	/* byte offset into state data for exit type byte */
	unsigned int getExitTypeOffset(void) const { return state_byte_c; }
	void setExitType(GuestExitType et) { *exit_type = (uint8_t)et; }
	GuestExitType getExitType(void) { return (GuestExitType)*exit_type; }
	
	uint8_t* copyOutStateData(void) {
		uint8_t	*old = GuestCPUState::copyOutStateData();
		exit_type = &state_data[state_byte_c];
		return old;
	}

	static void registerCPUs(void);

protected:
	uint8_t		*exit_type;
};

#endif
