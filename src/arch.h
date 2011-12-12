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
		return X86_64;
	#elif defined(__i386__)
		return I386;
	#elif defined(__arm__)
		return ARM;
	#else
		#error Unsupported Host Architecture
	#endif
}
}

#endif
