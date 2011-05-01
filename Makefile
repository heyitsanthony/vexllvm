CFLAGS=-g

OBJDEPS=	vexstmt.o	\
		vexsb.o		\
		vexexpr.o	\
		vexop.o		\
		genllvm.o	\
		gueststate.o	\
		vex_dispatch.o

ELFDEPS=	elfimg.o	\
		elfsegment.o	\
		elf_main.o


OBJDIRDEPS=$(OBJDEPS:%=obj/%)
ELFDIRDEPS=$(ELFDEPS:%=obj/%)

#TODO: use better config options

#VEXLIB="/usr/lib64/valgrind/libvex-amd64-linux.a"
VEXLIB="/usr/local/lib64/valgrind/libvex-amd64-linux.a"
LLVMLDFLAGS=$(shell llvm-config --ldflags)
LLVM_FLAGS_ORIGINAL=$(shell llvm-config --ldflags --cxxflags --libs core)
LLVMFLAGS:=$(shell echo "$(LLVM_FLAGS_ORIGINAL)" |  sed "s/-Woverloaded-virtual//;s/-fPIC//;s/-DNDEBUG//g") -Wall

all: bin/vex_test bin/elf_test

clean:
	rm -f obj/* bin/*

bin/vex_test: $(OBJDIRDEPS) obj/vex_test.o
	g++ $(CFLAGS)  $^ $(VEXLIB) $(LLVMFLAGS) -o $@

bin/elf_test: $(OBJDIRDEPS) $(ELFDIRDEPS)
	g++ $(CFLAGS)  $^ $(VEXLIB) $(LLVMFLAGS) -o $@

obj/%.o: src/%.s
	gcc $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.cc src/%.h
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

obj/%.o: src/%.cc
	g++ $(CFLAGS) $(LLVMFLAGS) -c -o $@ $<

	
