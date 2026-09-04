# User, Supervisor and Machine Mode

If an app in user mode attempts to execute a privilege inst, CPU would traps to special code in S-Mode

```txt
User mode

    application
        |
        | illegal privileged instruction
        v
   +-----------+
   |    CPU    |
   +-----------+
        |
        | TRAP
        v

Supervisor mode

    kernel trap handler
```

# entry.s and start.c 

`start.c`
```txt
start() [M-mode]

1. mstatus.MPP = S
   └─ mret 后进入哪里？

2. mepc = main
   └─ mret 后执行哪里？

3. satp = 0
   └─ 暂时关闭 paging

4. medeleg/mideleg
   └─ traps 以后交给 S-mode kernel

5. PMP
   └─ 允许 S-mode 访问 RAM

6. timerinit
   └─ 建立 timer interrupt

7. tp = hartid
   └─ kernel 可以知道当前 CPU

8. mret
   ↓
main() [S-mode]
```



# Questions
1. register is already set in `entry.S`, what is the point of 
`__attribute__ ((aligned (16))) char stack0[4096 * NCPU];` in `start.c`?
just align the memo.
2. What is MPP? what does 
```c
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK; // MT: set MPP bits to 0
  x |= MSTATUS_MPP_S; // MT: set MPP to Supervisor
  w_mstatus(x); // set CPU to Supervisor mode
```
do?
    what is CSR?
    This is CPU state registers. Record current state of CPU.
3. how kernel create the first user process?
Through userinit() in main.c

# How kenel create the first user process?
Answer:
in `main()` in `main.c`
```c
userinit();      // first user process
```

in `proc.c`
```c
void
userinit(void)
{
  struct proc *p;

  p = allocproc(); // what does allocproc do?
  initproc = p;
  
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}
```

in `proc.c`
```c
static struct proc* // MT: this func would return a proc pointer
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

```

in `spinlock.c`
```c

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

```


# How does `exec()` work?

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


# How does xv6 manage memo?

```txt
layout of pa

低地址
│
│   一些设备、boot 相关区域
│
├────────────────────────────
│ 0x80000000 = KERNBASE
│
│   xv6 kernel code
│   xv6 kernel data
│   ...
│
│ end
├────────────────────────────
│
│   可用物理内存
│
│   page
│   page
│   page
│   ...
│
├────────────────────────────
│ PHYSTOP
│
高地址
```

kmem.freelist
```txt
kmem.freelist
      │
      ↓
+------------------+
| free page        |
| next ------------|----+
+------------------+    |
                       ↓
                  +------------------+
                  | free page        |
                  | next ------------|----+
                  +------------------+    |
                                         ↓
                                    +------------------+
                                    | free page        |
                                    | next = 0         |
                                    +------------------+
```

Use the first bytes of free pages to store the pointer to the next free pages.
```c
r = (struct run*)pa
```

```txt
0x87000000
+-----------------------------+
| next pointer     8 bytes    |
+-----------------------------+
|                             |
|        剩余空闲空间          |
|                             |
+-----------------------------+
```

```txt
Physical RAM

+--------------------------+
| kernel                   |
| kernel                   |
| kernel                   |
+--------------------------+ ← end
| allocated page           |
+--------------------------+
| free page                | ← struct run
| next --------------------|----+
+--------------------------+    |
| allocated page           |    |
+--------------------------+    |
| free page                | <--+
| next --------------------|----+
+--------------------------+    |
| free page                | <--+
| next = NULL              |
+--------------------------+
| ...                      |
+--------------------------+ ← PHYSTOP
```


# how does kernel run in CPU?

```txt
ROM
 ↓
firmware
 ↓
bootloader
 ↓
load kernel
```

bootloader would load kernel code to RAM

```txt
磁盘上的 kernel ELF
        ↓
加载到 RAM
        ↓
0x80000000:
+----------------------+
| kernel machine code  |
| kernel machine code  |
| kernel data          |
| ...                  |
+----------------------+
```

CPU would run from 0x80000000 of RAM

```txt
             PC
             │
             ↓
RAM    0x80000000
       +----------------+
       | _entry         |
       | ...            |
       +----------------+
```

The first running instruction is `kernel/entry.S`
At this point, there is no VA.
```txt
CPU address
    │
    │ 没有 page table translation
    ↓
physical RAM
```

