#ifndef TRANSLATEDUTIL_H
#define TRANSLATEDUTIL_H
#include <endian.h>

#define VERIFY_READ	1
#define VERIFY_WRITE	2
#define lock_user_struct(...) true
#define unlock_user_struct(...)
#define lock_user(a, b, ...) (abi_ulong*)(b)
#define unlock_user(...)
#define gemu_log(...)
#define access_ok(...) true

/* these imply that the address space mapping is identity */
#define g2h(x) (void*)(uintptr_t)(x)
#define h2g(x) (abi_ulong)(uintptr_t)(x)

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


#define tswapl(s) s
#define tswap16(s) s
#define tswap32(s) s
#define tswap64(s) s

#define mmap_lock()
#define mmap_unlock()

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
static inline bool get_user_ual(abi_ulong& v, abi_ulong a) {
	v = *(abi_ulong*)a;
	return true;
}
static inline bool get_user_ual(size_t& v, abi_ulong a) {
	v = *(abi_ulong*)a;
	return true;
}

static inline bool get_user_u8(char& v, abi_ulong a) {
	v = *(char*)a;
	return true;
}
static inline bool get_user_u8(abi_long& v, abi_ulong a) {
	v = *(char*)a;
	return true;
}
static inline bool get_user_u32(abi_ulong& v, abi_ulong a) {
	v = *(abi_ulong*)a;
	return true;
}
static inline bool get_user_s32(abi_long& v, abi_ulong a) {
	v = *(abi_long*)a;
	return true;
}
static inline bool put_user_ual(abi_ulong& v, abi_ulong a) {
	*(abi_long*)a = v;
	return true;
}
static inline bool put_user_s32(abi_long v, abi_ulong a) {
	*(abi_long*)a = v;
	return true;
}
static inline bool put_user_u32(abi_ulong v, abi_ulong a) {
	*(abi_ulong*)a = v;
	return true;
	
}
static inline bool put_user_u8(abi_long v, abi_ulong a) {
	*(char*)a = v;
	return true;
	
}
typedef enum argtype {
    TYPE_NULL,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_PTRVOID, /* pointer on unknown data */
    TYPE_LONGLONG,
    TYPE_ULONGLONG,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_STRUCT,
} argtype;
static int host_to_target_errno(int err);
static inline abi_ulong mmap_find_vma_flags(abi_ulong start, abi_ulong size,
	int flags) 
{
	/* host page align? */
	void* res = mmap((void*)(uintptr_t)start, size, PROT_READ, 
		MAP_NORESERVE | flags, 0, 0);
	if(res != MAP_FAILED) {
		munmap(res, size);
		return (abi_ulong)(uintptr_t)res;
	}
	return (abi_ulong)-host_to_target_errno(errno);
}
void page_set_flags(target_ulong start, target_ulong end, int flags)
{
	assert(!g_syscall_last_mapping && "only one map per syscall allowed");
	g_syscall_last_mapping = new GuestMem::Mapping(
		(void*)start, end - start, flags);
}

#endif
