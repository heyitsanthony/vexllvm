#if defined(SYS__llseek) && defined(TARGET_NR__llseek)
	SYSCALL_RELATION(_llseek, SYS__llseek, TARGET_NR__llseek)
#endif
#if defined(SYS__newselect) && defined(TARGET_NR__newselect)
	SYSCALL_RELATION(_newselect, SYS__newselect, TARGET_NR__newselect)
#endif
#if defined(SYS__sysctl) && defined(TARGET_NR__sysctl)
	SYSCALL_RELATION(_sysctl, SYS__sysctl, TARGET_NR__sysctl)
#endif
#if defined(SYS_accept) && defined(TARGET_NR_accept)
	SYSCALL_RELATION(accept, SYS_accept, TARGET_NR_accept)
#endif
#if defined(SYS_accept4) && defined(TARGET_NR_accept4)
	SYSCALL_RELATION(accept4, SYS_accept4, TARGET_NR_accept4)
#endif
#if defined(SYS_access) && defined(TARGET_NR_access)
	SYSCALL_RELATION(access, SYS_access, TARGET_NR_access)
#endif
#if defined(SYS_acct) && defined(TARGET_NR_acct)
	SYSCALL_RELATION(acct, SYS_acct, TARGET_NR_acct)
#endif
#if defined(SYS_add_key) && defined(TARGET_NR_add_key)
	SYSCALL_RELATION(add_key, SYS_add_key, TARGET_NR_add_key)
#endif
#if defined(SYS_adjtimex) && defined(TARGET_NR_adjtimex)
	SYSCALL_RELATION(adjtimex, SYS_adjtimex, TARGET_NR_adjtimex)
#endif
#if defined(SYS_afs_syscall) && defined(TARGET_NR_afs_syscall)
	SYSCALL_RELATION(afs_syscall, SYS_afs_syscall, TARGET_NR_afs_syscall)
#endif
#if defined(SYS_alarm) && defined(TARGET_NR_alarm)
	SYSCALL_RELATION(alarm, SYS_alarm, TARGET_NR_alarm)
#endif
#if defined(SYS_arch_prctl) && defined(TARGET_NR_arch_prctl)
	SYSCALL_RELATION(arch_prctl, SYS_arch_prctl, TARGET_NR_arch_prctl)
#endif
#if defined(SYS_arm_fadvise64_64) && defined(TARGET_NR_arm_fadvise64_64)
	SYSCALL_RELATION(arm_fadvise64_64, SYS_arm_fadvise64_64, TARGET_NR_arm_fadvise64_64)
#endif
#if defined(SYS_arm_sync_file_range) && defined(TARGET_NR_arm_sync_file_range)
	SYSCALL_RELATION(arm_sync_file_range, SYS_arm_sync_file_range, TARGET_NR_arm_sync_file_range)
#endif
#if defined(SYS_bdflush) && defined(TARGET_NR_bdflush)
	SYSCALL_RELATION(bdflush, SYS_bdflush, TARGET_NR_bdflush)
#endif
#if defined(SYS_bind) && defined(TARGET_NR_bind)
	SYSCALL_RELATION(bind, SYS_bind, TARGET_NR_bind)
#endif
#if defined(SYS_break) && defined(TARGET_NR_break)
	SYSCALL_RELATION(break, SYS_break, TARGET_NR_break)
#endif
#if defined(SYS_brk) && defined(TARGET_NR_brk)
	SYSCALL_RELATION(brk, SYS_brk, TARGET_NR_brk)
#endif
#if defined(SYS_capget) && defined(TARGET_NR_capget)
	SYSCALL_RELATION(capget, SYS_capget, TARGET_NR_capget)
#endif
#if defined(SYS_capset) && defined(TARGET_NR_capset)
	SYSCALL_RELATION(capset, SYS_capset, TARGET_NR_capset)
#endif
#if defined(SYS_chdir) && defined(TARGET_NR_chdir)
	SYSCALL_RELATION(chdir, SYS_chdir, TARGET_NR_chdir)
#endif
#if defined(SYS_chmod) && defined(TARGET_NR_chmod)
	SYSCALL_RELATION(chmod, SYS_chmod, TARGET_NR_chmod)
#endif
#if defined(SYS_chown) && defined(TARGET_NR_chown)
	SYSCALL_RELATION(chown, SYS_chown, TARGET_NR_chown)
#endif
#if defined(SYS_chown32) && defined(TARGET_NR_chown32)
	SYSCALL_RELATION(chown32, SYS_chown32, TARGET_NR_chown32)
#endif
#if defined(SYS_chroot) && defined(TARGET_NR_chroot)
	SYSCALL_RELATION(chroot, SYS_chroot, TARGET_NR_chroot)
#endif
#if defined(SYS_clock_getres) && defined(TARGET_NR_clock_getres)
	SYSCALL_RELATION(clock_getres, SYS_clock_getres, TARGET_NR_clock_getres)
#endif
#if defined(SYS_clock_gettime) && defined(TARGET_NR_clock_gettime)
	SYSCALL_RELATION(clock_gettime, SYS_clock_gettime, TARGET_NR_clock_gettime)
#endif
#if defined(SYS_clock_nanosleep) && defined(TARGET_NR_clock_nanosleep)
	SYSCALL_RELATION(clock_nanosleep, SYS_clock_nanosleep, TARGET_NR_clock_nanosleep)
#endif
#if defined(SYS_clock_settime) && defined(TARGET_NR_clock_settime)
	SYSCALL_RELATION(clock_settime, SYS_clock_settime, TARGET_NR_clock_settime)
#endif
#if defined(SYS_clone) && defined(TARGET_NR_clone)
	SYSCALL_RELATION(clone, SYS_clone, TARGET_NR_clone)
#endif
#if defined(SYS_close) && defined(TARGET_NR_close)
	SYSCALL_RELATION(close, SYS_close, TARGET_NR_close)
#endif
#if defined(SYS_connect) && defined(TARGET_NR_connect)
	SYSCALL_RELATION(connect, SYS_connect, TARGET_NR_connect)
#endif
#if defined(SYS_creat) && defined(TARGET_NR_creat)
	SYSCALL_RELATION(creat, SYS_creat, TARGET_NR_creat)
#endif
#if defined(SYS_create_module) && defined(TARGET_NR_create_module)
	SYSCALL_RELATION(create_module, SYS_create_module, TARGET_NR_create_module)
#endif
#if defined(SYS_delete_module) && defined(TARGET_NR_delete_module)
	SYSCALL_RELATION(delete_module, SYS_delete_module, TARGET_NR_delete_module)
#endif
#if defined(SYS_dup) && defined(TARGET_NR_dup)
	SYSCALL_RELATION(dup, SYS_dup, TARGET_NR_dup)
#endif
#if defined(SYS_dup2) && defined(TARGET_NR_dup2)
	SYSCALL_RELATION(dup2, SYS_dup2, TARGET_NR_dup2)
#endif
#if defined(SYS_dup3) && defined(TARGET_NR_dup3)
	SYSCALL_RELATION(dup3, SYS_dup3, TARGET_NR_dup3)
#endif
#if defined(SYS_epoll_create) && defined(TARGET_NR_epoll_create)
	SYSCALL_RELATION(epoll_create, SYS_epoll_create, TARGET_NR_epoll_create)
#endif
#if defined(SYS_epoll_create1) && defined(TARGET_NR_epoll_create1)
	SYSCALL_RELATION(epoll_create1, SYS_epoll_create1, TARGET_NR_epoll_create1)
#endif
#if defined(SYS_epoll_ctl) && defined(TARGET_NR_epoll_ctl)
	SYSCALL_RELATION(epoll_ctl, SYS_epoll_ctl, TARGET_NR_epoll_ctl)
#endif
#if defined(SYS_epoll_ctl_old) && defined(TARGET_NR_epoll_ctl_old)
	SYSCALL_RELATION(epoll_ctl_old, SYS_epoll_ctl_old, TARGET_NR_epoll_ctl_old)
#endif
#if defined(SYS_epoll_pwait) && defined(TARGET_NR_epoll_pwait)
	SYSCALL_RELATION(epoll_pwait, SYS_epoll_pwait, TARGET_NR_epoll_pwait)
#endif
#if defined(SYS_epoll_wait) && defined(TARGET_NR_epoll_wait)
	SYSCALL_RELATION(epoll_wait, SYS_epoll_wait, TARGET_NR_epoll_wait)
#endif
#if defined(SYS_epoll_wait_old) && defined(TARGET_NR_epoll_wait_old)
	SYSCALL_RELATION(epoll_wait_old, SYS_epoll_wait_old, TARGET_NR_epoll_wait_old)
#endif
#if defined(SYS_eventfd) && defined(TARGET_NR_eventfd)
	SYSCALL_RELATION(eventfd, SYS_eventfd, TARGET_NR_eventfd)
#endif
#if defined(SYS_eventfd2) && defined(TARGET_NR_eventfd2)
	SYSCALL_RELATION(eventfd2, SYS_eventfd2, TARGET_NR_eventfd2)