The first thing `entry.S` does is to set up the stack pointer to run c code.
```txt
high address

+------------------+
| local variables  |
+------------------+
| saved ra         |
+------------------+
| ...              |
+------------------+
        ↑
        sp
```

This code set the sp to `stack0` in `entry.S`
```riscv
        la sp, stack0
```

After stack is set, `entry.S` would jump to `start.c` to run `start()`

```txt
_entry
  ↓
start()
  ↓
mret
  ↓
main()
```

`main()` would call `kinit()` to initialize the physical page allocator.
After `kinit()` is called, kernel can use `kalloc()`

`w_satp(...)` would open VA. Before calling `w_satp()`, kernel has already construct a direct mapping. CPU would not notice the switching.
```txt
        Before

PC 0x80001234
       ↓
PA 0x80001234


         After

PC 0x80001234
       ↓
MMU
       ↓
page table
       ↓
PA 0x80001234
```

all of the process
```txt
Power on / QEMU starts
        │
        ▼
物理 RAM 已存在
CPU、设备已存在
        │
        ▼
boot code / QEMU
把 xv6 kernel 装入 RAM
        │
        ▼
PA 0x80000000
+------------------------+
| _entry                 |
| kernel code            |
| kernel data            |
| initial stacks         |
+------------------------+
        │
        ▼
PC → _entry
        │
        ▼
entry.S
设置初始 sp
        │
        ▼
start()
M-mode 初始化
        │
        ▼
mret
        │
        ▼
main()
S-mode
paging 仍关闭
        │
        ▼
kinit()
        │
        ├── kernel image：已经占用
        │
        └── end ~ PHYSTOP：加入 freelist
        │
        ▼
kvminit()
用 kalloc() 创建 page tables
        │
        ▼
kvminithart()
写 satp
开启 MMU
        │
        ▼
kernel virtual memory 开始工作
        │
        ▼
proc / trap / plic / disk / fs 初始化
        │
        ▼
userinit()
建立第一个 user process
        │
        ▼
scheduler()
        │
        ▼
CPU 开始运行 user process
        │
        ├──── syscall/trap ────► kernel
        │                         │
        ◄──────── sret ───────────┘
```



# GCC inline asm

```c
asm volatile(
    "assembly"
    : outputs
    : inputs
    : clobbers
);
```

```c
static inline uint64
r_mstatus() // r means read, mstatus is a CSR in CPU
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}
```

`=r` means this is an output operand.
`=` means assembly would write into `x`. `r` would let compiler give a general-purpose register.
`"=r" (x)` means this asm would return a result, let compiler find a general-purpose register and save it into `x`

# CSR

1. mstatus register
MPP (Machine Previous Privilege) is a field in mstatus register.
It tells CPU what privilege mode would return to when calling `mret`.
```txt
63                           12 11                0
+-----------------------------+----+---------------+
|           ...               |MPP |      ...      |
+-----------------------------+----+---------------+


      bit 12:11      bit 8       bit 7       bit 5       bit 3       bit 1
          ↓            ↓           ↓           ↓           ↓           ↓

       ┌─────┐      ┌─────┐     ┌─────┐     ┌─────┐     ┌─────┐     ┌─────┐
       │ MPP │      │ SPP │     │MPIE │     │SPIE │     │ MIE │     │ SIE │
       └─────┘      └─────┘     └─────┘     └─────┘     └─────┘     └─────┘
          │            │           │           │           │           │
          ▼            ▼           ▼           ▼           ▼           ▼
      M trap前       S trap前    trap前MIE    trap前SIE    当前M中断   当前S中断
      privilege      privilege
```

two bits can encode different privilege modes.
```txt
MPP = 00  → User
MPP = 01  → Supervisor
MPP = 11  → Machine
```


2. mepc register (Machine Exception Program Counter)
stores the jump address of `mret`

when CPU `mret` it would `PC <- mepc`

3. satp register (Supervisor Address Translation and Protection)
satp would tell MMU:
1. wether paging is enabled
2. which page mode is enabled
3. where is the root page table

satp controls the address translation in U-mode and S-mode
The instruction running in M-mode would not use `satp` to translate address

```txt
63        60 59             44 43                    0
+-----------+-----------------+------------------------+
|   MODE    |      ASID       |         PPN            |
+-----------+-----------------+------------------------+
```

```txt
MODE = 0
    ↓
Bare mode
    ↓
不进行地址翻译

MODE = 8
    ↓
Sv39
    ↓
使用三级 page table 进行地址翻译
```

