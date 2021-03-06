BIN_BASE="0xa000000"
BIN_BASE2="0x10000000"
ifndef CFLAGS
CFLAGS=
endif

ifdef VEXLLVM_CFLAGS
# for termios from the qemu stuff on ubuntoo
# CFLAGS += -ltinfo
CFLAGS += $(VEXLLVM_CFLAGS)
endif

ifeq ($(shell uname -m), armv7l)
#fucking ubuntu mystery commands
CFLAGS=-Wl,-Bsymbolic-functions -Wl,--no-as-needed
endif

PROF_FLAGS = -pg -gprof

# jit does some weird memory accesses, but functions are annotated with
# ATTRIBUTE_NO_SANITIZE_ADDRESS so there's no false positive.
ASAN_FLAGS = -fsanitize=address

# linker flags get stripped off so clang won't complain too hard
CFLAGS +=  	-std=c++14 -g -O3 -I`pwd`/src/
CFLAG_LIBS = 	-lcrypto -ldl -lrt -lcrypt -lpthread -lssl -lz -lncurses

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


ifneq ($(shell readlink guestlib),)
GUESTLIB_PATH=`pwd`/guestlib
else
ifndef GUESTLIB_PATH
$(error "GUESTLIB_PATH not defined")
endif
endif

GUESTLIB=$(GUESTLIB_PATH)/bin/guestlib.a
ifeq ($(shell ls $(GUESTLIB)),)
$(error "No guestlib.a compiled?")
endif


CFLAGS += -I$(GUESTLIB_PATH)/src

CFLAGS += $(CFLAG_LIBS)

LLVMCC=clang
LLVMCFLAGS=$(shell echo $(CFLAGS) | sed "s/-g//g")
CORECC=clang++
CORELINK=clang++

# XXX, MAKES BINARY SIZE EXPLODE
LDRELOC="-Wl,-Ttext-segment=$(BIN_BASE)"
LDRELOC2="-Wl,-Ttext-segment=$(BIN_BASE2)"
#LDFLAGS=

##  ### ### ###
# # # #  #  # #
# # ###  #  ###
##  # #  #  # #

OBJBASE=	vexcpustate.o		\
		fragcache.o		\
		elfcore.o		\
		ptimgchk.o		\
		usage.o			\
		syscall/spimsyscalls.o	\
		cpu/amd64cpustate.o	\
		cpu/amd64syscalls.o	\
		cpu/i386cpustate.o	\
		cpu/i386syscalls.o	\
		cpu/armcpustate.o	\
		cpu/armsyscalls.o	\
		cpu/mips32cpustate.o

OBJSLLVM=	genllvm.o		\
		vexxlate.o		\
		vexstmt.o		\
		vexsb.o			\
		vexexpr.o		\
		vexop.o			\
		memlog.o		\
		debugprintpass.o	\
		jitengine.o		\
		jitobjcache.o		\
		vexfcache.o		\
		vexjitcache.o		\
		vexexec.o		\
		vexexecchk.o		\
		vexexecfastchk.o	\
		vexhelpers.o

ifeq ($(shell uname -m), armv7l)
CFLAGS += -Wl,-Bsymbolic-functions -Wl,--no-as-needed -I/usr/include/arm-linux-gnueabi/
VEXLIB="/usr/lib/valgrind/libvex-arm-linux.a"
endif

ifeq ($(shell uname -m), x86_64)
VEXLIB="/usr/lib/valgrind/libvexmultiarch-amd64-linux.a" "/usr/lib/valgrind/libvex-amd64-linux.a"
OBJBASE +=	cpu/ptshadowamd64.o	\
		cpu/ptshadowi386.o	\
		cpu/amd64_trampoline.o
endif

OBJDEPS=$(OBJBASE) $(OBJSLLVM)

#LLVMCONFIG_PATH=llvm-config
CFLAGS +=-I$(shell $(LLVMCONFIG_PATH) --includedir)
LLVMLDFLAGS=$(shell $(LLVMCONFIG_PATH) --ldflags)
LLVMLINK=$(shell $(LLVMCONFIG_PATH) --bindir)/llvm-link
LLVM_FLAGS_ORIGINAL=$(shell $(LLVMCONFIG_PATH) --ldflags --cxxflags --libs all)
LLVMFLAGS:=$(shell echo "$(LLVM_FLAGS_ORIGINAL)" |  sed "s/c++11/c++14/g;s/-Woverloaded-virtual//;s/-fPIC//;s/-DNDEBUG//g;s/-O3/ /g;s/-Wno-maybe-uninitialized//g;") -Wall

CFLAGS0= $(shell echo $(CFLAGS) | sed "s|-[lL][A-Za-z].*[^ ] | |g")
LLVMFLAGS0= $(shell echo $(LLVMFLAGS) | sed "s| -[lL][A-Za-z0-9_]*| |g;s|-L/usr/lib64/[^ ].*| |g")

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

LINKLIBS=	$(VEXLIB) $(GUESTLIB)

all:	bitcode 				\
	$(BINOBJS)				\
	$(LIBTARGETS)				\
	$(BINTARGETS_STDBASE)			\

all-rebase: all $(BINTARGETS_REBASE)