#endif
#if defined(SYS_execve) && defined(TARGET_NR_execve)
	SYSCALL_RELATION(execve, SYS_execve, TARGET_NR_execve)
#endif
#if defined(SYS_exit) && defined(TARGET_NR_exit)
	SYSCALL_RELATION(exit, SYS_exit, TARGET_NR_exit)
#endif
#if defined(SYS_exit_group) && defined(TARGET_NR_exit_group)
	SYSCALL_RELATION(exit_group, SYS_exit_group, TARGET_NR_exit_group)
#endif
#if defined(SYS_faccessat) && defined(TARGET_NR_faccessat)
	SYSCALL_RELATION(faccessat, SYS_faccessat, TARGET_NR_faccessat)
#endif
#if defined(SYS_fadvise64) && defined(TARGET_NR_fadvise64)
	SYSCALL_RELATION(fadvise64, SYS_fadvise64, TARGET_NR_fadvise64)
#endif
#if defined(SYS_fadvise64_64) && defined(TARGET_NR_fadvise64_64)
	SYSCALL_RELATION(fadvise64_64, SYS_fadvise64_64, TARGET_NR_fadvise64_64)
#endif
#if defined(SYS_fallocate) && defined(TARGET_NR_fallocate)
	SYSCALL_RELATION(fallocate, SYS_fallocate, TARGET_NR_fallocate)
#endif
#if defined(SYS_fchdir) && defined(TARGET_NR_fchdir)
	SYSCALL_RELATION(fchdir, SYS_fchdir, TARGET_NR_fchdir)
#endif
#if defined(SYS_fchmod) && defined(TARGET_NR_fchmod)
	SYSCALL_RELATION(fchmod, SYS_fchmod, TARGET_NR_fchmod)
#endif
#if defined(SYS_fchmodat) && defined(TARGET_NR_fchmodat)
	SYSCALL_RELATION(fchmodat, SYS_fchmodat, TARGET_NR_fchmodat)
#endif
#if defined(SYS_fchown) && defined(TARGET_NR_fchown)
	SYSCALL_RELATION(fchown, SYS_fchown, TARGET_NR_fchown)
#endif
#if defined(SYS_fchown32) && defined(TARGET_NR_fchown32)
	SYSCALL_RELATION(fchown32, SYS_fchown32, TARGET_NR_fchown32)
#endif
#if defined(SYS_fchownat) && defined(TARGET_NR_fchownat)
	SYSCALL_RELATION(fchownat, SYS_fchownat, TARGET_NR_fchownat)
#endif
#if defined(SYS_fcntl) && defined(TARGET_NR_fcntl)
	SYSCALL_RELATION(fcntl, SYS_fcntl, TARGET_NR_fcntl)
#endif
#if defined(SYS_fcntl64) && defined(TARGET_NR_fcntl64)
	SYSCALL_RELATION(fcntl64, SYS_fcntl64, TARGET_NR_fcntl64)
#endif
#if defined(SYS_fdatasync) && defined(TARGET_NR_fdatasync)
	SYSCALL_RELATION(fdatasync, SYS_fdatasync, TARGET_NR_fdatasync)
#endif
#if defined(SYS_fgetxattr) && defined(TARGET_NR_fgetxattr)
	SYSCALL_RELATION(fgetxattr, SYS_fgetxattr, TARGET_NR_fgetxattr)
#endif
#if defined(SYS_flistxattr) && defined(TARGET_NR_flistxattr)
	SYSCALL_RELATION(flistxattr, SYS_flistxattr, TARGET_NR_flistxattr)
#endif
#if defined(SYS_flock) && defined(TARGET_NR_flock)
	SYSCALL_RELATION(flock, SYS_flock, TARGET_NR_flock)
#endif
#if defined(SYS_fork) && defined(TARGET_NR_fork)
	SYSCALL_RELATION(fork, SYS_fork, TARGET_NR_fork)
#endif
#if defined(SYS_fremovexattr) && defined(TARGET_NR_fremovexattr)
	SYSCALL_RELATION(fremovexattr, SYS_fremovexattr, TARGET_NR_fremovexattr)
#endif
#if defined(SYS_fsetxattr) && defined(TARGET_NR_fsetxattr)
	SYSCALL_RELATION(fsetxattr, SYS_fsetxattr, TARGET_NR_fsetxattr)
#endif
#if defined(SYS_fstat) && defined(TARGET_NR_fstat)
	SYSCALL_RELATION(fstat, SYS_fstat, TARGET_NR_fstat)
#endif
#if defined(SYS_fstat64) && defined(TARGET_NR_fstat64)
	SYSCALL_RELATION(fstat64, SYS_fstat64, TARGET_NR_fstat64)
#endif
#if defined(SYS_fstatat64) && defined(TARGET_NR_fstatat64)
	SYSCALL_RELATION(fstatat64, SYS_fstatat64, TARGET_NR_fstatat64)
#endif
#if defined(SYS_fstatfs) && defined(TARGET_NR_fstatfs)
	SYSCALL_RELATION(fstatfs, SYS_fstatfs, TARGET_NR_fstatfs)
#endif
#if defined(SYS_fstatfs64) && defined(TARGET_NR_fstatfs64)
	SYSCALL_RELATION(fstatfs64, SYS_fstatfs64, TARGET_NR_fstatfs64)
#endif
#if defined(SYS_fsync) && defined(TARGET_NR_fsync)
	SYSCALL_RELATION(fsync, SYS_fsync, TARGET_NR_fsync)
#endif
#if defined(SYS_ftime) && defined(TARGET_NR_ftime)
	SYSCALL_RELATION(ftime, SYS_ftime, TARGET_NR_ftime)
#endif
#if defined(SYS_ftruncate) && defined(TARGET_NR_ftruncate)
	SYSCALL_RELATION(ftruncate, SYS_ftruncate, TARGET_NR_ftruncate)
#endif
#if defined(SYS_ftruncate64) && defined(TARGET_NR_ftruncate64)
	SYSCALL_RELATION(ftruncate64, SYS_ftruncate64, TARGET_NR_ftruncate64)
#endif
#if defined(SYS_futex) && defined(TARGET_NR_futex)
	SYSCALL_RELATION(futex, SYS_futex, TARGET_NR_futex)
#endif
#if defined(SYS_futimesat) && defined(TARGET_NR_futimesat)
	SYSCALL_RELATION(futimesat, SYS_futimesat, TARGET_NR_futimesat)
#endif
#if defined(SYS_get_kernel_syms) && defined(TARGET_NR_get_kernel_syms)
	SYSCALL_RELATION(get_kernel_syms, SYS_get_kernel_syms, TARGET_NR_get_kernel_syms)
#endif
#if defined(SYS_get_mempolicy) && defined(TARGET_NR_get_mempolicy)
	SYSCALL_RELATION(get_mempolicy, SYS_get_mempolicy, TARGET_NR_get_mempolicy)
#endif
#if defined(SYS_get_robust_list) && defined(TARGET_NR_get_robust_list)
	SYSCALL_RELATION(get_robust_list, SYS_get_robust_list, TARGET_NR_get_robust_list)
#endif
#if defined(SYS_get_thread_area) && defined(TARGET_NR_get_thread_area)
	SYSCALL_RELATION(get_thread_area, SYS_get_thread_area, TARGET_NR_get_thread_area)
#endif
#if defined(SYS_getcpu) && defined(TARGET_NR_getcpu)
	SYSCALL_RELATION(getcpu, SYS_getcpu, TARGET_NR_getcpu)
#endif
#if defined(SYS_getcwd) && defined(TARGET_NR_getcwd)
	SYSCALL_RELATION(getcwd, SYS_getcwd, TARGET_NR_getcwd)
#endif
#if defined(SYS_getdents) && defined(TARGET_NR_getdents)
	SYSCALL_RELATION(getdents, SYS_getdents, TARGET_NR_getdents)
#endif
#if defined(SYS_getdents64) && defined(TARGET_NR_getdents64)
	SYSCALL_RELATION(getdents64, SYS_getdents64, TARGET_NR_getdents64)
#endif
#if defined(SYS_getegid) && defined(TARGET_NR_getegid)
	SYSCALL_RELATION(getegid, SYS_getegid, TARGET_NR_getegid)
#endif
#if defined(SYS_getegid32) && defined(TARGET_NR_getegid32)
	SYSCALL_RELATION(getegid32, SYS_getegid32, TARGET_NR_getegid32)
#endif
#if defined(SYS_geteuid) && defined(TARGET_NR_geteuid)
	SYSCALL_RELATION(geteuid, SYS_geteuid, TARGET_NR_geteuid)
#endif
#if defined(SYS_geteuid32) && defined(TARGET_NR_geteuid32)
	SYSCALL_RELATION(geteuid32, SYS_geteuid32, TARGET_NR_geteuid32)
#endif
#if defined(SYS_getgid) && defined(TARGET_NR_getgid)
	SYSCALL_RELATION(getgid, SYS_getgid, TARGET_NR_getgid)
#endif
#if defined(SYS_getgid32) && defined(TARGET_NR_getgid32)
	SYSCALL_RELATION(getgid32, SYS_getgid32, TARGET_NR_getgid32)
