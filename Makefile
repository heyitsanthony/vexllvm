BIN_BASE="0xa000000"
BIN_BASE2="0xc000000"
CFLAGS=-g -O3
ifndef TRACE_CFLAGS
TRACE_CFLAGS=-g
endif
ifndef TRACECC
TRACECC=gcc
endif

# XXX, MAKES BINARY SIZE EXPLODE
LDRELOC="-Wl,-Ttext-segment=$(BIN_BASE)"
LDRELOC2="-Wl,-Ttext-segment=$(BIN_BASE2)"
#LDFLAGS=


OBJDEPS=	vexxlate.o		\
		vexstmt.o		\
		vexsb.o			\
		vexexpr.o		\
		vexop.o			\
		genllvm.o		\
		guestcpustate.o		\
		gueststate.o		\
		vex_dispatch.o		\
		syscalls.o		\
		syscallsmarshalled.o	\
		vexhelpers.o		\
		vexexec.o		\
		vexexecchk.o		\
		vexexecfastchk.o	\
		symbols.o		\
		elfimg.o		\
		dllib.o			\
		gueststateelf.o		\
		gueststateptimg.o	\
		guesttls.o		\
		ptimgchk.o		\
		elfsegment.o		\
#		libvex_amd64_helpers.o	\

ELFTRACEDEPS=	elf_trace.o

BITCODE_FILES=	bitcode/libvex_amd64_helpers.bc	\
		bitcode/vexops.bc

TRACEDEPS= nested_call strlen strrchr 	\
	print cmdline getenv 		\
	fwrite malloc memset 		\
	primes memset-large atoi	\
	strcpy qsort strstr		\
	sscanf time gettimeofday	\
	rand gettext uselocale		\
	getopt errno strerror strchrnul	\
	cmdline-many dlsym

OBJDIRDEPS=$(OBJDEPS:%=obj/%)
ELFTRACEDIRDEPS=$(ELFTRACEDEPS:%=obj/%)

#TODO: use better config options

VEXLIB="/usr/lib/valgrind/libvex-amd64-linux.a"
#LLVMCONFIG_PATH="/home/chz/src/llvm/llvm-2.6/Release/bin/llvm-config"
#CFLAGS += -DLLVM_VERSION_MAJOR=2 -DLLVM_VERSION_MINOR=6
LLVMCONFIG_PATH=llvm-config
LLVMLDFLAGS=$(shell $(LLVMCONFIG_PATH) --ldflags)
LLVM_FLAGS_ORIGINAL=$(shell $(LLVMCONFIG_PATH) --ldflags --cxxflags --libs all)
LLVMFLAGS:=$(shell echo "$(LLVM_FLAGS_ORIGINAL)" |  sed "s/-Woverloaded-virtual//;s/-fPIC//;s/-DNDEBUG//g;s/-O3/ /g;") -Wall

all:	bin/elf_trace bin/elf_run bin/jit_test \
	bitcode \
	bin/pt_run bin/pt_trace bin/pt_xchk \
	bin/pt_run_rebase bin/pt_xchk_rebase \
	bin/vexllvm.a

bitcode: $(BITCODE_FILES)

clean:
	rm -f obj/* bin/*

bin/vexllvm.a: $(OBJDIRDEPS)
	ar r $@ $^

tests/traces-bin/dlsym : tests/traces-obj/dlsym.o
	$(TRACECC) $(TRACE_CFLAGS) -ldl $< -o $@

tests/traces-bin/% : tests/traces-obj/%.o
	$(TRACECC) $(TRACE_CFLAGS) $< -o $@

tests/traces-obj/%.o: tests/traces-src/%.c
	$(TRACECC) $(TRACE_CFLAGS) -c -o $@ $<

bitcode/%.bc: support/%.c
	llvm-gcc -emit-llvm -O3 -c $< -o $@

tests: test-traces

tests-clean:
	rm -f tests/*-bin/* tests/*-obj/* tests/*-out/*

TRACEDEPS_PATH=$(TRACEDEPS:%=tests/traces-bin/%)
test-traces: all $(TRACEDEPS_PATH)
	tests/traces.sh


test-built-traces: all $(TRACEDEPS_PATH)
	ONLY_BUILTIN=1 tests/traces.sh


tests-pt_xchk: all $(TRACEDEPS_PATH)
	RUNCMD=bin/pt_xchk OUTPATH=tests/traces-xchk-out tests/traces.sh

tests-xchk_opcode: all $(TRACEDEPS_PATH)
	tests/opcodes.sh

tests-oprof: all
	TRACES_OPROFILE=1 tests/traces.sh

bin/pt_xchk: $(OBJDIRDEPS) obj/pt_xchk.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/elf_trace: $(OBJDIRDEPS) $(ELFTRACEDIRDEPS)
	g++ -o $@ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) $(LDRELOC)

bin/elf_run: $(OBJDIRDEPS) obj/elf_run.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/pt_run_rebase: $(OBJDIRDEPS) obj/pt_run.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC2)

bin/pt_xchk_rebase: $(OBJDIRDEPS) obj/pt_xchk.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC2)

bin/pt_run: $(OBJDIRDEPS) obj/pt_run.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/pt_trace: $(OBJDIRDEPS) obj/pt_trace.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)


bin/jit_test: $(OBJDIRDEPS) obj/jit_test.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

#obj/libvex_amd64_helpers.s: bitcode/libvex_amd64_helpers.bc
#	llc  $< -o $@

#obj/libvex_amd64_helpers.o:   obj/libvex_amd64_helpers.s
#	g++ $(CFLAGS)$(LLVMFLAGS)  -o $@ -c $<

obj/%.o: src/%.s
	gcc $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.cc src/%.h
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

obj/%.o: src/%.cc
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<