scan-build:
	mkdir -p scan-out
	scan-build \
		`clang -cc1 -analyzer-checker-help | awk ' { print "-enable-checker="$1 } ' | grep '\.' | grep -v debug ` \
		-o `pwd`/scan-out make -j7 all


clean:
	rm -f $(BINOBJS) obj/*vexop*.o $(OBJDIRDEPS) $(BINTARGETS_ALL) $(LIBTARGETS)

# don't wipe out softfloat because it mostly relies on fpu.git
clean-all: clean
	rm -f bitcode/*softfloat*

gen-sc-headers: src/syscall/xlate-linux-x86.h src/syscall/xlate-linux-x64.h

src/syscall/xlate-linux-x86.h: /usr/include/asm/unistd_32.h
	echo  "#ifndef XLATE_LINUX_X86_H" >src/syscall/xlate-linux-x86.h
	echo  "const char *xlate_tab_x86[] = {" >>src/syscall/xlate-linux-x86.h
	grep NR_ /usr/include/asm/unistd_32.h | sed "s/__NR_//g" | awk ' BEGIN { last = 0; } { x = last; while (x < $$3) {  print "NULL, ";  x++; } last = x+1; print "\"" $$2 "\","; }' >>src/syscall/xlate-linux-x86.h
	echo -e -n "};\n#endif" >>src/syscall/xlate-linux-x86.h
	

src/syscall/xlate-linux-x64.h: /usr/include/asm/unistd_64.h
	echo  "#ifndef XLATE_LINUX_X64_H" >src/syscall/xlate-linux-x64.h
	echo  "const char *xlate_tab_x64[] = {" >>src/syscall/xlate-linux-x64.h
	grep NR_ /usr/include/asm/unistd_64.h | sed "s/__NR_//g" | awk ' BEGIN { last = 0; } { x = last; while (x < $$3) {  print "NULL, ";  x++; } last = x+1; print "\"" $$2 "\","; }' >>src/syscall/xlate-linux-x64.h
	echo -e -n "};\n#endif" >>src/syscall/xlate-linux-x64.h



bitcode: $(BITCODE_FILES)

bin/vexllvm.a: $(OBJDIRDEPS) $(FPDIRDEPS)
	ar r $@ $^

bin/vexllvm-softfloat.a: $(OBJDIRDEPS) $(SOFTFLOATDIRDEPS)
	ar r $@ $^

bin/vexllvm_ss: $(OBJBASE:%=obj/%) obj/vexllvm_ss.o
	$(CORELINK) $^ $(LINKLIBS) -o $@ $(LDRELOC) $(CFLAGS)

bin/vexllvm_ss-static: $(OBJBASE:%=obj/%) obj/vexllvm_ss.o
	$(CORELINK) -static $^ $(LINKLIBS) -o $@ $(LDRELOC) $(CFLAGS)

bin/vexllvm_ss_rebase: $(OBJBASE:%=obj/%) obj/vexllvm_ss.o
	$(CORELINK) $^ $(LINKLIBS) -o $@ $(LDRELOC2) $(CFLAGS)

bin/%_rebase: $(OBJDIRDEPS) $(FPDIRDEPS) obj/%.o
	$(CORELINK) $^ $(LINKLIBS) $(LLVMFLAGS) -o $@ $(LDRELOC2) $(CFLAGS)

bin/%: $(OBJDIRDEPS) $(FPDIRDEPS) obj/%.o
	$(CORELINK) $^ $(LINKLIBS) $(LLVMFLAGS) -o $@ $(LDRELOC) $(CFLAGS)

bin/softfloat/%_rebase: $(OBJDIRDEPS) $(SOFTFLOATDIRDEPS) obj/%.o
	$(CORELINK) $^ $(LINKLIBS) $(LLVMFLAGS) -o $@ $(LDRELOC2) $(CFLAGS)

bin/softfloat/%: $(OBJDIRDEPS) $(SOFTFLOATDIRDEPS) obj/%.o
	$(CORELINK) $^ $(LINKLIBS) $(LLVMFLAGS) -o $@ $(LDRELOC) $(CFLAGS)

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
	$(LLVMCC) $(LLVMCFLAGS0) -emit-llvm -O3 -c $< -o $@

obj/%.o: src/%.s
	gcc -c -o $@ $< $(CFLAGS0)

obj/%.o: src/%.cc src/%.h
	$(CORECC) -c -o $@ $< $(CFLAGS0) $(LLVMFLAGS0)

obj/%.o: src/%.cc
	$(CORECC) -c -o $@ $< $(CFLAGS0) $(LLVMFLAGS0)

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
	
tests-snapshot: bin/pt_run tests/snapshot-bin/threads tests/snapshot-bin/read-post
	tests/snapshot.sh

tests/snapshot-bin/read-post: tests/snapshot-src/read-post.c
	gcc -lpthread -O3 -o $@ $<

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
	RUNCMD=bin/elf_run GUEST_32_ARCH=1 ONLY_BUILTIN=1 BUSYBOX=1 REALARCH=`uname -m` EMUARCH=i686 REALPATH=tests/traces-bin EMUPATH=tests/traces-i386-bin OUTPATH=tests/traces-i386-out tests/traces.sh

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