#endif
#if defined(SYS_getgroups) && defined(TARGET_NR_getgroups)
	SYSCALL_RELATION(getgroups, SYS_getgroups, TARGET_NR_getgroups)
#endif
#if defined(SYS_getgroups32) && defined(TARGET_NR_getgroups32)
	SYSCALL_RELATION(getgroups32, SYS_getgroups32, TARGET_NR_getgroups32)
#endif
#if defined(SYS_getitimer) && defined(TARGET_NR_getitimer)
	SYSCALL_RELATION(getitimer, SYS_getitimer, TARGET_NR_getitimer)
#endif
#if defined(SYS_getpeername) && defined(TARGET_NR_getpeername)
	SYSCALL_RELATION(getpeername, SYS_getpeername, TARGET_NR_getpeername)
#endif
#if defined(SYS_getpgid) && defined(TARGET_NR_getpgid)
	SYSCALL_RELATION(getpgid, SYS_getpgid, TARGET_NR_getpgid)
#endif
#if defined(SYS_getpgrp) && defined(TARGET_NR_getpgrp)
	SYSCALL_RELATION(getpgrp, SYS_getpgrp, TARGET_NR_getpgrp)
#endif
#if defined(SYS_getpid) && defined(TARGET_NR_getpid)
	SYSCALL_RELATION(getpid, SYS_getpid, TARGET_NR_getpid)
#endif
#if defined(SYS_getpmsg) && defined(TARGET_NR_getpmsg)
	SYSCALL_RELATION(getpmsg, SYS_getpmsg, TARGET_NR_getpmsg)
#endif
#if defined(SYS_getppid) && defined(TARGET_NR_getppid)
	SYSCALL_RELATION(getppid, SYS_getppid, TARGET_NR_getppid)
#endif
#if defined(SYS_getpriority) && defined(TARGET_NR_getpriority)
	SYSCALL_RELATION(getpriority, SYS_getpriority, TARGET_NR_getpriority)
#endif
#if defined(SYS_getresgid) && defined(TARGET_NR_getresgid)
	SYSCALL_RELATION(getresgid, SYS_getresgid, TARGET_NR_getresgid)
#endif
#if defined(SYS_getresgid32) && defined(TARGET_NR_getresgid32)
	SYSCALL_RELATION(getresgid32, SYS_getresgid32, TARGET_NR_getresgid32)
#endif
#if defined(SYS_getresuid) && defined(TARGET_NR_getresuid)
	SYSCALL_RELATION(getresuid, SYS_getresuid, TARGET_NR_getresuid)
#endif
#if defined(SYS_getresuid32) && defined(TARGET_NR_getresuid32)
	SYSCALL_RELATION(getresuid32, SYS_getresuid32, TARGET_NR_getresuid32)
#endif
#if defined(SYS_getrlimit) && defined(TARGET_NR_getrlimit)
	SYSCALL_RELATION(getrlimit, SYS_getrlimit, TARGET_NR_getrlimit)
#endif
#if defined(SYS_getrusage) && defined(TARGET_NR_getrusage)
	SYSCALL_RELATION(getrusage, SYS_getrusage, TARGET_NR_getrusage)
#endif
#if defined(SYS_getsid) && defined(TARGET_NR_getsid)
	SYSCALL_RELATION(getsid, SYS_getsid, TARGET_NR_getsid)
#endif
#if defined(SYS_getsockname) && defined(TARGET_NR_getsockname)
	SYSCALL_RELATION(getsockname, SYS_getsockname, TARGET_NR_getsockname)
#endif
#if defined(SYS_getsockopt) && defined(TARGET_NR_getsockopt)
	SYSCALL_RELATION(getsockopt, SYS_getsockopt, TARGET_NR_getsockopt)
#endif
#if defined(SYS_gettid) && defined(TARGET_NR_gettid)
	SYSCALL_RELATION(gettid, SYS_gettid, TARGET_NR_gettid)
#endif
#if defined(SYS_gettimeofday) && defined(TARGET_NR_gettimeofday)
	SYSCALL_RELATION(gettimeofday, SYS_gettimeofday, TARGET_NR_gettimeofday)
#endif
#if defined(SYS_getuid) && defined(TARGET_NR_getuid)
	SYSCALL_RELATION(getuid, SYS_getuid, TARGET_NR_getuid)
#endif
#if defined(SYS_getuid32) && defined(TARGET_NR_getuid32)
	SYSCALL_RELATION(getuid32, SYS_getuid32, TARGET_NR_getuid32)
#endif
#if defined(SYS_getxattr) && defined(TARGET_NR_getxattr)
	SYSCALL_RELATION(getxattr, SYS_getxattr, TARGET_NR_getxattr)
#endif
#if defined(SYS_gtty) && defined(TARGET_NR_gtty)
	SYSCALL_RELATION(gtty, SYS_gtty, TARGET_NR_gtty)
#endif
#if defined(SYS_idle) && defined(TARGET_NR_idle)
	SYSCALL_RELATION(idle, SYS_idle, TARGET_NR_idle)
#endif
#if defined(SYS_init_module) && defined(TARGET_NR_init_module)
	SYSCALL_RELATION(init_module, SYS_init_module, TARGET_NR_init_module)
#endif
#if defined(SYS_inotify_add_watch) && defined(TARGET_NR_inotify_add_watch)
	SYSCALL_RELATION(inotify_add_watch, SYS_inotify_add_watch, TARGET_NR_inotify_add_watch)
#endif
#if defined(SYS_inotify_init) && defined(TARGET_NR_inotify_init)
	SYSCALL_RELATION(inotify_init, SYS_inotify_init, TARGET_NR_inotify_init)
#endif
#if defined(SYS_inotify_init1) && defined(TARGET_NR_inotify_init1)
	SYSCALL_RELATION(inotify_init1, SYS_inotify_init1, TARGET_NR_inotify_init1)
#endif
#if defined(SYS_inotify_rm_watch) && defined(TARGET_NR_inotify_rm_watch)
	SYSCALL_RELATION(inotify_rm_watch, SYS_inotify_rm_watch, TARGET_NR_inotify_rm_watch)
#endif
#if defined(SYS_io_cancel) && defined(TARGET_NR_io_cancel)
	SYSCALL_RELATION(io_cancel, SYS_io_cancel, TARGET_NR_io_cancel)
#endif
#if defined(SYS_io_destroy) && defined(TARGET_NR_io_destroy)
	SYSCALL_RELATION(io_destroy, SYS_io_destroy, TARGET_NR_io_destroy)
#endif
#if defined(SYS_io_getevents) && defined(TARGET_NR_io_getevents)
	SYSCALL_RELATION(io_getevents, SYS_io_getevents, TARGET_NR_io_getevents)
#endif
#if defined(SYS_io_setup) && defined(TARGET_NR_io_setup)
	SYSCALL_RELATION(io_setup, SYS_io_setup, TARGET_NR_io_setup)
#endif
#if defined(SYS_io_submit) && defined(TARGET_NR_io_submit)
	SYSCALL_RELATION(io_submit, SYS_io_submit, TARGET_NR_io_submit)
#endif
#if defined(SYS_ioctl) && defined(TARGET_NR_ioctl)
	SYSCALL_RELATION(ioctl, SYS_ioctl, TARGET_NR_ioctl)
#endif
#if defined(SYS_ioperm) && defined(TARGET_NR_ioperm)
	SYSCALL_RELATION(ioperm, SYS_ioperm, TARGET_NR_ioperm)
#endif
#if defined(SYS_iopl) && defined(TARGET_NR_iopl)
	SYSCALL_RELATION(iopl, SYS_iopl, TARGET_NR_iopl)
#endif
#if defined(SYS_ioprio_get) && defined(TARGET_NR_ioprio_get)
	SYSCALL_RELATION(ioprio_get, SYS_ioprio_get, TARGET_NR_ioprio_get)
#endif
#if defined(SYS_ioprio_set) && defined(TARGET_NR_ioprio_set)
	SYSCALL_RELATION(ioprio_set, SYS_ioprio_set, TARGET_NR_ioprio_set)
#endif
#if defined(SYS_ipc) && defined(TARGET_NR_ipc)
	SYSCALL_RELATION(ipc, SYS_ipc, TARGET_NR_ipc)
#endif
#if defined(SYS_kexec_load) && defined(TARGET_NR_kexec_load)
	SYSCALL_RELATION(kexec_load, SYS_kexec_load, TARGET_NR_kexec_load)
#endif
#if defined(SYS_keyctl) && defined(TARGET_NR_keyctl)
	SYSCALL_RELATION(keyctl, SYS_keyctl, TARGET_NR_keyctl)
#endif
#if defined(SYS_kill) && defined(TARGET_NR_kill)
	SYSCALL_RELATION(kill, SYS_kill, TARGET_NR_kill)
#endif
#if defined(SYS_lchown) && defined(TARGET_NR_lchown)
	SYSCALL_RELATION(lchown, SYS_lchown, TARGET_NR_lchown)
#endif
#if defined(SYS_lchown32) && defined(TARGET_NR_lchown32)
	SYSCALL_RELATION(lchown32, SYS_lchown32, TARGET_NR_lchown32)
