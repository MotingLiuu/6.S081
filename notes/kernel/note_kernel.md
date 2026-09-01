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














