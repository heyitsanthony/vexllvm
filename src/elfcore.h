#ifndef ELFCORE_H
#define ELFCORE_H


#define PRSTATUS_REGS_SIZE	(27*8)

struct elf_siginfo
{
	int	si_signo;			/* signal number */
	int	si_code;			/* extra code */
	int	si_errno;			/* errno */
};



struct prstatus
{
	struct elf_siginfo	pr_info;
	uint16_t pr_cursig;
	uint64_t pr_sigpend;
	uint64_t pr_sighold;
	pid_t pr_pid;
	pid_t pr_ppid;
	pid_t pr_pgrp;
	pid_t pr_sid;
	struct timeval pr_utime;
	struct timeval pr_stime;
	struct timeval pr_cutime;
	struct timeval pr_cstime;
	struct
	{
	uint64_t pr_reg[PRSTATUS_REGS_SIZE / sizeof (uint64_t)];
	}
	#ifdef ALIGN_PR_REG
	__attribute__ ((aligned (ALIGN_PR_REG)))
	#endif
	;
	int32_t pr_fpvalid;
};

typedef struct prstatus prstatus_t;


#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H	1

#include <features.h>
#include <signal.h>

/* We need the signal context definitions even if they are not used
   included in <signal.h>.  */
#include <bits/sigcontext.h>

#ifdef __x86_64__

/* Type for general register.  */
__extension__ typedef long long int greg_t;

/* Number of general registers.  */
#define EC_NGREG	23

/* Container for all general registers.  */
typedef greg_t gregset_t[EC_NGREG];


struct _libc_fpxreg
{
  unsigned short int significand[4];
  unsigned short int exponent;
  unsigned short int padding[3];
};

struct _libc_xmmreg
{
  __uint32_t	element[4];
};

struct _libc_fpstate
{
  /* 64-bit FXSAVE format.  */
  __uint16_t		cwd;
  __uint16_t		swd;
  __uint16_t		ftw;
  __uint16_t		fop;
  __uint64_t		rip;
  __uint64_t		rdp;
  __uint32_t		mxcsr;
  __uint32_t		mxcr_mask;
  struct _libc_fpxreg	_st[8];
  struct _libc_xmmreg	_xmm[16];
  __uint32_t		padding[24];
};

/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate *fpregset_t;

#else /* !__x86_64__ */

/* Type for general register.  */
typedef int greg_t;

/* Number of general registers.  */
#define EC_NGREG	19

/* Container for all general registers.  */
typedef greg_t gregset_t[EC_NGREG];


/* Definitions taken from the kernel headers.  */
struct _libc_fpreg
{
  unsigned short int significand[4];
  unsigned short int exponent;
};

struct _libc_fpstate
{
  unsigned long int cw;
  unsigned long int sw;
  unsigned long int tag;
  unsigned long int ipoff;
  unsigned long int cssel;
  unsigned long int dataoff;
  unsigned long int datasel;
  struct _libc_fpreg _st[8];
  unsigned long int status;
};

/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate *fpregset_t;

#endif /* !__x86_64__ */

#endif /* sys/ucontext.h */


typedef greg_t elf_greg_t;
typedef gregset_t elf_gregset_t;
typedef fpregset_t elf_fpregset_t;
//typedef fpxregset_t elf_fpxregset_t;

#define ELF_PRARGSZ	(80)	/* Number of chars for args */

struct elf_prpsinfo
{
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* char for pr_state */
	char	pr_zomb;	/* zombie */
	char	pr_nice;	/* nice val */
	unsigned long pr_flag;	/* flags */
	uid_t	pr_uid;
	gid_t	pr_gid;
	pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	/* Lots missing */
	char	pr_fname[16];	/* filename of executable */
	char	pr_psargs[ELF_PRARGSZ];	/* initial part of arg list */
};

typedef struct elf_prpsinfo prpsinfo_t;


#endif
