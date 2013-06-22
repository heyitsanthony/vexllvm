extern "C" {
#include <valgrind/libvex_guest_mips32.h>
}
#include "spimsyscalls.h"
#include "guest.h"
#include "guestcpustate.h"
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define SPIM_PRINT_INT		1
#define SPIM_PRINT_FLOAT	2
#define SPIM_PRINT_DOUBLE	3
#define SPIM_PRINT_STRING	4
#define SPIM_READ_INT		5
#define SPIM_READ_FLOAT		6
#define SPIM_READ_DOUBLE	7
#define SPIM_READ_STRING	8
#define SPIM_SBRK		9
#define SPIM_EXIT		10
#define SPIM_PRINT_CHAR		11
#define SPIM_READ_CHAR		12
#define SPIM_OPEN		13
#define SPIM_READ		14
#define SPIM_WRITE		15
#define SPIM_CLOSE		16
#define SPIM_EXIT2		17


uint64_t SPIMSyscalls::apply(SyscallParams& args)
{
	VexGuestMIPS32State* s;
	
	s = (VexGuestMIPS32State*)guest->getCPUState()->getStateData();

//	std::cerr << "[SPIMSys] Call=" << args.getSyscall() << '\n';

	switch (args.getSyscall()) {
	case SPIM_PRINT_INT: std::cout << args.getArg(0); break;
	case SPIM_PRINT_FLOAT: {
		float	f;
		memcpy(&f, &s->guest_f12, sizeof(f));
		std::cout << f;
		break;
	}
	case SPIM_PRINT_DOUBLE: {
		double	d;
		memcpy(&d, &s->guest_f12, sizeof(d));
		std::cout << d;
		break;
	}
	case SPIM_PRINT_STRING:
		std::cout << (char*)args.getArgPtr(0);
		break;

	case SPIM_READ_INT: {
		uint32_t	i;
		std::cin >> i;
		s->guest_r2 = i;
		return s->guest_r2;
	}

	case SPIM_READ_FLOAT: {	
		float	f;
		std::cin >> f;
		memcpy(&s->guest_f0, &f, sizeof(f));
		break;
	}
	case SPIM_READ_DOUBLE: {
		double	d;
		std::cin >> d;
		memcpy(&s->guest_f0, &d, sizeof(d));
		break;
	}
	case SPIM_READ_STRING: {
		char	*b = (char*)args.getArgPtr(0);
		do {
			b[0] = '\0';
			if (!std::cin.getline(b, args.getArg(1)))
				break;
		} while (b[0] == '\n' || b[0] == '\0');
		break;
	}	
	case SPIM_SBRK: {
		guest_ptr	p(guest->getMem()->brk());
		guest->getMem()->sbrk(p + args.getArg(0));
		return p;
	}
	case SPIM_EXIT:
		exited = true;
		return args.getArg(0);
	case SPIM_PRINT_CHAR:
		std::cout << (char*)args.getArg(0);
		break;
	case SPIM_READ_CHAR: {
		char c;
		std::cin >> c;
		return c;
	}
	case SPIM_OPEN:
		return open((char*)args.getArgPtr(0), O_RDONLY);
	case SPIM_READ:
		return read(
			args.getArg(0),
			(char*)args.getArgPtr(1),
			args.getArg(2));
	case SPIM_WRITE:
		return write(
			args.getArg(0),
			(char*)args.getArgPtr(1),
			args.getArg(2));
	case SPIM_CLOSE:
		close(args.getArg(0));
		return 0;
	case SPIM_EXIT2:
		exited = true;
		return s->guest_r4;
	default:
		std::cerr 
			<< "[sysSPIM] Unknown syscall "
			<< args.getSyscall()
			<< '\n';
	}

	return 0;
}