#endif
#if defined(SYS_lgetxattr) && defined(TARGET_NR_lgetxattr)
	SYSCALL_RELATION(lgetxattr, SYS_lgetxattr, TARGET_NR_lgetxattr)
#endif
#if defined(SYS_link) && defined(TARGET_NR_link)
	SYSCALL_RELATION(link, SYS_link, TARGET_NR_link)
#endif
#if defined(SYS_linkat) && defined(TARGET_NR_linkat)
	SYSCALL_RELATION(linkat, SYS_linkat, TARGET_NR_linkat)
#endif
#if defined(SYS_listen) && defined(TARGET_NR_listen)
	SYSCALL_RELATION(listen, SYS_listen, TARGET_NR_listen)
#endif
#if defined(SYS_listxattr) && defined(TARGET_NR_listxattr)
	SYSCALL_RELATION(listxattr, SYS_listxattr, TARGET_NR_listxattr)
#endif
#if defined(SYS_llistxattr) && defined(TARGET_NR_llistxattr)
	SYSCALL_RELATION(llistxattr, SYS_llistxattr, TARGET_NR_llistxattr)
#endif
#if defined(SYS_lock) && defined(TARGET_NR_lock)
	SYSCALL_RELATION(lock, SYS_lock, TARGET_NR_lock)
#endif
#if defined(SYS_lookup_dcookie) && defined(TARGET_NR_lookup_dcookie)
	SYSCALL_RELATION(lookup_dcookie, SYS_lookup_dcookie, TARGET_NR_lookup_dcookie)
#endif
#if defined(SYS_lremovexattr) && defined(TARGET_NR_lremovexattr)
	SYSCALL_RELATION(lremovexattr, SYS_lremovexattr, TARGET_NR_lremovexattr)
#endif
#if defined(SYS_lseek) && defined(TARGET_NR_lseek)
	SYSCALL_RELATION(lseek, SYS_lseek, TARGET_NR_lseek)
#endif
#if defined(SYS_lsetxattr) && defined(TARGET_NR_lsetxattr)
	SYSCALL_RELATION(lsetxattr, SYS_lsetxattr, TARGET_NR_lsetxattr)
#endif
#if defined(SYS_lstat) && defined(TARGET_NR_lstat)
	SYSCALL_RELATION(lstat, SYS_lstat, TARGET_NR_lstat)
#endif
#if defined(SYS_lstat64) && defined(TARGET_NR_lstat64)
	SYSCALL_RELATION(lstat64, SYS_lstat64, TARGET_NR_lstat64)
#endif
#if defined(SYS_madvise) && defined(TARGET_NR_madvise)
	SYSCALL_RELATION(madvise, SYS_madvise, TARGET_NR_madvise)
#endif
#if defined(SYS_madvise1) && defined(TARGET_NR_madvise1)
	SYSCALL_RELATION(madvise1, SYS_madvise1, TARGET_NR_madvise1)
#endif
#if defined(SYS_mbind) && defined(TARGET_NR_mbind)
	SYSCALL_RELATION(mbind, SYS_mbind, TARGET_NR_mbind)
#endif
#if defined(SYS_migrate_pages) && defined(TARGET_NR_migrate_pages)
	SYSCALL_RELATION(migrate_pages, SYS_migrate_pages, TARGET_NR_migrate_pages)
#endif
#if defined(SYS_mincore) && defined(TARGET_NR_mincore)
	SYSCALL_RELATION(mincore, SYS_mincore, TARGET_NR_mincore)
#endif
#if defined(SYS_mkdir) && defined(TARGET_NR_mkdir)
	SYSCALL_RELATION(mkdir, SYS_mkdir, TARGET_NR_mkdir)
#endif
#if defined(SYS_mkdirat) && defined(TARGET_NR_mkdirat)
	SYSCALL_RELATION(mkdirat, SYS_mkdirat, TARGET_NR_mkdirat)
#endif
#if defined(SYS_mknod) && defined(TARGET_NR_mknod)
	SYSCALL_RELATION(mknod, SYS_mknod, TARGET_NR_mknod)
#endif
#if defined(SYS_mknodat) && defined(TARGET_NR_mknodat)
	SYSCALL_RELATION(mknodat, SYS_mknodat, TARGET_NR_mknodat)
#endif
#if defined(SYS_mlock) && defined(TARGET_NR_mlock)
	SYSCALL_RELATION(mlock, SYS_mlock, TARGET_NR_mlock)
#endif
#if defined(SYS_mlockall) && defined(TARGET_NR_mlockall)
	SYSCALL_RELATION(mlockall, SYS_mlockall, TARGET_NR_mlockall)
#endif
#if defined(SYS_mmap) && defined(TARGET_NR_mmap)
	SYSCALL_RELATION(mmap, SYS_mmap, TARGET_NR_mmap)
#endif
#if defined(SYS_mmap2) && defined(TARGET_NR_mmap2)
	SYSCALL_RELATION(mmap2, SYS_mmap2, TARGET_NR_mmap2)
#endif
#if defined(SYS_modify_ldt) && defined(TARGET_NR_modify_ldt)
	SYSCALL_RELATION(modify_ldt, SYS_modify_ldt, TARGET_NR_modify_ldt)
#endif
#if defined(SYS_mount) && defined(TARGET_NR_mount)
	SYSCALL_RELATION(mount, SYS_mount, TARGET_NR_mount)
#endif
#if defined(SYS_move_pages) && defined(TARGET_NR_move_pages)
	SYSCALL_RELATION(move_pages, SYS_move_pages, TARGET_NR_move_pages)
#endif
#if defined(SYS_mprotect) && defined(TARGET_NR_mprotect)
	SYSCALL_RELATION(mprotect, SYS_mprotect, TARGET_NR_mprotect)
#endif
#if defined(SYS_mpx) && defined(TARGET_NR_mpx)
	SYSCALL_RELATION(mpx, SYS_mpx, TARGET_NR_mpx)
#endif
#if defined(SYS_mq_getsetattr) && defined(TARGET_NR_mq_getsetattr)
	SYSCALL_RELATION(mq_getsetattr, SYS_mq_getsetattr, TARGET_NR_mq_getsetattr)
#endif
#if defined(SYS_mq_notify) && defined(TARGET_NR_mq_notify)
	SYSCALL_RELATION(mq_notify, SYS_mq_notify, TARGET_NR_mq_notify)
#endif
#if defined(SYS_mq_open) && defined(TARGET_NR_mq_open)
	SYSCALL_RELATION(mq_open, SYS_mq_open, TARGET_NR_mq_open)
#endif
#if defined(SYS_mq_timedreceive) && defined(TARGET_NR_mq_timedreceive)
	SYSCALL_RELATION(mq_timedreceive, SYS_mq_timedreceive, TARGET_NR_mq_timedreceive)
#endif
#if defined(SYS_mq_timedsend) && defined(TARGET_NR_mq_timedsend)
	SYSCALL_RELATION(mq_timedsend, SYS_mq_timedsend, TARGET_NR_mq_timedsend)
#endif
#if defined(SYS_mq_unlink) && defined(TARGET_NR_mq_unlink)
	SYSCALL_RELATION(mq_unlink, SYS_mq_unlink, TARGET_NR_mq_unlink)
#endif
#if defined(SYS_mremap) && defined(TARGET_NR_mremap)
	SYSCALL_RELATION(mremap, SYS_mremap, TARGET_NR_mremap)
#endif
#if defined(SYS_msgctl) && defined(TARGET_NR_msgctl)
	SYSCALL_RELATION(msgctl, SYS_msgctl, TARGET_NR_msgctl)
#endif
#if defined(SYS_msgget) && defined(TARGET_NR_msgget)
	SYSCALL_RELATION(msgget, SYS_msgget, TARGET_NR_msgget)
#endif
#if defined(SYS_msgrcv) && defined(TARGET_NR_msgrcv)
	SYSCALL_RELATION(msgrcv, SYS_msgrcv, TARGET_NR_msgrcv)
#endif
#if defined(SYS_msgsnd) && defined(TARGET_NR_msgsnd)
	SYSCALL_RELATION(msgsnd, SYS_msgsnd, TARGET_NR_msgsnd)
#endif
#if defined(SYS_msync) && defined(TARGET_NR_msync)
	SYSCALL_RELATION(msync, SYS_msync, TARGET_NR_msync)
#endif
#if defined(SYS_munlock) && defined(TARGET_NR_munlock)
	SYSCALL_RELATION(munlock, SYS_munlock, TARGET_NR_munlock)
#endif
#if defined(SYS_munlockall) && defined(TARGET_NR_munlockall)
	SYSCALL_RELATION(munlockall, SYS_munlockall, TARGET_NR_munlockall)
#endif
#if defined(SYS_munmap) && defined(TARGET_NR_munmap)
	SYSCALL_RELATION(munmap, SYS_munmap, TARGET_NR_munmap)
#endif
#if defined(SYS_nanosleep) && defined(TARGET_NR_nanosleep)
	SYSCALL_RELATION(nanosleep, SYS_nanosleep, TARGET_NR_nanosleep)
