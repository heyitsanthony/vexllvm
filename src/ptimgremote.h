#ifndef PTIMGCHK_H
#define PTIMGCHK_H

#include <sys/types.h>
#include <sys/user.h>
#include "guestptimg.h"
#include "ptimgarch.h"
#include "ptctl.h"

class MemLog;
class SyscallsMarshalled;

class PTImgRemote : public GuestPTImg, public PTCtl
{
public:
	PTImgRemote(
		const char* binname,
		bool use_entry = true/* so templates work */);
	virtual ~PTImgRemote();
protected:
	virtual void handleChild(pid_t pid);
	virtual void slurpBrains(pid_t pid);
private:
	void waitForSingleStep(void);

	uint64_t	blocks;
};

#endif
