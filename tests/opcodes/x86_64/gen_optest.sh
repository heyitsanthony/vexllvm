#!/bin/bash

for op in `cat tests/opcodes/x86_64/opcodes.txt`; do
	if [ ! -e tests/opcodes/x86_64/$op ]; then
		echo generating $op
		mkdir -p tests/opcodes/x86_64/$op
		for r1 in `cat tests/opcodes/x86_64/registers.txt`; do
			cat > "tests/opcodes/x86_64/$op/$r1.c"  <<InputComesFromHERE
unsigned int xma[4] __attribute__((aligned(0x10))) = {0x92847232,0xAD2314DA,0xC412409A,0x0D219477 };
unsigned int xmb[4] __attribute__((aligned(0x10))) = {0x322AC6E1,0x82FFB313,0x6612788B,0x56A3B321 };
unsigned long foo = 0xF1263826472926452ULL;
unsigned long bar = 0x8AB2382017B23AC12ULL;
int main() {
	asm volatile("movdqa (%0), %%xmm0" : : "a" (&xma));
	asm volatile("movdqa (%0), %%xmm3" : : "a" (&xmb));
	asm volatile("nop" : : "a" (&foo), "b" (bar));
        asm volatile("$op $r1");
        asm volatile("mov %rax, %rci");
        asm volatile("mov \$0xe7, %rax");
        asm volatile("mov \$0x3c, %rsi");
        asm volatile("xor %rdi, %rdi");
        asm volatile("syscall");
}
InputComesFromHERE
			gcc "tests/opcodes/x86_64/$op/$r1.c" -Os -o "tests/opcodes/x86_64/$op/$r1" &> /dev/null
			# if [ $? -ne 0 ]; then
			# 	echo "tests/opcodes/x86_64/$op/$r1 is bad"
			# 	cat "tests/opcodes/x86_64/$op/$r1.c"
			# fi
			rm "tests/opcodes/x86_64/$op/$r1.c"
			for r2 in `cat tests/opcodes/x86_64/registers.txt`; do
				cat > "tests/opcodes/x86_64/$op/$r1-$r2.c"  <<InputComesFromHERE
unsigned int xma[4] __attribute__((aligned(0x10))) = {0x92847232,0xAD2314DA,0xC412409A,0x0D219477 };
unsigned int xmb[4] __attribute__((aligned(0x10))) = {0x322AC6E1,0x82FFB313,0x6612788B,0x56A3B321 };
unsigned long foo = 0xF1263826472926452ULL;
unsigned long bar = 0x8AB2382017B23AC12ULL;
int main() {
	asm volatile("movdqa (%0), %%xmm0" : : "a" (&xma));
	asm volatile("movdqa (%0), %%xmm3" : : "a" (&xmb));
	asm volatile("nop" : : "a" (&foo), "b" (bar));
        asm volatile("$op $r1, $r2");
        asm volatile("mov %rax, %rcx");
        asm volatile("mov \$0xe7, %rax");
        asm volatile("mov \$0x3c, %rsi");
        asm volatile("xor %rdi, %rdi");
        asm volatile("syscall");
}
InputComesFromHERE
			gcc "tests/opcodes/x86_64/$op/$r1-$r2.c" -Os -o "tests/opcodes/x86_64/$op/$r1-$r2" &> /dev/null
			# if [ $? -ne 0 ]; then
			# 	echo "tests/opcodes/x86_64/$op/$r1-$r2 is bad"
			# 	cat "tests/opcodes/x86_64/$op/$r1-$r2.c"
			# fi
			rm "tests/opcodes/x86_64/$op/$r1-$r2.c"
			done
		done
	fi
done