#endif
#if defined(SYS_newfstatat) && defined(TARGET_NR_newfstatat)
	SYSCALL_RELATION(newfstatat, SYS_newfstatat, TARGET_NR_newfstatat)
#endif
#if defined(SYS_nfsservctl) && defined(TARGET_NR_nfsservctl)
	SYSCALL_RELATION(nfsservctl, SYS_nfsservctl, TARGET_NR_nfsservctl)
#endif
#if defined(SYS_nice) && defined(TARGET_NR_nice)
	SYSCALL_RELATION(nice, SYS_nice, TARGET_NR_nice)
#endif
#if defined(SYS_oldfstat) && defined(TARGET_NR_oldfstat)
	SYSCALL_RELATION(oldfstat, SYS_oldfstat, TARGET_NR_oldfstat)
#endif
#if defined(SYS_oldlstat) && defined(TARGET_NR_oldlstat)
	SYSCALL_RELATION(oldlstat, SYS_oldlstat, TARGET_NR_oldlstat)
#endif
#if defined(SYS_oldolduname) && defined(TARGET_NR_oldolduname)
	SYSCALL_RELATION(oldolduname, SYS_oldolduname, TARGET_NR_oldolduname)
#endif
#if defined(SYS_oldstat) && defined(TARGET_NR_oldstat)
	SYSCALL_RELATION(oldstat, SYS_oldstat, TARGET_NR_oldstat)
#endif
#if defined(SYS_olduname) && defined(TARGET_NR_olduname)
	SYSCALL_RELATION(olduname, SYS_olduname, TARGET_NR_olduname)
#endif
#if defined(SYS_open) && defined(TARGET_NR_open)
	SYSCALL_RELATION(open, SYS_open, TARGET_NR_open)
#endif
#if defined(SYS_openat) && defined(TARGET_NR_openat)
	SYSCALL_RELATION(openat, SYS_openat, TARGET_NR_openat)
#endif
#if defined(SYS_pause) && defined(TARGET_NR_pause)
	SYSCALL_RELATION(pause, SYS_pause, TARGET_NR_pause)
#endif
#if defined(SYS_pciconfig_iobase) && defined(TARGET_NR_pciconfig_iobase)
	SYSCALL_RELATION(pciconfig_iobase, SYS_pciconfig_iobase, TARGET_NR_pciconfig_iobase)
#endif
#if defined(SYS_pciconfig_read) && defined(TARGET_NR_pciconfig_read)
	SYSCALL_RELATION(pciconfig_read, SYS_pciconfig_read, TARGET_NR_pciconfig_read)
#endif
#if defined(SYS_pciconfig_write) && defined(TARGET_NR_pciconfig_write)
	SYSCALL_RELATION(pciconfig_write, SYS_pciconfig_write, TARGET_NR_pciconfig_write)
#endif
#if defined(SYS_personality) && defined(TARGET_NR_personality)
	SYSCALL_RELATION(personality, SYS_personality, TARGET_NR_personality)
#endif
#if defined(SYS_pipe) && defined(TARGET_NR_pipe)
	SYSCALL_RELATION(pipe, SYS_pipe, TARGET_NR_pipe)
#endif
#if defined(SYS_pipe2) && defined(TARGET_NR_pipe2)
	SYSCALL_RELATION(pipe2, SYS_pipe2, TARGET_NR_pipe2)
#endif
#if defined(SYS_pivot_root) && defined(TARGET_NR_pivot_root)
	SYSCALL_RELATION(pivot_root, SYS_pivot_root, TARGET_NR_pivot_root)
#endif
#if defined(SYS_poll) && defined(TARGET_NR_poll)
	SYSCALL_RELATION(poll, SYS_poll, TARGET_NR_poll)
#endif
#if defined(SYS_ppoll) && defined(TARGET_NR_ppoll)
	SYSCALL_RELATION(ppoll, SYS_ppoll, TARGET_NR_ppoll)
#endif
#if defined(SYS_prctl) && defined(TARGET_NR_prctl)
	SYSCALL_RELATION(prctl, SYS_prctl, TARGET_NR_prctl)
#endif
#if defined(SYS_pread) && defined(TARGET_NR_pread)
	SYSCALL_RELATION(pread, SYS_pread, TARGET_NR_pread)
#endif
#if defined(SYS_pread64) && defined(TARGET_NR_pread64)
	SYSCALL_RELATION(pread64, SYS_pread64, TARGET_NR_pread64)
#endif
#if defined(SYS_prof) && defined(TARGET_NR_prof)
	SYSCALL_RELATION(prof, SYS_prof, TARGET_NR_prof)
#endif
#if defined(SYS_profil) && defined(TARGET_NR_profil)
	SYSCALL_RELATION(profil, SYS_profil, TARGET_NR_profil)
#endif
#if defined(SYS_pselect6) && defined(TARGET_NR_pselect6)
	SYSCALL_RELATION(pselect6, SYS_pselect6, TARGET_NR_pselect6)
#endif
#if defined(SYS_ptrace) && defined(TARGET_NR_ptrace)
	SYSCALL_RELATION(ptrace, SYS_ptrace, TARGET_NR_ptrace)
#endif
#if defined(SYS_putpmsg) && defined(TARGET_NR_putpmsg)
	SYSCALL_RELATION(putpmsg, SYS_putpmsg, TARGET_NR_putpmsg)
#endif
#if defined(SYS_pwrite) && defined(TARGET_NR_pwrite)
	SYSCALL_RELATION(pwrite, SYS_pwrite, TARGET_NR_pwrite)
#endif
#if defined(SYS_pwrite64) && defined(TARGET_NR_pwrite64)
	SYSCALL_RELATION(pwrite64, SYS_pwrite64, TARGET_NR_pwrite64)
#endif
#if defined(SYS_query_module) && defined(TARGET_NR_query_module)
	SYSCALL_RELATION(query_module, SYS_query_module, TARGET_NR_query_module)
#endif
#if defined(SYS_quotactl) && defined(TARGET_NR_quotactl)
	SYSCALL_RELATION(quotactl, SYS_quotactl, TARGET_NR_quotactl)
#endif
#if defined(SYS_read) && defined(TARGET_NR_read)
	SYSCALL_RELATION(read, SYS_read, TARGET_NR_read)
#endif
#if defined(SYS_readahead) && defined(TARGET_NR_readahead)
	SYSCALL_RELATION(readahead, SYS_readahead, TARGET_NR_readahead)
#endif
#if defined(SYS_readdir) && defined(TARGET_NR_readdir)
	SYSCALL_RELATION(readdir, SYS_readdir, TARGET_NR_readdir)
#endif
#if defined(SYS_readlink) && defined(TARGET_NR_readlink)
	SYSCALL_RELATION(readlink, SYS_readlink, TARGET_NR_readlink)
#endif
#if defined(SYS_readlinkat) && defined(TARGET_NR_readlinkat)
	SYSCALL_RELATION(readlinkat, SYS_readlinkat, TARGET_NR_readlinkat)
#endif
#if defined(SYS_readv) && defined(TARGET_NR_readv)
	SYSCALL_RELATION(readv, SYS_readv, TARGET_NR_readv)
#endif
#if defined(SYS_reboot) && defined(TARGET_NR_reboot)
	SYSCALL_RELATION(reboot, SYS_reboot, TARGET_NR_reboot)
#endif
#if defined(SYS_recv) && defined(TARGET_NR_recv)
	SYSCALL_RELATION(recv, SYS_recv, TARGET_NR_recv)
#endif
#if defined(SYS_recvfrom) && defined(TARGET_NR_recvfrom)
	SYSCALL_RELATION(recvfrom, SYS_recvfrom, TARGET_NR_recvfrom)
#endif
#if defined(SYS_recvmsg) && defined(TARGET_NR_recvmsg)
	SYSCALL_RELATION(recvmsg, SYS_recvmsg, TARGET_NR_recvmsg)
#endif
#if defined(SYS_remap_file_pages) && defined(TARGET_NR_remap_file_pages)
	SYSCALL_RELATION(remap_file_pages, SYS_remap_file_pages, TARGET_NR_remap_file_pages)
#endif
#if defined(SYS_removexattr) && defined(TARGET_NR_removexattr)
	SYSCALL_RELATION(removexattr, SYS_removexattr, TARGET_NR_removexattr)
#endif
#if defined(SYS_rename) && defined(TARGET_NR_rename)
	SYSCALL_RELATION(rename, SYS_rename, TARGET_NR_rename)
#endif
#if defined(SYS_renameat) && defined(TARGET_NR_renameat)
	SYSCALL_RELATION(renameat, SYS_renameat, TARGET_NR_renameat)
#endif
#if defined(SYS_request_key) && defined(TARGET_NR_request_key)
	SYSCALL_RELATION(request_key, SYS_request_key, TARGET_NR_request_key)
#endif
#if defined(SYS_restart_syscall) && defined(TARGET_NR_restart_syscall)
	SYSCALL_RELATION(restart_syscall, SYS_restart_syscall, TARGET_NR_restart_syscall)