xv6 would use `MAKE_SATP(pagetable)1 to enable paging
This would set
```txt
MODE = Sv39
PPN  = 根 page table 的物理页号
```
Then use `w_satp(...)` to enable page table

3. medeleg register (Machine Exception Delegation Register)

controls what exceptions would be delegated to S-mode
```c
w_medeleg(0xffff);
```
would set
```txt
0000 ... 1111111111111111
         ↑
       低 16 bit
```

```txt
medeleg

bit 0   instruction address misaligned
bit 1   instruction access fault
bit 2   illegal instruction
bit 3   breakpoint
bit 4   load address misaligned
bit 5   load access fault
bit 6   store address misaligned
bit 7   store access fault
bit 8   ecall from U-mode
...
bit 12  instruction page fault
bit 13  load page fault
bit 15  store page fault
```

4. mideleg register (Machine Interrupt Delegation Register)

controls what interrupts would be delegated to S-mode
```txt
Supervisor Software Interrupt
Supervisor Timer Interrupt
Supervisor External Interrupt

Machine Software Interrupt
Machine Timer Interrupt
Machine External Interrupt
...
```

5. sie register (Supervisor Interrupt Enable Register)

controls what interrupts would be enabled in S-mode

e.g. there is a external interrupt
```txt
UART
 │
 │ “有字符到了”
 ↓
PLIC
 │
 ↓
CPU external interrupt
```

At first, CPU would check `mideleg` to see if it would be delegated to S-mode

Then CPU would check `sie` to see whether this interrupt is allowed to be dealed in S-mode

```txt
External interrupt
        │
        ▼
┌─────────────────────┐
│ mideleg              │
│ 归不归 S-mode 管？   │
└──────────┬──────────┘
           │ yes
           ▼
┌─────────────────────┐
│ sie.SEIE            │
│ 这种中断开没开启？   │
└──────────┬──────────┘
           │ yes
           ▼
┌─────────────────────┐
│ sstatus.SIE         │
│ S-mode 全局允许吗？  │
└──────────┬──────────┘
           │ yes
           ▼
      trap to S-mode
```

5. PMP pmpaddr0 pmpcfg0
pmpaddr0 controls where the pmp can control
pmpcfg0 controls what kind of pmp is enabled

every PMP entry in pmpcfg0 has a config byte
```txt
bit 7      6 5   4 3    2   1   0
+---------+-----+------+---+---+---+
|    L    |  0  |  A   | X | W | R |
+---------+-----+------+---+---+---+
```

```txt
R = Read
W = Write
X = Execute

A = Address matching mode
L = Lock
```

```c
w_pmpcfg0(0xf);
```
sets
```txt
R = 1
W = 1
X = 1
A = TOR
```

PMP has several modes
```txt
A = 00    OFF
A = 01    TOR
A = 10    NA4
A = 11    NAPOT
```
So, xv6 use TOR Top Of Range, `pmpaddr0` would be the top of this PMP
So the range of entry 0 is `[0, pmpaddr0<<2]`. PMP address do not save the last 2 bits of address.

```txt
User / Supervisor
       │
       │ virtual address
       ▼
┌───────────────────────┐
│ MMU / Page Table      │
│                       │
│ VA → PA               │
│ 检查 PTE 权限          │
└──────────┬────────────┘
           │
           │ physical address
           ▼
┌───────────────────────┐
│ PMP                   │
│                       │
│ 检查物理地址权限        │
└──────────┬────────────┘
           │
           ▼
       Physical RAM
```

6. mie Machine Interrupt Enable Register

STIE is Supervisor Timer Interrupt Enable.

7. menvcfg Machine Environment Configuration Register
```c
  w_menvcfg(r_menvcfg() | (1L << 63)); 
```
This would set
```txt
S-mode xv6

      │
      │ write stimecmp
      ▼
hardware timer comparator
```
xv6 is allowed to write to stimecmp, do not need to get in M-mode to write to stimecmp

8. mcounteren Machine Counter Enable Register

This register controls whether S-mode would allowed to access some hardware counters (e.g. cycle time instret)


# page table in xv6

RISC-V Sv39 is 3-level page table
The root of the tree is a 4096-byte page table that contains 512 PTEs, which contain the physical address for page-table pages in the next level of the tree. Each of those pages contains 512 PTEs for the final level in the tree.

```txt
pagetable
   |
   | VPN[2]
   v
