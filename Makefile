BIN_BASE="0xa000000"
BIN_BASE2="0xc000000"
ifndef CFLAGS
CFLAGS=
endif

ifeq ($(shell uname -m), armv7l)
#fucking ubuntu mystery commands
CFLAGS=-Wl,-Bsymbolic-functions -Wl,--no-as-needed
endif

CFLAGS += -lssl -g -O3 -I`pwd`/src/ -lcrypto

ifndef TRACE_CFLAGS
TRACE_CFLAGS=-g
endif
TRACE_CFLAGS+= -lm
I386TRACE_CFLAGS=$(TRACE_CFLAGS) -m32
ifndef TRACECC
TRACECC=gcc
endif
ifndef ARMTRACECC
ARMTRACECC=arm-linux-gnueabi-gcc
endif

ifndef LLVMCONFIG_PATH
LLVMCONFIG_PATH = llvm-config
endif

ifeq ($(shell $(LLVMCONFIG_PATH) --version),3.1)
CFLAGS += -DLLVM_VERSION_MAJOR=3 -DLLVM_VERSION_MINOR=1
endif

ifeq ($(shell $(LLVMCONFIG_PATH) --version),3.2)
CFLAGS += -DLLVM_VERSION_MAJOR=3 -DLLVM_VERSION_MINOR=2
endif

LLVMCC=clang
LLVMCFLAGS=$(shell echo $(CFLAGS) | sed "s/-g//g")
#CORECC=g++-4.4.5
CORECC=g++

# XXX, MAKES BINARY SIZE EXPLODE
LDRELOC="-Wl,-Ttext-segment=$(BIN_BASE)"
LDRELOC2="-Wl,-Ttext-segment=$(BIN_BASE2)"
#LDFLAGS=

##  ### ### ###
# # # #  #  # #
# # ###  #  ###
##  # #  #  # #

OBJBASE=	guest.o			\
		guestabi.o		\
		guestmem.o		\
		guestmemsink.o		\
		guestptmem.o		\
		guestmemdual.o		\
		guestcpustate.o		\
		guestelf.o		\
		guestfragment.o		\
		guestptimg.o		\
		guestsnapshot.o		\
		procmap.o		\
		procargs.o		\
		cpu/amd64cpustate.o	\
		cpu/amd64syscalls.o	\
		cpu/i386cpustate.o	\
		cpu/i386syscalls.o	\
		cpu/i386windowsabi.o	\
		cpu/armcpustate.o	\
		cpu/armsyscalls.o	\
		cpu/mips32cpustate.o	\
		fragcache.o		\
		syscall/syscalls.o		\
		syscall/spimsyscalls.o		\
		syscall/syscallsmarshalled.o	\
		symbols.o		\
		elfimg.o		\
		elfdebug.o		\
		ptctl.o			\
		ptimgremote.o		\
		ptimgchk.o		\
		ptimgarch.o		\
		elfsegment.o

OBJSLLVM=	vexxlate.o		\
		vexstmt.o		\
		vexsb.o			\
		vexexpr.o		\
		vexop.o			\
		memlog.o		\
		genllvm.o		\
		vexfcache.o		\
		vexjitcache.o		\
		vexexec.o		\
		vexexecchk.o		\
		vexexecfastchk.o	\
		vexhelpers.o		\

VDSO_OBJ=	vdso/vdso_none.o

ifeq ($(shell uname -m), armv7l)
CFLAGS += -Wl,-Bsymbolic-functions -Wl,--no-as-needed -I/usr/include/arm-linux-gnueabi/ -lcrypto -lcrypt -lrt -lpthread
OBJBASE += cpu/ptimgarm.o
VEXLIB="/usr/lib/valgrind/libvex-arm-linux.a"
endif

ifeq ($(shell uname -m), x86_64)
VEXLIB="/usr/lib/valgrind/libvex-amd64-linux.a"
OBJBASE +=	cpu/ptimgamd64.o	\
		cpu/ptimgi386.o		\
		cpu/amd64_trampoline.o
VDSO_OBJ= vdso/vdso_x64.o
endif

OBJBASE += $(VDSO_OBJ)



OBJDEPS=$(OBJBASE) $(OBJSLLVM)

#LLVMCONFIG_PATH=llvm-config
CFLAGS += -I$(shell $(LLVMCONFIG_PATH) --includedir)
LLVMLDFLAGS=$(shell $(LLVMCONFIG_PATH) --ldflags)
LLVMLINK=$(shell $(LLVMCONFIG_PATH) --bindir)/llvm-link
LLVM_FLAGS_ORIGINAL=$(shell $(LLVMCONFIG_PATH) --ldflags --cxxflags --libs all)
LLVMFLAGS:=$(shell echo "$(LLVM_FLAGS_ORIGINAL)" |  sed "s/-Woverloaded-virtual//;s/-fPIC//;s/-DNDEBUG//g;s/-O3/ /g;") -Wall