#endif
#if defined(SYS_rmdir) && defined(TARGET_NR_rmdir)
	SYSCALL_RELATION(rmdir, SYS_rmdir, TARGET_NR_rmdir)
#endif
#if defined(SYS_rt_sigaction) && defined(TARGET_NR_rt_sigaction)
	SYSCALL_RELATION(rt_sigaction, SYS_rt_sigaction, TARGET_NR_rt_sigaction)
#endif
#if defined(SYS_rt_sigpending) && defined(TARGET_NR_rt_sigpending)
	SYSCALL_RELATION(rt_sigpending, SYS_rt_sigpending, TARGET_NR_rt_sigpending)
#endif
#if defined(SYS_rt_sigprocmask) && defined(TARGET_NR_rt_sigprocmask)
	SYSCALL_RELATION(rt_sigprocmask, SYS_rt_sigprocmask, TARGET_NR_rt_sigprocmask)
#endif
#if defined(SYS_rt_sigqueueinfo) && defined(TARGET_NR_rt_sigqueueinfo)
	SYSCALL_RELATION(rt_sigqueueinfo, SYS_rt_sigqueueinfo, TARGET_NR_rt_sigqueueinfo)
#endif
#if defined(SYS_rt_sigreturn) && defined(TARGET_NR_rt_sigreturn)
	SYSCALL_RELATION(rt_sigreturn, SYS_rt_sigreturn, TARGET_NR_rt_sigreturn)
#endif
#if defined(SYS_rt_sigsuspend) && defined(TARGET_NR_rt_sigsuspend)
	SYSCALL_RELATION(rt_sigsuspend, SYS_rt_sigsuspend, TARGET_NR_rt_sigsuspend)
#endif
#if defined(SYS_rt_sigtimedwait) && defined(TARGET_NR_rt_sigtimedwait)
	SYSCALL_RELATION(rt_sigtimedwait, SYS_rt_sigtimedwait, TARGET_NR_rt_sigtimedwait)
#endif
#if defined(SYS_sched_get_priority_max) && defined(TARGET_NR_sched_get_priority_max)
	SYSCALL_RELATION(sched_get_priority_max, SYS_sched_get_priority_max, TARGET_NR_sched_get_priority_max)
#endif
#if defined(SYS_sched_get_priority_min) && defined(TARGET_NR_sched_get_priority_min)
	SYSCALL_RELATION(sched_get_priority_min, SYS_sched_get_priority_min, TARGET_NR_sched_get_priority_min)
#endif
#if defined(SYS_sched_getaffinity) && defined(TARGET_NR_sched_getaffinity)
	SYSCALL_RELATION(sched_getaffinity, SYS_sched_getaffinity, TARGET_NR_sched_getaffinity)
#endif
#if defined(SYS_sched_getparam) && defined(TARGET_NR_sched_getparam)
	SYSCALL_RELATION(sched_getparam, SYS_sched_getparam, TARGET_NR_sched_getparam)
#endif
#if defined(SYS_sched_getscheduler) && defined(TARGET_NR_sched_getscheduler)
	SYSCALL_RELATION(sched_getscheduler, SYS_sched_getscheduler, TARGET_NR_sched_getscheduler)
#endif
#if defined(SYS_sched_rr_get_interval) && defined(TARGET_NR_sched_rr_get_interval)
	SYSCALL_RELATION(sched_rr_get_interval, SYS_sched_rr_get_interval, TARGET_NR_sched_rr_get_interval)
#endif
#if defined(SYS_sched_setaffinity) && defined(TARGET_NR_sched_setaffinity)
	SYSCALL_RELATION(sched_setaffinity, SYS_sched_setaffinity, TARGET_NR_sched_setaffinity)
#endif
#if defined(SYS_sched_setparam) && defined(TARGET_NR_sched_setparam)
	SYSCALL_RELATION(sched_setparam, SYS_sched_setparam, TARGET_NR_sched_setparam)
#endif
#if defined(SYS_sched_setscheduler) && defined(TARGET_NR_sched_setscheduler)
	SYSCALL_RELATION(sched_setscheduler, SYS_sched_setscheduler, TARGET_NR_sched_setscheduler)
#endif
#if defined(SYS_sched_yield) && defined(TARGET_NR_sched_yield)
	SYSCALL_RELATION(sched_yield, SYS_sched_yield, TARGET_NR_sched_yield)
#endif
#if defined(SYS_security) && defined(TARGET_NR_security)
	SYSCALL_RELATION(security, SYS_security, TARGET_NR_security)
#endif
#if defined(SYS_select) && defined(TARGET_NR_select)
	SYSCALL_RELATION(select, SYS_select, TARGET_NR_select)
#endif
#if defined(SYS_semctl) && defined(TARGET_NR_semctl)
	SYSCALL_RELATION(semctl, SYS_semctl, TARGET_NR_semctl)
#endif
#if defined(SYS_semget) && defined(TARGET_NR_semget)
	SYSCALL_RELATION(semget, SYS_semget, TARGET_NR_semget)
#endif
#if defined(SYS_semop) && defined(TARGET_NR_semop)
	SYSCALL_RELATION(semop, SYS_semop, TARGET_NR_semop)
#endif
#if defined(SYS_semtimedop) && defined(TARGET_NR_semtimedop)
	SYSCALL_RELATION(semtimedop, SYS_semtimedop, TARGET_NR_semtimedop)
#endif
#if defined(SYS_send) && defined(TARGET_NR_send)
	SYSCALL_RELATION(send, SYS_send, TARGET_NR_send)
#endif
#if defined(SYS_sendfile) && defined(TARGET_NR_sendfile)
	SYSCALL_RELATION(sendfile, SYS_sendfile, TARGET_NR_sendfile)
#endif
#if defined(SYS_sendfile64) && defined(TARGET_NR_sendfile64)
	SYSCALL_RELATION(sendfile64, SYS_sendfile64, TARGET_NR_sendfile64)
#endif
#if defined(SYS_sendmsg) && defined(TARGET_NR_sendmsg)
	SYSCALL_RELATION(sendmsg, SYS_sendmsg, TARGET_NR_sendmsg)
#endif
#if defined(SYS_sendto) && defined(TARGET_NR_sendto)
	SYSCALL_RELATION(sendto, SYS_sendto, TARGET_NR_sendto)
#endif
#if defined(SYS_set_mempolicy) && defined(TARGET_NR_set_mempolicy)
	SYSCALL_RELATION(set_mempolicy, SYS_set_mempolicy, TARGET_NR_set_mempolicy)
#endif
#if defined(SYS_set_robust_list) && defined(TARGET_NR_set_robust_list)
	SYSCALL_RELATION(set_robust_list, SYS_set_robust_list, TARGET_NR_set_robust_list)
#endif
#if defined(SYS_set_thread_area) && defined(TARGET_NR_set_thread_area)
	SYSCALL_RELATION(set_thread_area, SYS_set_thread_area, TARGET_NR_set_thread_area)
#endif
#if defined(SYS_set_tid_address) && defined(TARGET_NR_set_tid_address)
	SYSCALL_RELATION(set_tid_address, SYS_set_tid_address, TARGET_NR_set_tid_address)
#endif
#if defined(SYS_setdomainname) && defined(TARGET_NR_setdomainname)
	SYSCALL_RELATION(setdomainname, SYS_setdomainname, TARGET_NR_setdomainname)
#endif
#if defined(SYS_setfsgid) && defined(TARGET_NR_setfsgid)
	SYSCALL_RELATION(setfsgid, SYS_setfsgid, TARGET_NR_setfsgid)
#endif
#if defined(SYS_setfsgid32) && defined(TARGET_NR_setfsgid32)
	SYSCALL_RELATION(setfsgid32, SYS_setfsgid32, TARGET_NR_setfsgid32)
#endif
#if defined(SYS_setfsuid) && defined(TARGET_NR_setfsuid)
	SYSCALL_RELATION(setfsuid, SYS_setfsuid, TARGET_NR_setfsuid)
#endif
#if defined(SYS_setfsuid32) && defined(TARGET_NR_setfsuid32)
	SYSCALL_RELATION(setfsuid32, SYS_setfsuid32, TARGET_NR_setfsuid32)
#endif
#if defined(SYS_setgid) && defined(TARGET_NR_setgid)
	SYSCALL_RELATION(setgid, SYS_setgid, TARGET_NR_setgid)
#endif
#if defined(SYS_setgid32) && defined(TARGET_NR_setgid32)
	SYSCALL_RELATION(setgid32, SYS_setgid32, TARGET_NR_setgid32)
#endif
#if defined(SYS_setgroups) && defined(TARGET_NR_setgroups)
	SYSCALL_RELATION(setgroups, SYS_setgroups, TARGET_NR_setgroups)
#endif
#if defined(SYS_setgroups32) && defined(TARGET_NR_setgroups32)
	SYSCALL_RELATION(setgroups32, SYS_setgroups32, TARGET_NR_setgroups32)
#endif
#if defined(SYS_sethostname) && defined(TARGET_NR_sethostname)
	SYSCALL_RELATION(sethostname, SYS_sethostname, TARGET_NR_sethostname)
