/*  FROM
	qemu.git
	eb47d7c5d96060040931c42773ee07e61e547af9
	linux-user/arm/syscall.c
	
    ***	NOTE: these are the header includes so we can namespace several	****
    ***	      includes of  the syscall.c file (one per arch)  		****
*/
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <elf.h>
#include <endian.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/swap.h>
#include <signal.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/times.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/statfs.h>
#include <utime.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
//#include <sys/user.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/wireless.h>
#ifdef TARGET_GPROF
#include <sys/gmon.h>
#endif
#ifdef CONFIG_EVENTFD
#include <sys/eventfd.h>
#endif
#ifdef CONFIG_EPOLL
#include <sys/epoll.h>
#endif
#include <termios.h>
#include <linux/unistd.h>
#include <linux/utsname.h>
#include <linux/cdrom.h>
#include <linux/hdreg.h>
#include <linux/soundcard.h>
#include <linux/kd.h>
#include <linux/mtio.h>
#include <linux/fs.h>
#if defined(CONFIG_FIEMAP)
#include <linux/fiemap.h>
#endif
#include <linux/fb.h>
#include <linux/vt.h>
#ifdef CONFIG_INOTIFY
#include <sys/inotify.h>
#endif
#if defined(__NR_mq_open)
#include <mqueue.h>
#endif
/* using the platform one which apparently was prob with qemu
   for people with older kernels... */
#include <linux/loop.h>
/* this one if for the signal handling code */
#include <sys/ucontext.h>

/* i am bitch! i must include EVERYTHING because of this sycall emulation */
#include <sys/personality.h>
#include <sys/file.h>
#include <sys/fsuid.h>
#include <grp.h>
#include <time.h>



/* Translation table for bitmasks... */
typedef struct bitmask_transtbl {
	unsigned int	x86_mask;
	unsigned int	x86_bits;
	unsigned int	alpha_mask;
	unsigned int	alpha_bits;
} bitmask_transtbl;
