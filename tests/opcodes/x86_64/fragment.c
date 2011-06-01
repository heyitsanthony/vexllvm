unsigned int xma[4] __attribute__((aligned(0x10))) = {0x92847232,0xAD2314DA,0xC412409A,0x0D219477 };
unsigned int xmb[4] __attribute__((aligned(0x10))) = {0x322AC6E1,0x82FFB313,0x6612788B,0x56A3B321 };
unsigned long foo = 0xF1263826472926452ULL;
unsigned long bar = 0x8AB2382017B23AC12ULL;
int main() {
	asm volatile("movdqa (%0), %%xmm0" : : "a" (&xma));
	asm volatile("movdqa (%0), %%xmm3" : : "a" (&xmb));
	asm volatile("nop" : : "a" (&foo), "b" (bar));
#ifdef _FRAG_UNOP
	asm volatile("X_UNOP REG1");
#elif _FRAG_BINOP
	asm volatile("X_BINOP REG1, REG2");
#elif _FRAG_TRIOPI
	asm volatile("X_TRIOPIMM $IMM, REG1, REG2");
#else
#error	No op type defined
#endif
	asm volatile("mov %rax, %rsi");
	asm volatile("mov $0xe7, %rax");
	asm volatile("mov $0x3c, %rsi");
	asm volatile("xor %rdi, %rdi");
	asm volatile("syscall");
}