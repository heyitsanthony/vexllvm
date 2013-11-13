/* TODO, overwrite VDSO page with this data */
/* replaces VDSO native calls with pure system calls */
/* NOTE: addresses must be exact and code must be shorter than vdso functions* */
.globl vdso_x64_clock_gettime
.type vdso_x64_clock_gettime,@function
vdso_x64_clock_gettime:
	push   %rbp
	mov    %rsp,%rbp
	push   %r13
	push   %r12
	push   %rbx
	sub    $0x10,%rsp
	movslq %edi,%rdi
	mov    $0xe4,%eax
	syscall 
	add    $0x10,%rsp
	pop    %rbx
	pop    %r12
	pop    %r13
	pop    %rbp
	retq   
.Lvdso_x64_clock_gettime_end:
.size vdso_x64_clock_gettime, .Lvdso_x64_clock_gettime_end - vdso_x64_clock_gettime


//__vdso_getcpu:
//vdso_x64_getcpu:
/* XXX: use defaults (rdtscp?) */


//__vdso_gettimeofday:
.globl vdso_x64_gettimeofday
.type vdso_x64_gettimeofday,@function
vdso_x64_gettimeofday:
	push   %rbp
	mov    %rsp,%rbp
	push   %r13
	push   %r12
	push   %rbx
	sub    $0x10,%rsp
	xorq    %rax,%rax
	movb    $0x60,%al
	syscall 
	addq   $0x10,%rsp
	pop    %rbx
	pop    %r12
	pop    %r13
	pop    %rbp
	retq
.Lvdso_x64_gettimeofday_end:
.size vdso_x64_gettimeofday, .Lvdso_x64_gettimeofday_end - vdso_x64_gettimeofday

//__vdso_time:
.globl vdso_x64_time
.type vdso_x64_time,@function
vdso_x64_time:
	push   %rbp
	mov    %rsp,%rbp
	push   %r13
	push   %r12
	push   %rbx
	sub    $0x10,%rsp
	xorq    %rax,%rax
	movb    $0xc9,%al
	syscall 
	addq   $0x10,%rsp
	pop    %rbx
	pop    %r12
	pop    %r13
	pop    %rbp
	retq
.Lvdso_x64_time_end:
.size vdso_x64_time, .Lvdso_x64_time_end - vdso_x64_time


.Lvdso_x64_gettimeofday_str: .string "__vdso_gettimeofday"
.Lvdso_x64_time_str: .string "time"
.Lvdso_x64_clock_gettime_str: .string "clock_gettime"

.globl vdso_tab
.type vdso_tab,@object
vdso_tab:
.quad	vdso_x64_gettimeofday,	.Lvdso_x64_gettimeofday_end-vdso_x64_gettimeofday, .Lvdso_x64_gettimeofday_str
.quad	vdso_x64_time, .Lvdso_x64_time_end-vdso_x64_time,.Lvdso_x64_time_str
.quad	vdso_x64_clock_gettime,.Lvdso_x64_clock_gettime_end-vdso_x64_clock_gettime,.Lvdso_x64_clock_gettime_str
.quad	0,0,0
.Lvdso_tab_end:
.size vdso_tab,.Lvdso_tab_end-vdso_tab
