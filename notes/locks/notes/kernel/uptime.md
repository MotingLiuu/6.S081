# uptime

`uptime()` answers one simple question:
"how long has the computer been turned on?"

It would return the ticks. The OS counts tick each time a built-in alarm clock goes off(~10 times/second). 

CLINT, timerinit, ecall, harts, are the plumbing that makes that counter work.

1. `ticks`, counter stored inside the kernel. It's defined in `kernel/trap.h`, `unit ticks`. Every time the hardware alarm fires (about every 0.1 second), the kernel does: `ticks++`.

There is a lock(ticklock) around it. Multiple CPUs can read ticks at the same time. just mean: one cpu in here at a time.

2. `virt`, name of a fake machine that the QEMU pretends to be.
3. `CLINT`(Core Local Interruptor). A modern CPU has multiple cores(RISC-V calls them harts, "hardware threads"). 
The CLINT is a tiny hardware box that gives hart its own private:
    - Timer
    - ***Software interrupts(IPI, iter-processor interrupts) "Hart #0 wants Hart #3 to do something"***
A per-core mailbox + alarm clock.
4. `timerinit()` is a function that sets up the alarm clock at boot. Runs one per hart. From `kernel/start.c`
```c
void timerinit()
  {
    w_mie(r_mie() | MIE_STIE);                  // ①
    w_menvcfg(r_menvcfg() | (1L << 63));        // ②
    w_mcounteren(r_mcounteren() | 2);           // ③
    w_stimecmp(r_time() + 1000000);             // ④
  }
```
juest mean: Make sure the alarm clock is allowed to fire, then set it to go off in 0.1 seconds.

5.`clockintr()` the hardware interrupt is one-shot, rings once. somebody has to keep resetting it.

6. User-mode program call `uptime()` throught `syscall`
The user program call `uptime()`, then assembly of `uptime()` triggers a trap.
```RISC-V
uptime:
    li a7, 14
    ecall
    ret # come back; the answer is in a0
```
`ecall` a special instruction saves program counter, switches to kernel mode, jumps to pre-set location the kernel chose.
`ecall` jumps into `usertrap()` in `kernel/trap.c`. It looks scause(supervisor cause register). `scause == 8` means it was an syscall.
It then calls `syscall()` in `kernel/syscall.c`
```c
num = p-->trapfram-->a7;
syscalls[num](p);
p-->trapframe-->a0 = result;
```
when a trap happens, the kernel saves the user program's registers in a struct called `trapframe` in memo. The kernel read these to know what the user wanted and overwrite a0 to send the answer back.
after that.
`sys_uptime` returns the counter.

```c
uint64 sys_uptime(void)
  {
    uint xticks;
    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
  }``
```

user program
     │ uptime()
     ▼
  usys.S stub                (a7 = 14)
     │ ecall  ←── trap into kernel
     ▼
  usertrap()                 (scause == 8 → it's a syscall)
     │
     ▼
  syscall()                  reads a7 (=14), calls syscalls[14]
     │
     ▼
  sys_uptime()               reads ticks under lock, returns it
     │
     ▼
  syscall()                  writes return value into trapframe->a0
     │
     ▼
  usertrap returns to user   a0 holds the tick count
     │
     ▼
  fprintf prints it

  ---
  One-page cheat sheet

  ┌────────────────┬───────────────────────────────────────────────────────────┐
  │      Term      │                     What it really is                     │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ tick           │ one beat of the system alarm clock (~0.1 s)               │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ ticks          │ the counter that records how many beats since boot        │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ hart           │ a CPU core (RISC-V's word for it)                         │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ RISC-V         │ the CPU architecture xv6 is compiled for                  │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ virt           │ the fake machine layout QEMU emulates                     │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ CLINT          │ per-hart timer + interrupt mailbox                        │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ timerinit()    │ sets up the alarm clock at boot, arms the first tick      │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ clockintr()    │ the alarm handler — bumps ticks and re-arms the next tick │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ CSR            │ a tiny variable built into the CPU (knobs on the front    │
  │                │ panel)                                                    │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ sstc /         │ the newer "set the next alarm" register                   │
  │ stimecmp       │                                                           │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ ecall          │ the CPU instruction that says "trap into the kernel,      │
  │                │ please"                                                   │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ trapframe      │ where the kernel stashes the user process's registers     │
  │                │ during a trap                                             │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ a7             │ the register where you put the syscall number             │
  ├────────────────┼───────────────────────────────────────────────────────────┤
  │ syscall        │ a request from user code for the kernel to do something   │
  │                │ privileged                                                │
  └────────────────┴───────────────────────────────────────────────────────────┘



