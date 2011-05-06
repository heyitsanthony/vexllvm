BIN_BASE="0x8000000"
CFLAGS=-g -DBIN_BASE="$(BIN_BASE)"
# XXX, MAKES BINARY SIZE EXPLODE
LDFLAGS="-Wl,-Ttext-segment=$(BIN_BASE)"
#LDFLAGS=


OBJDEPS=	vexxlate.o	\
		vexstmt.o	\
		vexsb.o		\
		vexexpr.o	\
		vexop.o		\
		genllvm.o	\
		guestcpustate.o	\
		gueststate.o	\
		vex_dispatch.o

ELFDEPS=	elfimg.o	\
		dllib.o		\
		elfsegment.o	\
		elf_main.o


OBJDIRDEPS=$(OBJDEPS:%=obj/%)
ELFDIRDEPS=$(ELFDEPS:%=obj/%)

#TODO: use better config options

#VEXLIB="/usr/lib64/valgrind/libvex-amd64-linux.a"
VEXLIB="/usr/local/lib64/valgrind/libvex-amd64-linux.a"
LLVMLDFLAGS=$(shell llvm-config --ldflags)
LLVM_FLAGS_ORIGINAL=$(shell llvm-config --ldflags --cxxflags --libs all)
LLVMFLAGS:=$(shell echo "$(LLVM_FLAGS_ORIGINAL)" |  sed "s/-Woverloaded-virtual//;s/-fPIC//;s/-DNDEBUG//g") -Wall $(LDFLAGS)

all: bin/elf_test

clean:
	rm -f obj/* bin/*

bin/elf_test: $(OBJDIRDEPS) $(ELFDIRDEPS)
	g++ $(CFLAGS) -ldl  $^ $(VEXLIB) $(LLVMFLAGS) -o $@ $(LDRELOC)

obj/%.o: src/%.s
	gcc $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.cc src/%.h
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

obj/%.o: src/%.cc
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

	
