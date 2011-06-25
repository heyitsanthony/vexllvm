#ifndef TRANSLATEDUTIL_H
#define TRANSLATEDUTIL_H
#include <endian.h>

/* make up an os version */
#define qemu_uname_release "2.6.38-8-generic"

/* mask strace */
#define do_strace false
#define print_syscall(...)
#define print_syscall_ret(...)
#define gdb_exit(...)

#ifdef __amd64
	#define HOST_LONG_SIZE	8
	#define HOST_LONG_BITS	64
#else
	#define HOST_LONG_SIZE	4
	#define HOST_LONG_BITS	32
#endif
/* we should do checks here to make it return EFAULT as necessary */
#define lock_user_struct(m, p, a, ...) (*(void**)&p = (void*)a)
#define unlock_user_struct(...)
#define lock_user_string(b) (char*)(b)
#define unlock_user_string(...)
#define lock_user(a, b, ...) (char*)(b)
#define unlock_user(...)
#define access_ok(...) true
/* These are made up numbers... */
#define VERIFY_READ	1
#define VERIFY_WRITE	2

/* these imply that the address space mapping is identity */
#define g2h(x) (void*)(uintptr_t)(x)
#define h2g(x) (abi_ulong)(uintptr_t)(x)

#define gemu_log(...)

/* same as PROT_xxx */
#define PAGE_READ      0x0001
#define PAGE_WRITE     0x0002
#define PAGE_EXEC      0x0004
#define PAGE_BITS      (PAGE_READ | PAGE_WRITE | PAGE_EXEC)
#define PAGE_VALID     0x0008
/* original state of the write flag (used when tracking self-modifying
   code */
#define PAGE_WRITE_ORG 0x0010
#if defined(CONFIG_BSD) && defined(CONFIG_USER_ONLY)
/* FIXME: Code that sets/uses this is broken and needs to go away.  */
#define PAGE_RESERVED  0x0020
#endif

/* oh poo, these need to record mappings... they don't right now */
#define target_munmap(a, l)		munmap((void*)a, l)
#define target_mprotect(a, l, p) 	mprotect((void*)a, l, p)
#define target_mremap(oa, ol, ns, f, na) \
	(abi_ulong)(uintptr_t)mremap((void*)oa, ol, ns, f, (void*)na)

#define tswapl(s) s
#define tswap16(s) s
#define tswap32(s) s
#define tswap64(s) s
#define tswap64s(s) *(s) = tswap64(*(s))
#define tswapls(s) *(s) = tswapl(*(s))

#define mmap_lock()
#define mmap_unlock()

#define copy_to_user(d, s, l) memcpy((void*)d, s, l)
#define __put_user(x, hptr)\
({\
    int size = sizeof(*hptr);\
    switch(size) {\
    case 1:\
        *(uint8_t *)(hptr) = (uint8_t)(typeof(*hptr))(x);\
        break;\
    case 2:\
        *(uint16_t *)(hptr) = tswap16((typeof(*hptr))(x));\
        break;\
    case 4:\
        *(uint32_t *)(hptr) = tswap32((typeof(*hptr))(x));\
        break;\
    case 8:\
        *(uint64_t *)(hptr) = tswap64((typeof(*hptr))(x));\
        break;\
    default:\
        abort();\
    }\
    0;\
})

#define __get_user(x, hptr) \
({\
    int size = sizeof(*hptr);\
    switch(size) {\
    case 1:\
        x = (typeof(*hptr))*(uint8_t *)(hptr);\
        break;\
    case 2:\
        x = (typeof(*hptr))tswap16(*(uint16_t *)(hptr));\
        break;\
    case 4:\
        x = (typeof(*hptr))tswap32(*(uint32_t *)(hptr));\
        break;\
    case 8:\
        x = (typeof(*hptr))tswap64(*(uint64_t *)(hptr));\
        break;\
    default:\
        /* avoid warning */\
        x = 0;\
        abort();\
    }\
    0;\
})
#ifdef TARGET_ABI64
/* icky bastards are doing weird stuff with native size
   chunks in do_socket call... maybe fix them later */