ifndef VALGRIND_TRUNK
VALGRIND_TRUNK=/home4/ajromano/valgrind
endif
HAS_VALGRIND_TRUNK=$(shell [ -d $(VALGRIND_TRUNK) ] && echo \'yes\' )
ifeq ($(HAS_VALGRIND_TRUNK), 'yes')
	VEXLIB=$(VALGRIND_TRUNK)/lib/valgrind/libvex-amd64-linux.a
	CFLAGS += -I$(VALGRIND_TRUNK)/include -DVALGRIND_TRUNK
endif


FPDEPS= vexop_fp.o
SOFTFLOATDEPS=vexop_softfloat.o


BITCODE_FILES=	bitcode/libvex_amd64_helpers.bc	\
		bitcode/libvex_x86_helpers.bc	\
		bitcode/libvex_arm_helpers.bc	\
		bitcode/vexops.bc		\
		bitcode/softfloat.bc		\
		bitcode/fpu-softgun.bc		\
		bitcode/fpu-softfloat.bc	\
		bitcode/fpu-linuxmips.bc	\
		bitcode/fpu-bsdppc.bc		\
		bitcode/fpu-bsdhppa.bc

OBJDIRDEPS=$(OBJDEPS:%=obj/%)
FPDIRDEPS=$(FPDEPS:%=obj/%)
SOFTFLOATDIRDEPS=$(SOFTFLOATDEPS:%=obj/%)

#TODO: use better config options
BINTARGETS=	elf_run jit_test	\
		ss_run			\
		frag_run		\
		pt_run pt_xchk		\
		ss_chkpt		\
		vexllvm_ss		\
		dump_loader

BINOBJS=$(BINTARGETS:%=obj/%.o)
BINTARGETS_FP=$(BINTARGETS:%=bin/%)
BINTARGETS_FP_REBASE=$(BINTARGETS:%=bin/%_rebase)
BINTARGETS_SOFTFLOAT=$(BINTARGETS:%=bin/softfloat/%)
BINTARGETS_SOFTFLOAT_REBASE=$(BINTARGETS:%=bin/softfloat/%_rebase)
BINTARGETS_REBASE=$(BINTARGETS_SOFTFLOAT_REBASE) $(BINTARGETS_FP_REBASE)
BINTARGETS_STDBASE=$(BINTARGETS_FP) $(BINTARGETS_SOFTFLOAT)
BINTARGETS_ALL=$(BINTARGETS_STDBASE) $(BINTARGETS_REBASE)

LIBTARGETS=	bin/vexllvm.a 			\
		bin/vexllvm-softfloat.a

all:	bitcode 				\
	$(BINOBJS)				\
	$(LIBTARGETS)				\
	$(BINTARGETS_STDBASE)			\

all-rebase: all $(BINTARGETS_REBASE)

clean:
	rm -f $(BINOBJS) obj/*vexop*.o $(OBJDIRDEPS) $(BINTARGETS_ALL) $(LIBTARGETS) bitcode/*softfloat*

bitcode: $(BITCODE_FILES)

bin/vexllvm.a: $(OBJDIRDEPS) $(FPDIRDEPS)
	ar r $@ $^

bin/vexllvm-softfloat.a: $(OBJDIRDEPS) $(SOFTFLOATDIRDEPS)
	ar r $@ $^

bin/vexllvm_ss: $(OBJBASE:%=obj/%) obj/vexllvm_ss.o
	g++ $(CFLAGS) $^ $(VEXLIB) -o $@ $(LDRELOC) -ldl -lrt

bin/vexllvm_ss-static: $(OBJBASE:%=obj/%) obj/vexllvm_ss.o
	g++ -static $(CFLAGS) $^ $(VEXLIB) -o $@ $(LDRELOC) -ldl -lrt -lcrypt -lcrypto


bin/vexllvm_ss_rebase: $(OBJBASE:%=obj/%) obj/vexllvm_ss.o
	g++ $(CFLAGS) $^ $(VEXLIB) -o $@ $(LDRELOC2) -ldl -lrt

bin/%_rebase: $(OBJDIRDEPS) $(FPDIRDEPS) obj/%.o
	g++ $(CFLAGS) $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC2) -ldl -lrt

bin/%: $(OBJDIRDEPS) $(FPDIRDEPS) obj/%.o
	g++ $(CFLAGS) $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC) -ldl -lrt

bin/softfloat/%_rebase: $(OBJDIRDEPS) $(SOFTFLOATDIRDEPS) obj/%.o
	g++ $(CFLAGS) $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC2) -ldl -lrt

bin/softfloat/%: $(OBJDIRDEPS) $(SOFTFLOATDIRDEPS) obj/%.o
	g++ $(CFLAGS) $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC) -ldl -lrt

#obj/libvex_amd64_helpers.s: bitcode/libvex_amd64_helpers.bc
#	llc  $< -o $@

#obj/libvex_amd64_helpers.o:   obj/libvex_amd64_helpers.s
#	g++ $(CFLAGS)$(LLVMFLAGS)  -o $@ -c $<


bitcode/fpu-softfloat.bc: bitcode/softfloat-fpu.bc bitcode/vexops_softfloat.bc
	$(LLVMLINK) -o $@ $^

bitcode/softfloat.bc: bitcode/softfloat-fpu.bc bitcode/vexops_softfloat.bc
	$(LLVMLINK) -o $@ $^

bitcode/fpu-bsdppc.bc: bitcode/bsd-fpu-ppc.bca bitcode/vexops_softfloat.bc
	$(LLVMLINK) -o $@ $^

bitcode/fpu-softgun.bc: bitcode/softgun-fpu.bca bitcode/vexops_softfloat.bc
	$(LLVMLINK) -o $@ $^


bitcode/fpu-bsdhppa.bc: bitcode/bsd-fpu-hppa.bca bitcode/vexops_softfloat.bc
	$(LLVMLINK) -o $@ $^

bitcode/fpu-linuxmips.bc: bitcode/linux-fpu-mips.bca bitcode/vexops_softfloat.bc
	$(LLVMLINK) -o $@ $^


bitcode/%.bc: support/%.c
	$(LLVMCC) $(LLVMCFLAGS) -emit-llvm -O3 -c $< -o $@

obj/%.o: src/%.s
	gcc $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.cc src/%.h
	$(CORECC) $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

obj/%.o: src/%.cc
	$(CORECC) $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

# TEST DATA
TRACEDEPS= nested_call strlen strrchr 	\
	print cmdline getenv 		\
	fwrite malloc memset 		\
	primes memset-large atoi	\
	strcpy qsort strstr		\
	sscanf time gettimeofday	\
	rand gettext uselocale		\
	getopt errno strerror strchrnul	\
	cmdline-many

TRACEDEPS_DYN= dlsym fp-test

tests/traces-bin/dlsym : tests/traces-obj/dlsym.o
	$(TRACECC) $(TRACE_CFLAGS) -ldl $< -o $@

tests/traces-bin/% : tests/traces-obj/%.o
	$(TRACECC) $(TRACE_CFLAGS) $< -o $@

tests/traces-bin/%-static : tests/traces-obj/%.o
	$(TRACECC) $(TRACE_CFLAGS) -static $< -o $@

tests/traces-obj/%.o: tests/traces-src/%.c
	$(TRACECC) $(TRACE_CFLAGS) -c -o $@ $<

tests/traces-arm-bin/dlsym : tests/traces-arm-obj/dlsym.o
	$(ARMTRACECC) $(TRACE_CFLAGS) -ldl $< -o $@

tests/traces-arm-bin/% : tests/traces-arm-obj/%.o
	$(ARMTRACECC) $(TRACE_CFLAGS) $< -o $@

tests/traces-arm-bin/%-static : tests/traces-arm-obj/%.o
	$(ARMTRACECC) $(TRACE_CFLAGS) -static $< -o $@

tests/traces-arm-obj/%.o: tests/traces-src/%.c
	$(ARMTRACECC) $(TRACE_CFLAGS) -c -o $@ $<

tests/traces-i386-bin/dlsym : tests/traces-i386-obj/dlsym.o
	$(TRACECC) $(I386TRACE_CFLAGS) -ldl $< -o $@

tests/traces-i386-bin/% : tests/traces-i386-obj/%.o
	$(TRACECC) $(I386TRACE_CFLAGS) $< -o $@

tests/traces-i386-bin/%-static : tests/traces-i386-obj/%.o
	$(TRACECC) $(I386TRACE_CFLAGS) -static $< -o $@

tests/traces-i386-obj/%.o: tests/traces-src/%.c
	$(TRACECC) $(I386TRACE_CFLAGS) -c -o $@ $<

### ### ### ### ###
 #  #   #    #  #
 #  ### ###  #  ###
 #  #     #  #    #
 #  ### ###  #  ###

tests: test-traces tests-snapshot
tests-softfloat: tests-softfloat-traces

tests/snapshot-bin/threads: tests/snapshot-src/threads.c
	gcc -lpthread -O3 -o $@ $<
	
tests-snapshot: bin/pt_run tests/snapshot-bin/threads
	tests/snapshot.sh

tests-clean:
	rm -f tests/*-bin/* tests/*-obj/* tests/*-out/* tests/meta/*

TRACEDEPS_PATH=						\
	$(TRACEDEPS:%=tests/traces-bin/%)		\
	$(TRACEDEPS:%=tests/traces-bin/%)		\
	$(TRACEDEPS:%=tests/traces-bin/%-static)

ARMTRACEDEPS_PATH=					\
	$(TRACEDEPS:%=tests/traces-arm-bin/%)		\
	$(TRACEDEPS:%=tests/traces-arm-bin/%)		\
	$(TRACEDEPS:%=tests/traces-arm-bin/%-static)

I386TRACEDEPS_PATH=					\
	$(TRACEDEPS:%=tests/traces-i386-bin/%)		\
	$(TRACEDEPS:%=tests/traces-i386-bin/%)		\
	$(TRACEDEPS:%=tests/traces-i386-bin/%-static)

test-traces: all $(BINTARGETS_REBASE) $(TRACEDEPS_PATH)
	tests/traces.sh

test-built-traces: all $(TRACEDEPS_PATH)
	ONLY_BUILTIN=1 tests/traces.sh

tests-arm: all $(ARMTRACEDEPS_PATH)
	RUNCMD=bin/elf_run ONLY_BUILTIN=1 BUSYBOX=1 REALARCH=`uname -m` EMUARCH=armv6l VEXLLVM_LIBARY_ROOT=/usr/arm-linux-gnueabi REALPATH=tests/traces-bin EMUPATH=tests/traces-arm-bin OUTPATH=tests/traces-arm-out tests/traces.sh

tests-i386: all $(I386TRACEDEPS_PATH)
	RUNCMD=bin/elf_run ONLY_BUILTIN=1 BUSYBOX=1 REALARCH=`uname -m` EMUARCH=i686 REALPATH=tests/traces-bin EMUPATH=tests/traces-i386-bin OUTPATH=tests/traces-i386-out tests/traces.sh

tests-pt_xchk: all $(TRACEDEPS_PATH)
	RUNCMD=bin/pt_xchk OUTPATH=tests/traces-xchk-out tests/traces.sh

tests-syscall_xchk: all $(TRACEDEPS_PATH)
	VEXLLVM_XLATE_SYSCALLS=1 RUNCMD=bin/pt_xchk OUTPATH=tests/traces-xchk-out tests/traces.sh

tests-elf: all $(TRACEDEPS_PATH)	
	RUNCMD=bin/elf_run tests/traces.sh

tests-softfloat-elf: all $(TRACEDEPS_PATH)
	RUNCMD=bin/softfloat/elf_run tests/traces.sh

tests-fastxchk: all $(TRACEDEPS_PATH)
	VEXLLVM_FASTCHK=1 RUNCMD=bin/pt_xchk OUTPATH=tests/traces-fastxchk-out tests/traces.sh

tests-xchk_opcode: all $(TRACEDEPS_PATH)
	tests/opcodes.sh

tests-softfloat-traces: all $(TRACEDEPS_PATH)
	RUNCMD=bin/softfloat/pt_trace tests/traces.sh

tests-softfloat-pt_xchk:
	RUNCMD=bin/softfloat/pt_xchk OUTPATH=tests/traces-xchk-out tests/traces.sh

tests-softfloat-fastxchk: all $(TRACEDEPS_PATH)
	VEXLLVM_FASTCHK=1 RUNCMD=bin/softfloat/pt_xchk OUTPATH=tests/traces-fastxchk-out tests/traces.sh

tests-oprof: all
	TRACES_OPROFILE=1 tests/traces.sh

## ## ### ### ###
## ## #    #  # #
# # # ###  #  ###
#   # #    #  # #
#   # ###  #  # #
META_TESTS= 	meta-pt-jit_test		\
		meta-elf-jit_test		\
		meta-xchk-jit_test		\
		meta-fastxchk-jit_test		\
		meta-pt_run-elf_run-print	\
		meta-elf_run-pt_run-print	\
		meta-elf_run-elf_run-print

tests-meta: $(META_TESTS)

meta-pt-jit_test: all
	bin/pt_run_rebase bin/jit_test >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.
meta-elf-jit_test: all
	bin/elf_run_rebase bin/jit_test >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.
meta-xchk-jit_test: all
	bin/pt_xchk_rebase bin/jit_test >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.
meta-fastxchk-jit_test: all
	VEXLLVM_FASTCHK=1 bin/pt_xchk_rebase bin/jit_test >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.
meta-pt_run-elf_run-print: all
	bin/pt_run_rebase bin/elf_run tests/traces-bin/print >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.
meta-elf_run-pt_run-print: all
	bin/elf_run_rebase bin/pt_run tests/traces-bin/print >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.
meta-elf_run-elf_run-print: all
	bin/elf_run_rebase bin/elf_run tests/traces-bin/print >tests/meta/$@.out 2>tests/meta/$@.err; echo $@ done.


tests-web: test-traces tests-pt_xchk tests-fastxchk
tests-softfloat-web: tests-softfloat-traces tests-softfloat-pt_xchk tests-softfloat-fastxchk

