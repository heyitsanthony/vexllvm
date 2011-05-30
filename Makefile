BIN_BASE="0xa000000"
CFLAGS=-g -O3 -DBIN_BASE="$(BIN_BASE)"
ifndef TRACE_CFLAGS
TRACE_CFLAGS=-g
endif

# XXX, MAKES BINARY SIZE EXPLODE
LDFLAGS="-Wl,-Ttext-segment=$(BIN_BASE)"
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
		symbols.o		\
		elfimg.o		\
		dllib.o			\
		gueststateelf.o		\
		gueststateptimg.o	\
		guesttls.o		\
		ptimgchk.o		\
		elfsegment.o

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
LLVMLDFLAGS=$(shell llvm-config --ldflags)
LLVM_FLAGS_ORIGINAL=$(shell llvm-config --ldflags --cxxflags --libs all)
LLVMFLAGS:=$(shell echo "$(LLVM_FLAGS_ORIGINAL)" |  sed "s/-Woverloaded-virtual//;s/-fPIC//;s/-DNDEBUG//g;s/-O3/ /g;") -Wall $(LDFLAGS)

all: bin/elf_trace bin/jit_test bitcode bin/elf_run bin/pt_run bin/pt_trace bin/pt_xchk

bitcode: $(BITCODE_FILES)

clean:
	rm -f obj/* bin/*

tests/traces-bin/dlsym : tests/traces-obj/dlsym.o
	gcc $(TRACE_CFLAGS) -ldl $< -o $@

tests/traces-bin/% : tests/traces-obj/%.o
	gcc $(TRACE_CFLAGS) $< -o $@

tests/traces-obj/%.o: tests/traces-src/%.c
	gcc $(TRACE_CFLAGS) -c -o $@ $<

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

tests-oprof: all
	TRACES_OPROFILE=1 tests/traces.sh

bin/pt_xchk: $(OBJDIRDEPS) obj/pt_xchk.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/elf_trace: $(OBJDIRDEPS) $(ELFTRACEDIRDEPS)
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/elf_run: $(OBJDIRDEPS) obj/elf_run.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/pt_run: $(OBJDIRDEPS) obj/pt_run.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

bin/pt_trace: $(OBJDIRDEPS) obj/pt_trace.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)


bin/jit_test: $(OBJDIRDEPS) obj/jit_test.o
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)


obj/%.o: src/%.s
	gcc $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.cc src/%.h
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

obj/%.o: src/%.cc
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<