Level 2 page table
   |
   | VPN[1]
   v
Level 1 page table
   |
   | VPN[0]
   v
Level 0 page table
   |
   v
PTE
```

only bottom 39 bits of a 64bit virtual address are used.
```txt
xxx01111111 11111111111111111111111111111111
```
Each PTE (Page Table Entry) has a 44 bit PPN (Physical page number) and some flags
```txt
63                    54 53                10 9        0
+-----------------------+--------------------+----------+
|      Reserved         |        PPN         | flags    |
+-----------------------+--------------------+----------+
```
The paging hardware translate a virtual address by using the top 27 bits of the 39 bits to index into the page table to find PTE
making a 56bit physical address whose 44 bits come from the PPN in the PTE and whose bottom 12 bits are copied from the original virtual address.

A RISC-V caches page table entries in a TLB (Translation Lookaside Buffer).

## kvminit() and kvmmake()

The big picture of paging

```txt
                 MMU OFF
                    │
                    ▼
          physical memory usable
                    │
                    ▼
                 kinit()
                    │
              initialize kalloc
                    │
                    ▼
                kvminit()
                    │
              kalloc page(s)
             for page tables
                    │
                    ▼
             build page table
                    │
                    ▼
             kvminithart()
                    │
               write satp
                    │
                    ▼
                 MMU ON
```

Questions:
1. what is MMU?
Memory Management Unit
```txt
Virtual Address (VA)
        |
        v
      MMU
        |
        v
Physical Address (PA)
        |
        v
       RAM
```
`satp` register in CPU tells the MMU:
    1. whether paging is enabled
    2. what page-table scheme is being used
    3. where the root page table is


2. At the beginning MMU is disabled? It is enabled during `kvminit()`?
```txt
Machine starts
     |
     v
entry.S
     |
     v
start()
     |
     | satp = 0
     v
paging disabled
     |
     v
main()
     |
     +--> kinit()
     |
     +--> kvminit()
     |      create kernel page table
     |
     +--> kvminithart()
            enable paging/MMU
```



3. before `kvminit()`, kernel already enabled `kalloc()`?
```c
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();

    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
```
`kalloc()` must already work before virtual memo is enabled.


```txt
kvminit()
   |
   v
kvmmake()
   |
   | 创建一张完整的内核三级页表
   | 建立 UART / VIRTIO / PLIC / kernel text / RAM / trampoline / stack 映射
   v
kernel_pagetable
   |
   | 还只是内存中的数据结构
   v
kvminithart()
   |
   | 写 satp
   v
CPU/MMU 开始真正使用它
```

`pagetable_t kernel_pagetable` is a pointer pointing to level-2 page table.
```
kernel_pagetable
      |
      v
+----------------------+     一个 4096B 页面
| L2 PTE 0             |
| L2 PTE 1             |
| L2 PTE 2             |
| ...                  |
| L2 PTE 511           |
+----------------------+
       |
       | 某些 PTE 指向其他物理页
       v
+----------------------+     L1 page table
| 512 个 PTE           |
+----------------------+
       |
       v
+----------------------+     L0 page table
| 512 个 PTE           |
+----------------------+
```

The calling chain of `kvmmake()`
```txt
kvmmake()
   ↓
kvmmap()
   ↓
mappages()
   ↓
每处理一个 4KB 页面
   ↓
walk()
   ↓
根据 VA 找到 L0 PTE
   ↓
必要时 kalloc() 创建 L1/L0 页表
   ↓