#endif
#if defined(SYS_setitimer) && defined(TARGET_NR_setitimer)
	SYSCALL_RELATION(setitimer, SYS_setitimer, TARGET_NR_setitimer)
#endif
#if defined(SYS_setpgid) && defined(TARGET_NR_setpgid)
	SYSCALL_RELATION(setpgid, SYS_setpgid, TARGET_NR_setpgid)
#endif
#if defined(SYS_setpriority) && defined(TARGET_NR_setpriority)
	SYSCALL_RELATION(setpriority, SYS_setpriority, TARGET_NR_setpriority)
#endif
#if defined(SYS_setregid) && defined(TARGET_NR_setregid)
	SYSCALL_RELATION(setregid, SYS_setregid, TARGET_NR_setregid)
#endif
#if defined(SYS_setregid32) && defined(TARGET_NR_setregid32)
	SYSCALL_RELATION(setregid32, SYS_setregid32, TARGET_NR_setregid32)
#endif
#if defined(SYS_setresgid) && defined(TARGET_NR_setresgid)
	SYSCALL_RELATION(setresgid, SYS_setresgid, TARGET_NR_setresgid)
#endif
#if defined(SYS_setresgid32) && defined(TARGET_NR_setresgid32)
	SYSCALL_RELATION(setresgid32, SYS_setresgid32, TARGET_NR_setresgid32)
#endif
#if defined(SYS_setresuid) && defined(TARGET_NR_setresuid)
	SYSCALL_RELATION(setresuid, SYS_setresuid, TARGET_NR_setresuid)
#endif
#if defined(SYS_setresuid32) && defined(TARGET_NR_setresuid32)
	SYSCALL_RELATION(setresuid32, SYS_setresuid32, TARGET_NR_setresuid32)
#endif
#if defined(SYS_setreuid) && defined(TARGET_NR_setreuid)
	SYSCALL_RELATION(setreuid, SYS_setreuid, TARGET_NR_setreuid)
#endif
#if defined(SYS_setreuid32) && defined(TARGET_NR_setreuid32)
	SYSCALL_RELATION(setreuid32, SYS_setreuid32, TARGET_NR_setreuid32)
#endif
#if defined(SYS_setrlimit) && defined(TARGET_NR_setrlimit)
	SYSCALL_RELATION(setrlimit, SYS_setrlimit, TARGET_NR_setrlimit)
#endif
#if defined(SYS_setsid) && defined(TARGET_NR_setsid)
	SYSCALL_RELATION(setsid, SYS_setsid, TARGET_NR_setsid)
#endif
#if defined(SYS_setsockopt) && defined(TARGET_NR_setsockopt)
	SYSCALL_RELATION(setsockopt, SYS_setsockopt, TARGET_NR_setsockopt)
#endif
#if defined(SYS_settimeofday) && defined(TARGET_NR_settimeofday)
	SYSCALL_RELATION(settimeofday, SYS_settimeofday, TARGET_NR_settimeofday)
#endif
#if defined(SYS_setuid) && defined(TARGET_NR_setuid)
	SYSCALL_RELATION(setuid, SYS_setuid, TARGET_NR_setuid)
#endif
#if defined(SYS_setuid32) && defined(TARGET_NR_setuid32)
	SYSCALL_RELATION(setuid32, SYS_setuid32, TARGET_NR_setuid32)
#endif
#if defined(SYS_setxattr) && defined(TARGET_NR_setxattr)
	SYSCALL_RELATION(setxattr, SYS_setxattr, TARGET_NR_setxattr)
#endif
#if defined(SYS_sgetmask) && defined(TARGET_NR_sgetmask)
	SYSCALL_RELATION(sgetmask, SYS_sgetmask, TARGET_NR_sgetmask)
#endif
#if defined(SYS_shmat) && defined(TARGET_NR_shmat)
	SYSCALL_RELATION(shmat, SYS_shmat, TARGET_NR_shmat)
#endif
#if defined(SYS_shmctl) && defined(TARGET_NR_shmctl)
	SYSCALL_RELATION(shmctl, SYS_shmctl, TARGET_NR_shmctl)
#endif
#if defined(SYS_shmdt) && defined(TARGET_NR_shmdt)
	SYSCALL_RELATION(shmdt, SYS_shmdt, TARGET_NR_shmdt)
#endif
#if defined(SYS_shmget) && defined(TARGET_NR_shmget)
	SYSCALL_RELATION(shmget, SYS_shmget, TARGET_NR_shmget)
#endif
#if defined(SYS_shutdown) && defined(TARGET_NR_shutdown)
	SYSCALL_RELATION(shutdown, SYS_shutdown, TARGET_NR_shutdown)
#endif
#if defined(SYS_sigaction) && defined(TARGET_NR_sigaction)
	SYSCALL_RELATION(sigaction, SYS_sigaction, TARGET_NR_sigaction)
#endif
#if defined(SYS_sigaltstack) && defined(TARGET_NR_sigaltstack)
	SYSCALL_RELATION(sigaltstack, SYS_sigaltstack, TARGET_NR_sigaltstack)
#endif
#if defined(SYS_signal) && defined(TARGET_NR_signal)
	SYSCALL_RELATION(signal, SYS_signal, TARGET_NR_signal)
#endif
#if defined(SYS_signalfd) && defined(TARGET_NR_signalfd)
	SYSCALL_RELATION(signalfd, SYS_signalfd, TARGET_NR_signalfd)
#endif
#if defined(SYS_signalfd4) && defined(TARGET_NR_signalfd4)
	SYSCALL_RELATION(signalfd4, SYS_signalfd4, TARGET_NR_signalfd4)
#endif
#if defined(SYS_sigpending) && defined(TARGET_NR_sigpending)
	SYSCALL_RELATION(sigpending, SYS_sigpending, TARGET_NR_sigpending)
#endif
#if defined(SYS_sigprocmask) && defined(TARGET_NR_sigprocmask)
	SYSCALL_RELATION(sigprocmask, SYS_sigprocmask, TARGET_NR_sigprocmask)
#endif
#if defined(SYS_sigreturn) && defined(TARGET_NR_sigreturn)
	SYSCALL_RELATION(sigreturn, SYS_sigreturn, TARGET_NR_sigreturn)
#endif
#if defined(SYS_sigsuspend) && defined(TARGET_NR_sigsuspend)
	SYSCALL_RELATION(sigsuspend, SYS_sigsuspend, TARGET_NR_sigsuspend)
#endif
#if defined(SYS_socket) && defined(TARGET_NR_socket)
	SYSCALL_RELATION(socket, SYS_socket, TARGET_NR_socket)
#endif
#if defined(SYS_socketcall) && defined(TARGET_NR_socketcall)
	SYSCALL_RELATION(socketcall, SYS_socketcall, TARGET_NR_socketcall)
#endif
#if defined(SYS_socketpair) && defined(TARGET_NR_socketpair)
	SYSCALL_RELATION(socketpair, SYS_socketpair, TARGET_NR_socketpair)
#endif
#if defined(SYS_splice) && defined(TARGET_NR_splice)
	SYSCALL_RELATION(splice, SYS_splice, TARGET_NR_splice)
#endif
#if defined(SYS_ssetmask) && defined(TARGET_NR_ssetmask)
	SYSCALL_RELATION(ssetmask, SYS_ssetmask, TARGET_NR_ssetmask)
#endif
#if defined(SYS_stat) && defined(TARGET_NR_stat)
	SYSCALL_RELATION(stat, SYS_stat, TARGET_NR_stat)
#endif
#if defined(SYS_stat64) && defined(TARGET_NR_stat64)
	SYSCALL_RELATION(stat64, SYS_stat64, TARGET_NR_stat64)
#endif
#if defined(SYS_statfs) && defined(TARGET_NR_statfs)
	SYSCALL_RELATION(statfs, SYS_statfs, TARGET_NR_statfs)
#endif
#if defined(SYS_statfs64) && defined(TARGET_NR_statfs64)
	SYSCALL_RELATION(statfs64, SYS_statfs64, TARGET_NR_statfs64)
#endif
#if defined(SYS_stime) && defined(TARGET_NR_stime)
	SYSCALL_RELATION(stime, SYS_stime, TARGET_NR_stime)
#endif
#if defined(SYS_stty) && defined(TARGET_NR_stty)
	SYSCALL_RELATION(stty, SYS_stty, TARGET_NR_stty)
#endif
#if defined(SYS_swapoff) && defined(TARGET_NR_swapoff)
	SYSCALL_RELATION(swapoff, SYS_swapoff, TARGET_NR_swapoff)
#endif
#if defined(SYS_swapon) && defined(TARGET_NR_swapon)
	SYSCALL_RELATION(swapon, SYS_swapon, TARGET_NR_swapon)
#endif
#if defined(SYS_symlink) && defined(TARGET_NR_symlink)
	SYSCALL_RELATION(symlink, SYS_symlink, TARGET_NR_symlink)
#endif
#if defined(SYS_symlinkat) && defined(TARGET_NR_symlinkat)
	SYSCALL_RELATION(symlinkat, SYS_symlinkat, TARGET_NR_symlinkat)
