
//#define XMM_BASE	224

.data
amd64_trampoline_pc: .long 0

.text
.global amd64_trampoline
.type  amd64_trampoline,%function
.align 16

/* %RDI = pointer to vex guest state */
amd64_trampoline:
	/* taken from libvex_guest_amd64.h, should probably use ptrace regs */

	// save PC-- we'll jump to it at the end
	pushq	%rdi
	movq	184(%rdi), %rax
	movq	%rax, (amd64_trampoline_pc)

	// load the thread register, %fs (FIRST BUG XCHK FOUND!)
	// The kernel interface uses %rdi, %rsi, %rdx, %r10, %r8 and %r9.
	// A system-call is done via the syscall instruction.
	// The kernel destroys registers %rcx and %r11.
	// The number of the syscall has to be passed in register %rax.
	movq	$158, %rax	/* arch_prctl */
	movq	208(%rdi), %rsi	/* %fs value */
	movq	$0x1002, %rdi	/* ARCH_SET_FS */
	syscall

	popq	%rdi		/* restore vex state to rdi */

	/* first, fake rflags */
	/* we cheat and store rflags in CC_NDEP */
	pushq	168(%rdi)
	popfq

	/* if I weren't so paranoid, I'd make this an aligned access */ 
	movdqu	(224+0*32)(%rdi), %xmm0
	movdqu	(224+1*32)(%rdi), %xmm1
	movdqu	(224+2*32)(%rdi), %xmm2
	movdqu	(224+3*32)(%rdi), %xmm3
	movdqu	(224+4*32)(%rdi), %xmm4
	movdqu	(224+5*32)(%rdi), %xmm5
	movdqu	(224+6*32)(%rdi), %xmm6
	movdqu	(224+7*32)(%rdi), %xmm7
	movdqu	(224+8*32)(%rdi), %xmm8
	movdqu	(224+9*32)(%rdi), %xmm9
	movdqu	(224+10*32)(%rdi), %xmm10
	movdqu	(224+11*32)(%rdi), %xmm11
	movdqu	(224+12*32)(%rdi), %xmm12
	movdqu	(224+13*32)(%rdi), %xmm13
	movdqu	(224+14*32)(%rdi), %xmm14
	movdqu	(224+15*32)(%rdi), %xmm15

	/* XXX load old x87 FP? */
	
	/* GPRs */
	movq	16(%rdi), %rax
	movq	24(%rdi), %rcx
	movq	32(%rdi), %rdx
	movq	40(%rdi), %rbx
	movq	48(%rdi), %rsp
	movq	56(%rdi), %rbp
	movq	64(%rdi), %rsi
	movq	80(%rdi), %r8
	movq	88(%rdi), %r9
	movq	96(%rdi), %r10
	movq	104(%rdi), %r11
	movq	112(%rdi), %r12
	movq	120(%rdi), %r13
	movq	128(%rdi), %r14
	movq	136(%rdi), %r15

	movq	72(%rdi), %rdi	/* with such knowledge, what forgiveness */
	jmp	*(amd64_trampoline_pc) /* Jump to raw code block address */

.size amd64_trampoline,.-amd64_trampoline