*pte = PA2PTE(pa) | perm | PTE_V
```

### mappages(pagetable, 0x4000, 0x3000, 0x80004000, PTE_R | PTE_W);

```c
#define PXMASK 0x1FF
#define PXSHIFT(level) (PGSHIFT + (9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)
```

PX(0, va) = (va >> 12) & PXMASK
PX(1, va) = (va >> 21) & PXMASK
PX(2, va) = (va >> 30) & PXMASK

This extract the three 9-bit page table indices from a virtual address.

```c
define PTE2PA(pte) (((pte) >> 10) << 12)
```

Think:

Every VA has a corresponding PTE in the page table.

The first 9 bits would find the PTE in level-2 page table.
Then the second 9 bits would find the PTE in level-1 page table.
And the last 9 bits would find the PTE in level-0 page table.

This 27 bits just seperate the VA into 2^27 pages, and any address in the Virtual Space would be delegated to one of 2^27 page (one PTE serials num), and kernel would save the information of corresponding PA into PTE.

An PTE is
```txt
63-53       53-10                10-8 8-7 7-6 6-5 5-4 4-3 3-2 2-1 1-0
Reserved  Physical Page Number   RSW   D   A   G   U   X   W   R   V
```
The physical page number + offset in VA are used to find the physical address.

When calling `mappages()` 

```c
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
```
e.g.
`pa` is 
```txt
0x xxxx xxxx xxxx x000 
```
Then `PA2PTE(pa)` would `>>10 <<12` to get the corresponding Physical Page Number.
```c
*pte = PA2PTE(pa) | perm | PTE_V
```
After this, the mapping between 'va' and 'pa' would be saved in the page table.

```c
walk(pagetable_t pagetable, uint64 va, int alloc)
```
`walk()` just return the physical address of PTE corresponding to 'va'
If the level-1 and level-0 page table is not created, it would create them.

## `pagetable_t kvmmake(void)`

```txt
虚拟地址                             映射到物理地址

UART0        ---------------------> UART0
VIRTIO0      ---------------------> VIRTIO0
PLIC         ---------------------> PLIC

KERNBASE     ---------------------> KERNBASE
kernel text

etext        ---------------------> etext
kernel data
physical RAM

...
TRAMPOLINE   ---------------------> trampoline 的真实物理地址

kernel stack ---------------------> kalloc() 得到的物理页
```

This function just makes a direct mapping from va to pa.






































# trap in RISC-V
```txt
                    trap
                 /        \
                /          \
        exception          interrupt
        同步异常             异步中断
```


# start.c

All of the process

```txt
                  CPU starts in M-mode
                          │
                          ▼
                    start()
                          │
          ┌───────────────┼──────────────────┐
          │               │                  │
          ▼               ▼                  ▼
      MPP = S          mepc=main          satp=0
      目标模式           目标PC             暂无paging
          │
          ▼
   medeleg / mideleg
   trap交给S-mode
          │
          ▼
      sie / mie
      打开所需中断
          │
          ▼
         PMP
   给S-mode访问RAM权限
          │
          ▼
      timerinit()
   配置每hart timer
          │
          ▼
    tp = mhartid
   保存当前CPU编号
          │
          ▼
         mret
          │
          ▼
               S-mode main()
```

```c
w_mie(r_mie() | MIE_STIE);

w_menvcfg(r_menvcfg() | (1L << 63));

w_mcounteren(r_mcounteren() | 2);
```

They deal with

```txt
① timer interrupt 要不要 enable？
   ↓
mie.STIE


② S-mode 能不能直接使用 stimecmp？
   ↓
menvcfg.STCE


③ S-mode 能不能读取 time？
   ↓
mcounteren.TM
```

seperately.


# proc.c

```txt
             process A

user mode:
              user stack
                  ↑
                  |
               user code

                  |
                  | trap / syscall
                  v

kernel mode:
              kernel code
                  |
                  v
             kernel stack A
```

```c
                 proc_mapstacks()

proc[0]
   |
   +---- kalloc() ----> physical page A
   |
   +---- KSTACK(0)
             |
             | kvmmap
             v
       KSTACK(0) VA ─────────> PA A


proc[1]
   |
   +---- kalloc() ----> physical page B
   |
   +---- KSTACK(1)
             |
             v
       KSTACK(1) VA ─────────> PA B


Virtual layout:

TRAMPOLINE
────────────────────────

unmapped guard page
────────────────────────
KSTACK(0) ───────────────→ physical page A
────────────────────────

unmapped guard page
────────────────────────
KSTACK(1) ───────────────→ physical page B
────────────────────────

unmapped guard page
────────────────────────
KSTACK(2) ───────────────→ physical page C
────────────────────────
```

## struct proc

```c
UNUSED
   |
   | allocproc()
   v
USED
   |
   | 初始化完成
   v
RUNNABLE
   |
   | scheduler 选中
   v
RUNNING
   |
   +------> SLEEPING
   |
   +------> RUNNABLE
   |
   +------> ZOMBIE
                |
                | parent wait()
                v
             UNUSED
