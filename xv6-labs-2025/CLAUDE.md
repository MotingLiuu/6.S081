# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`xv6-labs-2025` is the 2025 edition of the MIT 6.S081 / 6.1810 lab tree — a RISC-V re-implementation of Unix V6 that students extend each lab. The active lab is selected by `LAB=...` in `conf/lab.mk` (currently `LAB=util`); that single variable flips kernel compile flags (`-DLAB_$(LABUPPER)`, `-DSOL_$(LABUPPER)`), adds lab-specific `UPROGS`, and tunes constants in `kernel/param.h`.

## Build / run / test commands

Requires a RISC-V `newlib` toolchain (riscv64-unknown-elf-*) and `qemu-system-riscv64` ≥ 7.2. On this machine both live in `/opt/homebrew/bin`, which the Makefile's TOOLPREFIX probe will find automatically.

```
make qemu           # build kernel + user progs + fs.img, then boot in qemu (-smp 3 by default)
make qemu-fs        # boot qemu without rebuilding fs.img (faster iteration)
make qemu-gdb       # boot with -S and a gdb stub on tcp::GDBPORT (25000 + uid%5000)
make CPUS=1 qemu    # restrict to N harts (LAB=fs forces CPUS=1)
make KCSAN=1 qemu   # build with kernel thread sanitizer
make newfs.img      # move current fs.img to fs.img.bk before next build
make clean          # nuke *.o *.d *.asm *.sym, kernel/kernel, fs.img, mkfs/mkfs, usys.S
make grade          # runs `make clean` then ./grade-lab-$(LAB); points total = 100
```

The `make grade` and `make clean` targets are the authoritative way to test. There is no per-test target — `grade-lab-util` (a Python harness in `gradelib.py`) runs each `@test(...)` block sequentially against a freshly-booted xv6. Use `V=1 make grade` for verbose qemu output. Captured qemu output from the last grading run is saved to `xv6.out` (ignored by git, kept for inspection when a test fails).

To iterate on a single program quickly, `make qemu` and type at the shell prompt inside xv6; exit qemu with `Ctrl-A x`.

## Code architecture

Two parallel source trees, both freestanding (no libc):

- **`kernel/`** — supervisor-mode code. Boot path: `entry.S` (per-hart entry, sets up stack, jumps to `start.c`) → `main.c` (hart 0 does all `*init()` calls then sets `started=1`; other harts spin on `started` and only re-enable paging + traps) → `scheduler()` in `proc.c`. Memory layout is the qemu `virt` machine (UART0, virtio, PLIC); see `kernel/memlayout.h`. Page tables: `vm.c`. Traps: `trap.c` (kernel) + `trampoline.S` (user → kernel trampoline that lives in the highest page, mapped into every process). Per-process state, including the `trapframe` layout consumed by `trampoline.S`, lives in `kernel/proc.h` — keep the C struct and the assembly in sync.
- **`user/`** — statically-linked userland. Every program is linked against `ULIB = ulib.o usys.o printf.o umalloc.o` plus `user/user.ld`. `user/usys.pl` generates the syscall stubs (`usys.S`) from the prototypes in `user/user.h`, so adding a syscall requires updating `kernel/syscall.h`, `kernel/syscall.c`, `kernel/sysproc.c` (or similar), and `user/user.h` in that order. `user/ulib.c` is the only libc the kernel ships; notably it exposes `sbrk()` (eager allocation via `sys_sbrk(SBRK_EAGER)`) and `sbrklazy()` (lazy allocation via `SBRK_LAZY`).

The on-disk filesystem image `fs.img` is built by the host tool `mkfs/mkfs` from `README`, `$(UEXTRA)`, and the compiled `$(UPROGS)`. `make qemu` first runs `make newfs.img` (which renames the old `fs.img` to `fs.img.bk`) so a stale image never breaks a boot.

## Lab-specific notes (current `LAB=util`)

This tree has a non-stock `util` lab layered on top of upstream xv6. Things to know:

- New user programs: `user/sixfive.c` (tokenize a file, print numbers that are multiples of 5 or 6 — sample input `user/sixfive.txt`) and `user/memdump.c` (read up to 512 bytes from stdin and dump them per a format string like `"iiSpch"`; the function body is left for the student). The `find` program is also extended to support `-exec CMD ...`.
- `user/findtest.sh` is the `-exec` integration fixture; it expects `find . b -exec grep hello` to walk subdirectories.
- A new `SYS_pause` syscall was added (kernel/sysproc.c:66) and is used by `user/sleep.c` instead of the upstream `sleep(1)` busy-wait. `kernel/param.h` bumps `USERSTACK` from 1 → 2 pages when `LAB_UTIL` is defined so the slightly fatter stack frame in `find` doesn't overflow.
- `LAB_UTIL` does not change `FSSIZE` (still 2000), `NPROC` (64), or `NCPU` (8); the default qemu SMP count is 3 unless overridden.
- `user/init.c`, `user/sh.c`, and `kernel/sh.c` are stock — don't be surprised they look like textbook xv6.

## Things to watch out for

- `make clean` deletes `fs.img`, so back it up (or commit a known-good one) before iterating if you care about on-disk state.
- `make grade` always calls `make clean` first; if a previous `qemu` is still running it will fail with "Do you have another running instance of xv6?".
- `xv6.out*` files are local debugging artifacts (last qemu capture, sample grading runs for `sixfive`, etc.) — they are gitignored and should not be edited by hand.
- `kernel/proc.h`'s `trapframe` struct and `kernel/trampoline.S` are tightly coupled by offset; adding fields means updating both.
- Branch names in `git branch` are the canonical lab labels (`util`, `syscall`, `pgtbl`, etc.) — the upstream `submit-check` target in the Makefile expects to run on a branch matching `$(LAB)`.