static inline int get_user_ual(socklen_t& v, abi_ulong a) {
	v = *(socklen_t*)a;
	return 0;
}
#endif
static inline int get_user_sal(abi_long& v, abi_ulong a) {
	v = *(abi_long*)a;
	return 0;
}
static inline int get_user_ual(abi_ulong& v, abi_ulong a) {
	v = *(abi_ulong*)a;
	return 0;
}
#ifndef TARGET_ABI64
static inline int get_user_ual(size_t& v, abi_ulong a) {
	v = *(abi_ulong*)a;
	return 0;
}
#endif
static inline int get_user_u8(char& v, abi_ulong a) {
	v = *(char*)a;
	return 0;
}
static inline int get_user_u8(abi_long& v, abi_ulong a) {
	v = *(char*)a;
	return 0;
}
#ifdef TARGET_ABI64
static inline int get_user_u8(int& v, abi_ulong a) {
	v = *(char*)a;
	return 0;
}
#endif
static inline int get_user_u32(unsigned int& v, abi_ulong a) {
	v = *(unsigned int*)a;
	return 0;
}
static inline int get_user_s32(int& v, abi_ulong a) {
	v = *(int*)a;
	return 0;
}
static inline int put_user_ual(abi_ulong v, abi_ulong a) {
	*(abi_ulong*)a = v;
	return 0;
}
static inline int put_user_sal(abi_long v, abi_ulong a) {
	*(abi_long*)a = v;
	return 0;
}
static inline int put_user_s32(int v, abi_ulong a) {
	*(int*)a = v;
	return 0;
}
static inline int put_user_u32(unsigned int v, abi_ulong a) {
	*(unsigned int*)a = v;
	return 0;
}
static inline int put_user_u16(unsigned short v, abi_ulong a) {
	*(unsigned short*)a = v;
	return 0;
}
static inline int put_user_s64(long long v, abi_ulong a) {
	*(long long*)a = v;
	return 0;
}
static inline int put_user_u8(abi_long v, abi_ulong a) {
	*(char*)a = v;
	return 0;
	
}
static int host_to_target_errno(int err);
abi_ulong mmap_find_vma_flags(abi_ulong start, abi_ulong size,
	int flags) 
{
	/* host page align? */
	void* res = mmap((void*)(uintptr_t)start, size, PROT_READ, 
		MAP_NORESERVE | flags, -1, 0);
	if(res != MAP_FAILED) {
		munmap(res, size);
		return (abi_ulong)(uintptr_t)res;
	}
	return (abi_ulong)-host_to_target_errno(errno);
}
void page_set_flags(target_ulong start, target_ulong end, 
	int flags)
{
	assert(!g_syscall_last_mapping && "only one map per syscall allowed");
	g_syscall_last_mapping = new GuestMem::Mapping(
		(void*)start, end - start, flags);
}

static char* path(char* p) {
	std::string path((char*)(uintptr_t)p);
	if(path.empty() || path[0] != '/')
		return (char*)(uintptr_t)p;
	path = Syscalls::chroot + path;
	g_to_delete.push_back(new char[path.size() + 1]);
	memcpy(g_to_delete.back(), path.c_str(), path.size() + 1);
	return g_to_delete.back();
}

/* we don't have signal handling and i have no idea if it is really in scope
   for doing something interesting, so i'm erring on the side of leaving
   enough of the qemu code here so we can mess with it as needed */
struct target_siginfo;
typedef struct target_siginfo target_siginfo_t;
int queue_signal(int sig, target_siginfo_t *info) {
	assert(!"signal propagation not implemented");
}
abi_ulong get_sp() {
	assert(!"fetching platform sp not implemented");	
}
/* a few signal handling definitions that came from qemu.h */
void host_to_target_siginfo(target_siginfo_t *tinfo, const siginfo_t *info);
void target_to_host_siginfo(siginfo_t *info, const target_siginfo_t *tinfo);
int target_to_host_signal(int sig);
int host_to_target_signal(int sig);
long do_sigreturn() {
	assert(!"signal return not implemented");	
}
long do_rt_sigreturn() {
	assert(!"signal rt return not implemented");
}
abi_long do_sigaltstack(abi_ulong uss_addr, abi_ulong uoss_addr, abi_ulong sp);

#define unlikely(x)     __builtin_expect((x),false)

/* strcpy isn't good enough for them */
static inline void pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0)
        return;

    for(;;) {
        c = *str++;
        if (c == 0 || q >= buf + buf_size - 1)
            break;
        *q++ = c;
    }
    *q = '\0';
}


#endif