```

## void procinit(void)

Questions:
1.
how does `acquire(&p->lock)` work?
why only one thread can acquire the lock?
Is there a situation that two threads runs `acquire(&p->lock)` at the same time? So both get the lock?

2. 
what is `wait_lock`?


# Parsing cmd

## Grammar of cmd

```txt
cmd       ::= line
line      ::= pipe { '&' } [ ';' line ]
pipe      ::= block [ '|' pipe ]
block     ::= exec
            | '(' line ')' redirs
exec      ::= redirs { WORD redirs }
redirs    ::= { redir }
redir     ::= '<' WORD
            | '>' WORD
            | '>>' WORD
```

## redir cmd

`< word`
redirect the stdin `fd 0` to `in`

`> word`
redirect the stdout `fd 1` to `out`

`>> word`
redirect the stdout `fd 1` to `out`, `O_WRONLY | O_CREATE`

```txt
cat file < in > out
│   │    │  │  │  │
│   │    │  │  │  └─ WORD
│   │    │  │  └──── output redirection
│   │    │  └─────── WORD
│   │    └────────── input redirection
│   └─────────────── argument
└─────────────────── program
```


# Operation System Interface

## I/O and File descriptors

When launching a shell
```c
  while((fd = open("console", O_RDWR)) >= 0){

    /*
     * what is open("console", O_RDWR)? what is console? why fd >= 3?
     */

    if(fd >= 3){
      close(fd);
      break;
    }
  }
```
This code sets
```txt
fd 0 ──────> console
fd 1 ──────> console
fd 2 ──────> console
```

xv6 use `fd 2` to prompt, this is because `fd 0` and `fd 1` can be redirected to other file.
Using `fd 2` to prompt can ensure that the prompt is always on the screen.











# vm.c

## uint64 walkaddr(pagetable_t pagetable, uint64 va)

`walk()` just return the address of PTE self.

`walkaddr()` returns the physical page address correspondin to va
It would call `walk()` to find the corresponding PTE, then use `pa = PTE(*pte)` to get the physical address.


## int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)

This function would copy from kernel to user. Copy len bytes from src to va dstva in a given page table.

1. pagetable is the root page table of the user process.
2. dstva is the destination virtual address in user space.
3. src is the address of the kernel buffer
4. len is the length of the buffer

## int copying(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)

This function would copy from user to kernel. Copy len bytes from va srcva in a given page table.

## int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)

This function would copy a null-terminated string from user to kernel.



```c
kernel src
   ↓
"hello"

copyout(pagetable, 0x4123, src, 5)

最终：

user VA 0x4123
   ↓
"hello"
```










# lab2

## Look at the backtrace output, which function called syscall?

Answer:
`usertrap()` in `trap.c`

## what is the value of p->trapframe->a7 and what does that value represent?

in `syscall(void)` `num=p->trapframe-a7` then call `syscalls[num]()`, so `p->trapframe->a7` represent the syscall num.

For example, when calling `write(2, "$ ", 2)`
1st
```c
li a7, SYS_write
ecall
ret
```
The code that compiler generated for the function call loads the three arguments into register `a0`, `a1` and `a2`. Then `write()` function loads the system call nuber, `SYS_write` into `a7`

The `ecall` instruction traps from user space into the kernel, and causes `uservec`, `usertrap` and then `syscall` to execute.

```c
用户程序
   |
   | call write
   |   ra = write 返回后的地址
   v
write:
   li a7, SYS_write
   ecall
       |
       | trap：U-mode -> S-mode
       v
   uservec
   usertrap
   syscall
   sys_write
       ...
       |
       | 内核最后通过 usertrapret / trampoline
       | 执行 sret
       v
   回到用户态 write() 中 ecall 后面的指令
   |
   v
ret
   |
   | PC <- ra
   v
用户程序中 call write 后面的下一条指令
```

when `ecall` is executed
1. save address of `ecall` into `sepc`
2. set `scause` to `8`, if in U-mode
3. set `SPP` to `0`
3. set `sstatus.SPP` to `0`, SPP would record the privilege mode before trap. When running `sret` CPU would return to `U-mode` based on the content in `SPP`
4. `SPIE = SIE` `SIE = 0` would disable `S-mode interrupt`
5. `PC` would jump to `stvec`, which stores the address of `uservec` in `trampoline.S`


## what was the previous mode that CPU was in?

(gdb) p /x $sstatus
$3 = 0x200000022

SSP is the 8th bit in sstatus, which is `0`. So the previous mode was `S-mode`

## 

































