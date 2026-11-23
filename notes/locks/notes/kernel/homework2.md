# Suppose that an xv6 kernel has used up all of the struct proc entries in the struct proc proc[NPROC] table (i.e., none of them have state == UNUSED). What happens if one of the processes calls exec()?

`exec()` would run normally. Because `exec()` would replace the current process not occupy another process.

# What happens if one of the processes calls `fork()`?

`kfork()` would return `-1` for there is no free process.

# What happens if one of the processes calls `kill()` on an existing PID and then call `fork()`?

For `kill()` just set `p->killed` to `1`. But `allocproc()` in `kfork()` would search for processes with `p->UNUSED`.

`kill()` just set `p->killed` to `1`. When the process killed starts to run, it would find that its `killed` is `1`, then ecall `kexit()` to set `p->state` to `ZOMBIE`. After parent calls `wait()`, it would be set to `UNUSED` through `freeproc()`

```txt
kill(pid)
   |
   v
p->killed = 1
   |
   | 目标进程之后运行
   v
kexit()
   |
   v
ZOMBIE
   |
   | parent calls wait()
   v
freeproc()
   |
   v
UNUSED
```



The hole process of `exec()`
```txt
用户程序
exec("sh", argv)
      ▼
user/usys.S 中的 exec stub
      │ a7 = SYS_exec
      │ ecall
      ▼
CPU 从 U-mode 进入 S-mode
      ▼
trampoline.S:uservec
      ▼
trap.c:usertrap()
      ▼
syscall.c:syscall()
      ▼
sysfile.c:sys_exec()
      ▼
exec.c:kexec()
      ▼
加载新的 ELF 程序
      ▼
usertrap 返回
      ▼
trampoline.S:userret
      ▼
sret
      ▼
新的用户程序开始执行
```

`stvec`(Supervisor Trap Vector Base Address Register) is a register which stores the address of code to run after the trap.

During user mode, `stvec` is set to `uservec` in `trampoline.S` 
`uservec` would do:
```txt
save user registers
set kernel stack
set kernel page table
```

During kernel mode(kernel stack and kernel page table are already set), `stvec` is set to `kernelvec` in `trampoline.S`
If there is a trap,
```txt
kernel code->interrupt->kernelvec->kerneltrap()
```

Use `csrw stvec, a0` to set `stvec` to `a0`
```txt
stvec: where to jump after trap
sepc: where is pc before trap
scause: what is the cause of trap
sstatus: what is the status before/after trap
satp: what is the current page table
```
e.g.
```txt
当前：
PC = 0x1004
mode = U

        ecall
          ↓

sepc   = 0x1004
scause = 8
sstatus record CPU is in U-mode before ecall
mode   = S-mode
PC     = stvec
```

When jumping to `uservec` in `trampoline.S`
```txt
mode = S-mode
satp = USER pagetable (MT: Question: what is pagetable?)
sp = USER stack
registers = user registers
```
In supervisor mode, but with a user page table. This means:
CPU is in S-mode, but the translation between virtual address and physical address is still done by user page table.
Pagetable is decided by `satp` register.

The kernel code is located at `0x80000000` 
but user page table doesn't have these kenrl mappings.
If `stvec` is set to `usertrap()`, which is not in user page table, page fault. (MT: Question: explain this in detail, what is page table? what is mapping?)

The layout of `struct trapframe` is:
```txt
struct trapframe {
  /*   0 */ uint64 kernel_satp;
  /*   8 */ uint64 kernel_sp;
  /*  16 */ uint64 user_trap;
  /*  24 */ uint64 epc;
  /*  32 */ uint64 kernel_hartid;

  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;

  /*  72 */ uint64 t0;
  ...
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  ...
};

struct trapframe

┌─────────────────────────────┐
│ kernel_satp        0        │ ← 怎么切 kernel page table
├─────────────────────────────┤
│ kernel_sp          8        │ ← kernel stack 在哪
├─────────────────────────────┤
│ kernel_trap       16        │ ← usertrap() 在哪
├─────────────────────────────┤
│ epc               24        │ ← user PC
├─────────────────────────────┤
│ kernel_hartid     32        │ ← 当前 CPU
├─────────────────────────────┤
│ ra                40        │
├─────────────────────────────┤
│ sp                48        │
│ ...                         │
│ user registers              │
│ ...                         │
└─────────────────────────────┘
```

