#include <sys/mman.h>
#include <elf.h>
#include <iostream>
#include <fstream>
#include <unistd.h>

#include "guest.h"
#include "guestcpustate.h"
#include "elfimg.h"
#include "elfcore.h"

#define WARNING(x)	fprintf(stderr, "WARNING: " x "\n")

extern "C" { extern uint64_t LibVEX_GuestAMD64_get_rflags(const void* s); }

template <typename Elf_Nhdr>
static unsigned writeNote(std::ostream& os, int ty, const void* b, unsigned b_c)
{
	Elf_Nhdr	nhdr;

	nhdr.n_namesz = 5;
	nhdr.n_descsz = b_c;
	nhdr.n_type = ty;
	os.write((char*)&nhdr, sizeof(nhdr));
	os.write("CORE\0\0\0\0", 8); /* lol */
	os.write((const char*)b, b_c);
	
	return sizeof(nhdr) + nhdr.n_namesz + nhdr.n_descsz;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Nhdr>
static void writeElfCore(const Guest *gs, std::ostream& os)
{
	unsigned	map_c, load_off, note_off, note_sz, slack;
	std::list<GuestMem::Mapping>	maps(gs->getMem()->getMaps());
	Elf_Ehdr	ehdr;
	Elf_Phdr	phdr;
	Arch::Arch	arch;


	struct _libc_fpstate	fpr;
	/* stuff for NOTE section */
	prstatus_t	prs;
	prpsinfo_t	prps;

	memset(&prs, 0, sizeof(prs));
	memset(&prps, 0, sizeof(prps));

	memset(&ehdr.e_ident, 0, sizeof(ehdr.e_ident));
	strcpy((char*)&ehdr.e_ident, "\x7f\x45\x4c\x46\x02\x01\x01");

	arch = gs->getArch();
	ehdr.e_type = ET_CORE;
	switch (arch) {
	case Arch::I386:
		ehdr.e_machine = EM_386;
		ehdr.e_ident[EI_CLASS] = ELFCLASS32;
#if 0
		prs.pr_reg[0] = GS;
		prs.pr_reg[1] = FS;
		prs.pr_reg[2] = ES;
		prs.pr_reg[3] = DS;
		prs.pr_reg[4] = EDI;
		prs.pr_reg[5] = ESI;
		prs.pr_reg[6] = EBP;
		prs.pr_reg[7] = ESP;
		prs.pr_reg[8] = EBX;
		prs.pr_reg[9] = EDX;
		prs.pr_reg[10] = ECX;
		prs.pr_reg[11] = TRAPNO;
		prs.pr_reg[12] = ERR;
		prs.pr_reg[13] = EIP;
		prs.pr_reg[14] = EFL;
		prs.pr_reg[15] = UESP;
		prs.pr_reg[16] = SS;
#endif
		assert (0 == 1 && "STUB");
		abort();
		break;

	case Arch::X86_64:
		ehdr.e_machine = EM_X86_64;
		ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	
#define GET_REG(x,y,z) gs->getCPUState()->getReg(x, y, z)
#define SET_PRREG(i,x) prs.pr_reg[i] = gs->getCPUState()->getReg(x, 64)
		
		/* taken from elfutils prstatus_regs */
		SET_PRREG(0, "R15");
		SET_PRREG(1, "R14");
		SET_PRREG(2, "R13");
		SET_PRREG(3, "R12");
		SET_PRREG(4, "RBP");
		SET_PRREG(5, "RBX");
		SET_PRREG(6, "R11");
		SET_PRREG(7, "R10");
		SET_PRREG(8, "R9");
		SET_PRREG(9, "R8");
		SET_PRREG(10, "RAX");
		SET_PRREG(11, "RCX");
		SET_PRREG(12, "RDX");
		SET_PRREG(13, "RSI");
		SET_PRREG(14, "RDI");
		SET_PRREG(16, "RIP");
		SET_PRREG(19, "RSP");

		SET_PRREG(21, "FS_ZERO");
		prs.pr_reg[18] = LibVEX_GuestAMD64_get_rflags(
			gs->getCPUState()->getStateData());
		// SET_PRREG(17, "CSFSGS");
		WARNING("core not setting CSFSGS");
		// SET_PRREG(18, "TRAPNO");
		WARNING("core not setting TRAPNO");
		// SET_PRREG(19, "OLDMASK");
		WARNING("core not setting CR2");
		// SET_PRREG(20, "CR2");


		memset(&fpr, 0, 0);
#if 0
/* UHH?? */
		fpr.cwd
		fpr.swd
		fpr.ftw
		fpr.fop
		fpr.rip
		fpr.rdp
		fpr.mxcsr
		fpr.mxcr_mask
		fpr._st[...];
#endif
		for (int i = 0; i < 16; i++) {
			uint64_t *v = (uint64_t*)(((char*)&fpr) + (32 + 128 + i*2*8));
			v[1] = GET_REG("YMM", 64, i*4);
			v[0] = GET_REG("YMM", 64, i*4 + 1);
		}
		break;
	default:
		std::cerr << "ARCH = " << arch << '\n';
		assert (0 == 1 && "WAT??");
		abort();
	}

	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV; /* XXX */

	ehdr.e_version = EV_CURRENT;
	ehdr.e_entry = 0;
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_shoff = 0;
	ehdr.e_flags = 0;
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf_Phdr);
	ehdr.e_shentsize = 0;
	ehdr.e_shnum = 0;
	ehdr.e_shstrndx = 0;


	map_c = maps.size();
	ehdr.e_phnum = map_c + 1 /* note section */;

	os.write((char*)&ehdr, sizeof(ehdr));

	note_off = ehdr.e_phoff + ehdr.e_phnum*sizeof(Elf_Phdr);
	note_sz = 3*(sizeof(Elf_Nhdr)+8) + sizeof(prs) + sizeof(prps) + sizeof(fpr);

	/* the NOTE phdr */
	phdr.p_type = PT_NOTE;
	phdr.p_offset = note_off; 
	phdr.p_vaddr = 0;
	phdr.p_paddr = 0;
	phdr.p_filesz = note_sz;
	phdr.p_memsz = 0;
	phdr.p_flags = 0;
	phdr.p_align = 0;

	os.write((char*)&phdr, sizeof(phdr));

	/* the LOAD phdrs */
	phdr.p_type = PT_LOAD;
	phdr.p_paddr = 0;
	phdr.p_offset = note_off + note_sz; 
	phdr.p_align = 0x1000;

	load_off = note_off + note_sz;
	slack = (0x1000*(((load_off)+(0xfff))/0x1000))-(load_off);
	load_off += slack;


	unsigned			load_sz = 0;

	foreach (it, maps.begin(), maps.end()) {
		GuestMem::Mapping	gm(*it);
		int			prot = gm.getCurProt();

		phdr.p_vaddr = gm.offset.o;
		phdr.p_filesz = gm.length;
		phdr.p_memsz = gm.length;
		phdr.p_offset = load_off + load_sz; 
		phdr.p_flags =
			((prot & PROT_READ) ? PF_R : 0) |
			((prot & PROT_WRITE) ? PF_W : 0) |
			((prot & PROT_EXEC) ? PF_X : 0);

		load_sz += gm.length;
		os.write((char*)&phdr, sizeof(phdr));
	}

	// NT_PRSTATUS
	/* XXX: this needs to be filled out with actual siginfo
	 * on a fault (SIGSEGV, SIGFPE) */
	prs.pr_pid = getpid();
	prs.pr_ppid = getppid();
	prs.pr_pgrp = getpgrp();
	prs.pr_sid = getsid(0);
	prs.pr_fpvalid = 1;

	/* XXX: this needs to be adapted to work for all
	 * kinds of exceptions.. maybe writeCore should
	 * have a signal struct param? */
	prs.pr_info.si_signo = 11;
	prs.pr_info.si_code = 0;
	prs.pr_info.si_errno = 0;
	prs.pr_cursig  = 11;

	writeNote<Elf_Nhdr>(os, NT_PRSTATUS, &prs, sizeof(prs));

	// NT_PRPSINFO
	//
	memset(&prps, 0, sizeof(prps));
	prps.pr_uid = getuid();
	prps.pr_gid = getgid();
	prps.pr_pid = getpid();
	prps.pr_ppid = getppid();
	prps.pr_pgrp = getpgrp();
	prps.pr_sid = getsid(0);
	prps.pr_fname[0] = 'e';
	/* XXX: need to put argvs in here */
	// prs.pr_psargs

	writeNote<Elf_Nhdr>(os, NT_PRPSINFO, &prps, sizeof(prps));
	writeNote<Elf_Nhdr>(os, NT_FPREGSET, &fpr, sizeof(fpr));

	/* hurr */
	for (unsigned i = 0; i < slack; i++) os.put('\0');

	/* and all the rest! */
	foreach (it, maps.begin(), maps.end()) {
		GuestMem::Mapping	gm(*it);
		char			buf[4096];

		for (unsigned i = 0; i < (gm.length + 4095) / 4096; i++) {
			gs->getMem()->memcpy(
				buf,
				gm.offset + 4096*i,
				4096);
			os.write(buf, 4096);
		}
	}
}

void ElfImg::writeCore(const Guest *gs, std::ostream& os)
{
	Arch::Arch arch(gs->getArch());

	switch (arch) {
	case Arch::I386:
		writeElfCore<Elf32_Ehdr, Elf32_Phdr, Elf32_Nhdr>(gs, os);
		break;
	case Arch::X86_64:
		writeElfCore<Elf64_Ehdr, Elf64_Phdr, Elf64_Nhdr>(gs, os);
		break;
	default:
		assert (0 ==1 && "NOT SUPPORTED");
		abort();
	}
}

void Guest::toCore(const char* path) const
{
	std::ofstream	os(((path) ? path : "core"));

	if (!os.good()) {
		std::cerr << "[Guest] Couldn't save core.\n";
		return;
	}

	ElfImg::writeCore(this, os);
}