#endif
#if defined(SYS_sync) && defined(TARGET_NR_sync)
	SYSCALL_RELATION(sync, SYS_sync, TARGET_NR_sync)
#endif
#if defined(SYS_sync_file_range) && defined(TARGET_NR_sync_file_range)
	SYSCALL_RELATION(sync_file_range, SYS_sync_file_range, TARGET_NR_sync_file_range)
#endif
#if defined(SYS_sync_file_range2) && defined(TARGET_NR_sync_file_range2)
	SYSCALL_RELATION(sync_file_range2, SYS_sync_file_range2, TARGET_NR_sync_file_range2)
#endif
#if defined(SYS_syscall) && defined(TARGET_NR_syscall)
	SYSCALL_RELATION(syscall, SYS_syscall, TARGET_NR_syscall)
#endif
#if defined(SYS_sysfs) && defined(TARGET_NR_sysfs)
	SYSCALL_RELATION(sysfs, SYS_sysfs, TARGET_NR_sysfs)
#endif
#if defined(SYS_sysinfo) && defined(TARGET_NR_sysinfo)
	SYSCALL_RELATION(sysinfo, SYS_sysinfo, TARGET_NR_sysinfo)
#endif
#if defined(SYS_syslog) && defined(TARGET_NR_syslog)
	SYSCALL_RELATION(syslog, SYS_syslog, TARGET_NR_syslog)
#endif
#if defined(SYS_tee) && defined(TARGET_NR_tee)
	SYSCALL_RELATION(tee, SYS_tee, TARGET_NR_tee)
#endif
#if defined(SYS_tgkill) && defined(TARGET_NR_tgkill)
	SYSCALL_RELATION(tgkill, SYS_tgkill, TARGET_NR_tgkill)
#endif
#if defined(SYS_time) && defined(TARGET_NR_time)
	SYSCALL_RELATION(time, SYS_time, TARGET_NR_time)
#endif
#if defined(SYS_timer_create) && defined(TARGET_NR_timer_create)
	SYSCALL_RELATION(timer_create, SYS_timer_create, TARGET_NR_timer_create)
#endif
#if defined(SYS_timer_delete) && defined(TARGET_NR_timer_delete)
	SYSCALL_RELATION(timer_delete, SYS_timer_delete, TARGET_NR_timer_delete)
#endif
#if defined(SYS_timer_getoverrun) && defined(TARGET_NR_timer_getoverrun)
	SYSCALL_RELATION(timer_getoverrun, SYS_timer_getoverrun, TARGET_NR_timer_getoverrun)
#endif
#if defined(SYS_timer_gettime) && defined(TARGET_NR_timer_gettime)
	SYSCALL_RELATION(timer_gettime, SYS_timer_gettime, TARGET_NR_timer_gettime)
#endif
#if defined(SYS_timer_settime) && defined(TARGET_NR_timer_settime)
	SYSCALL_RELATION(timer_settime, SYS_timer_settime, TARGET_NR_timer_settime)
#endif
#if defined(SYS_timerfd) && defined(TARGET_NR_timerfd)
	SYSCALL_RELATION(timerfd, SYS_timerfd, TARGET_NR_timerfd)
#endif
#if defined(SYS_timerfd_gettime) && defined(TARGET_NR_timerfd_gettime)
	SYSCALL_RELATION(timerfd_gettime, SYS_timerfd_gettime, TARGET_NR_timerfd_gettime)
#endif
#if defined(SYS_timerfd_settime) && defined(TARGET_NR_timerfd_settime)
	SYSCALL_RELATION(timerfd_settime, SYS_timerfd_settime, TARGET_NR_timerfd_settime)
#endif
#if defined(SYS_times) && defined(TARGET_NR_times)
	SYSCALL_RELATION(times, SYS_times, TARGET_NR_times)
#endif
#if defined(SYS_tkill) && defined(TARGET_NR_tkill)
	SYSCALL_RELATION(tkill, SYS_tkill, TARGET_NR_tkill)
#endif
#if defined(SYS_truncate) && defined(TARGET_NR_truncate)
	SYSCALL_RELATION(truncate, SYS_truncate, TARGET_NR_truncate)
#endif
#if defined(SYS_truncate64) && defined(TARGET_NR_truncate64)
	SYSCALL_RELATION(truncate64, SYS_truncate64, TARGET_NR_truncate64)
#endif
#if defined(SYS_tuxcall) && defined(TARGET_NR_tuxcall)
	SYSCALL_RELATION(tuxcall, SYS_tuxcall, TARGET_NR_tuxcall)
#endif
#if defined(SYS_ugetrlimit) && defined(TARGET_NR_ugetrlimit)
	SYSCALL_RELATION(ugetrlimit, SYS_ugetrlimit, TARGET_NR_ugetrlimit)
#endif
#if defined(SYS_ulimit) && defined(TARGET_NR_ulimit)
	SYSCALL_RELATION(ulimit, SYS_ulimit, TARGET_NR_ulimit)
#endif
#if defined(SYS_umask) && defined(TARGET_NR_umask)
	SYSCALL_RELATION(umask, SYS_umask, TARGET_NR_umask)
#endif
#if defined(SYS_umount) && defined(TARGET_NR_umount)
	SYSCALL_RELATION(umount, SYS_umount, TARGET_NR_umount)
#endif
#if defined(SYS_umount2) && defined(TARGET_NR_umount2)
	SYSCALL_RELATION(umount2, SYS_umount2, TARGET_NR_umount2)
#endif
#if defined(SYS_uname) && defined(TARGET_NR_uname)
	SYSCALL_RELATION(uname, SYS_uname, TARGET_NR_uname)
#endif
#if defined(SYS_unlink) && defined(TARGET_NR_unlink)
	SYSCALL_RELATION(unlink, SYS_unlink, TARGET_NR_unlink)
#endif
#if defined(SYS_unlinkat) && defined(TARGET_NR_unlinkat)
	SYSCALL_RELATION(unlinkat, SYS_unlinkat, TARGET_NR_unlinkat)
#endif
#if defined(SYS_unshare) && defined(TARGET_NR_unshare)
	SYSCALL_RELATION(unshare, SYS_unshare, TARGET_NR_unshare)
#endif
#if defined(SYS_uselib) && defined(TARGET_NR_uselib)
	SYSCALL_RELATION(uselib, SYS_uselib, TARGET_NR_uselib)
#endif
#if defined(SYS_ustat) && defined(TARGET_NR_ustat)
	SYSCALL_RELATION(ustat, SYS_ustat, TARGET_NR_ustat)
#endif
#if defined(SYS_utime) && defined(TARGET_NR_utime)
	SYSCALL_RELATION(utime, SYS_utime, TARGET_NR_utime)
#endif
#if defined(SYS_utimensat) && defined(TARGET_NR_utimensat)
	SYSCALL_RELATION(utimensat, SYS_utimensat, TARGET_NR_utimensat)
#endif
#if defined(SYS_utimes) && defined(TARGET_NR_utimes)
	SYSCALL_RELATION(utimes, SYS_utimes, TARGET_NR_utimes)
#endif
#if defined(SYS_vfork) && defined(TARGET_NR_vfork)
	SYSCALL_RELATION(vfork, SYS_vfork, TARGET_NR_vfork)
#endif
#if defined(SYS_vhangup) && defined(TARGET_NR_vhangup)
	SYSCALL_RELATION(vhangup, SYS_vhangup, TARGET_NR_vhangup)
#endif
#if defined(SYS_vm86) && defined(TARGET_NR_vm86)
	SYSCALL_RELATION(vm86, SYS_vm86, TARGET_NR_vm86)
#endif
#if defined(SYS_vm86old) && defined(TARGET_NR_vm86old)
	SYSCALL_RELATION(vm86old, SYS_vm86old, TARGET_NR_vm86old)
#endif
#if defined(SYS_vmsplice) && defined(TARGET_NR_vmsplice)
	SYSCALL_RELATION(vmsplice, SYS_vmsplice, TARGET_NR_vmsplice)
#endif
#if defined(SYS_vserver) && defined(TARGET_NR_vserver)
	SYSCALL_RELATION(vserver, SYS_vserver, TARGET_NR_vserver)
#endif
#if defined(SYS_wait4) && defined(TARGET_NR_wait4)
	SYSCALL_RELATION(wait4, SYS_wait4, TARGET_NR_wait4)
#endif
#if defined(SYS_waitid) && defined(TARGET_NR_waitid)
	SYSCALL_RELATION(waitid, SYS_waitid, TARGET_NR_waitid)
#endif
#if defined(SYS_waitpid) && defined(TARGET_NR_waitpid)
	SYSCALL_RELATION(waitpid, SYS_waitpid, TARGET_NR_waitpid)
#endif
#if defined(SYS_write) && defined(TARGET_NR_write)
	SYSCALL_RELATION(write, SYS_write, TARGET_NR_write)
#endif
#if defined(SYS_writev) && defined(TARGET_NR_writev)
	SYSCALL_RELATION(writev, SYS_writev, TARGET_NR_writev)
#endif