Before returning to user mode, `prepare_return()` would do:
```txt
p->trapframe->kernel_satp = r_satp();

p->trapframe->kernel_sp =
    p->kstack + PGSIZE;

p->trapframe->kernel_trap =
    (uint64)usertrap;

p->trapframe->kernel_hartid =
    r_tp();
``` citeturn659434search0


也就是说，在进入用户态之前，kernel 已经提前留下了一张“小纸条”：

```text
下次你 trap 回来时：

kernel page table = 这个

kernel stack = 这个

usertrap 地址 = 这个

CPU hartid = 这个
```
So that `uservec` can recover anything using `TRAPFRAME`


All of the process
```txt
USER MODE
================================================

user program

a0 = syscall arg0
a1 = syscall arg1
a7 = syscall number
sp = user stack

        │
        │ ecall
        ▼


RISC-V HARDWARE
================================================

sepc   ← user PC
scause ← trap cause

mode:
U → S

PC ← stvec

但是：

satp 还是 USER pagetable
sp   还是 USER stack
registers 还是 USER registers

        │
        ▼


TRAMPOLINE: uservec
================================================

S-mode + user pagetable

csrw sscratch,a0
        │
        └── 暂存 user a0

li a0,TRAPFRAME
        │
        ▼

save all user registers
        │
        ▼

restore original a0 from sscratch
save it to trapframe
        │
        ▼

sp ← kernel_sp
tp ← kernel_hartid
t0 ← usertrap
t1 ← kernel_satp
        │
        ▼

satp ← kernel pagetable
        │
        ▼

jalr usertrap
        │
        ▼


KERNEL C CODE
================================================

usertrap()

syscall / interrupt / exception
        │
        ▼

prepare_return()
        │
        ▼

return user_satp
        │
        │ a0 = user_satp
        ▼


TRAMPOLINE: userret
================================================

satp ← user pagetable
        │
        ▼

a0 ← TRAPFRAME
        │
        ▼

restore ra/sp/gp/.../a7
        │
        ▼

restore a0 last
        │
        ▼

sret
        │
        ▼


USER MODE
================================================

PC = sepc

用户程序继续
```

```txt
USER
================================================

exec("sh", argv)

编译器：
a0 = path
a1 = argv

usys.S：
a7 = SYS_exec

ecall
        │
        ▼


uservec
================================================

保存：

a0 → trapframe->a0
a1 → trapframe->a1
a7 → trapframe->a7

        │
        ▼


usertrap()
================================================

scause == 8
        │
        ▼
syscall()


syscall.c
================================================

num = trapframe->a7
    = SYS_exec

        │
        ▼

syscalls[SYS_exec]
        │
        ▼
sys_exec()


sys_exec()
================================================

argstr(0,...)
     │
     └→ trapframe->a0 → path

argaddr(1,...)
     │
     └→ trapframe->a1 → argv

        │
        ▼

kexec(...)
```








e.g.
A user process calls `exec`
CPU find that this is a U-mode ecall.
CPU would do
```txt
sepc = ecall address
scause = 8
privilege: U -> S
PC = stvec
```

The definition of `exec` is in `usys.o` which is compiled from `user/usys.S`
```risc-v
li a7, SYS_exec
ecall
ret
```

after `li a7, SYS_exec` the registers are `a0 = path, a1 = argv, a7 = 7`

`a0-a5` are the parameters of syscall, and `a0` would be the return value of syscall.

The `ecall` instruction would 
```txt
1. sepc <- current pc of ecall
2. scause <- 8 (this represents that the ecall is from user mode)
3. record the privilege level before the ecall
4. set the privilege level to S-mode
5. PC <- stvec (Question: what is stvec?
```

when xv6 returning to user mode. It would set `stvec` to `uservec` in trampoline.

Question: what does trampoline.S:uservec do?
Answer:
1. Save CPU registers to `p->trapframe`
2. Set to S-mode

what does `csrw satp, t1` mean?





