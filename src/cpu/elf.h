#ifndef CPU_ELF_H
#define CPU_ELF_H

#define ARM_HWCAP_SWP       (1 << 0)
#define ARM_HWCAP_HALF      (1 << 1)
#define ARM_HWCAP_THUMB     (1 << 2)
#define ARM_HWCAP_26BIT     (1 << 3)	/* Play it safe */
#define ARM_HWCAP_FAST_MULT (1 << 4)
#define ARM_HWCAP_FPA       (1 << 5)
#define ARM_HWCAP_VFP       (1 << 6)
#define ARM_HWCAP_EDSP      (1 << 7)
#define ARM_HWCAP_JAVA      (1 << 8)
#define ARM_HWCAP_IWMMXT    (1 << 9)
#define ARM_HWCAP_CRUNCH    (1 << 10)
#define ARM_HWCAP_THUMBEE   (1 << 11)
#define ARM_HWCAP_NEON      (1 << 12)
#define ARM_HWCAP_VFPv3     (1 << 13)
#define ARM_HWCAP_VFPv3D16  (1 << 14)
#define ARM_HWCAP_TLS       (1 << 15)

#endif
