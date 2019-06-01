# vexllvm

This is a dynamic binary translator for converting machine code to LLVM IR.

## Build

First, install [valgrind](https://valgrind.org) for its libvex headers.

Next, build [guestlib](https://github.com/heyitsanthony/guestlib). Create a symlink `guestlib` in the root directory or specify it using by setting `GUESTLIB_PATH`.

The build system is just a primitive Makefile that only needs a call to `make`. If `llvm-config` isn't in the `PATH` (e.g., gentoo, custom LLVM build), specify it:
```sh
LLVMCONFIG_PATH=/usr/lib64/llvm/8/bin/llvm-config make
```

### LLVM debugging build

If debugging, it's useful to compile Release+Asserts to catch LLVM issues that wouldn't show up in a release build:
```sh
cmake -DLLVM_ENABLE_RTTI=ON -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=Release ../llvm-8.0.0.src
make
```

## JIT execution

### Machine code through LLVM JIT

A guest program is loaded into the host process, then JIT'd via `pt_run`:
```sh
bin/pt_run /bin/ls /
```

### Cross-checking JIT against host

vexllvm can cross-check its JIT execution against host hardware to detect mismatches between JIT semantics and hardware:
```sh
GUEST_STEP_GAUGE=1000 bin/pt_xchk /bin/ls /
```

### Process snapshots

Process snapshots can be saved by setting `VEXLLVM_SAVE` or `VEXLLVM_SAVEAS` environment variables, then loaded and run multiple times via `ss_run`:
```sh
VEXLLVM_SAVE=1 bin/pt_run /bin/ls /
bin/ss_run
bin/ss_run
```

### Nested JIT

The JIT can run inside itself using `rebase` binaries which are linked at non-conflicting addresses. This is useful in cross-checking mode for discovering deep JIT bugs:
```sh
VEXLLVM_SAVE=1 bin/pt_run /bin/ls /
bin/pt_xchk_rebase bin/ss_run
```

### IR debugging

This dumps a trace of the vex frontend and llvm IR to stderr:
```sh
VEXLLVM_DUMP_LLVM=1 VEXLLVM_DISPATCH_TRACE=stderr VEXLLVM_SB_LOG=stderr VEXLLVM_TRACE_FE=stdout bin/pt_run /bin/ls /
```

