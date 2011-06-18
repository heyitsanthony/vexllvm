#ifndef ARCH_H
#define ARCH_H

namespace Arch {
enum Arch {
	Unknown,
	X86_64,
	ARM,
	I386
};

static inline Arch getHostArch(void)
{
	#if defined(__amd64__)
		return Arch::X86_64;
	#elif defined(__i386__)
		return Arch::I386;
	#elif defined(__arm__)
		return Arch::ARM;
	#else
		#error Unsupported Host Architecture
	#endif
}
}

#endif
