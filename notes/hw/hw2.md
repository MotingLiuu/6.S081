# Using gdb

1. Looking at the backtrace output, which function called `syscall`?
```txt
(gdb) backtrace
#0  syscall () at kernel/syscall.c:133
#1  0x0000000080001abc in usertrap () at kernel/trap.c:68
#2  0x0000003ffffff09c in ?? ()
```
`usertrap()` called `syscall()`

2. What is the value of `p->trapframe->a7` and what does that value represent?
15, The number of syscall.

3. What was the previous mode that the CPU was in?
```txt
(gdb) p /x $sstatus
$3 = 0x200000022
```
0x22 -> 0b0000000100010
The 13th bit is SSP, which is 0.

4. Replace the statement `num->trapframe->a7` with `num = *(int *)0`. what is the corresponding asm?
```txt
hart 1 starting
hart 2 starting
scause=0xd sepc=0x80001d08 stval=0x0
panic: kerneltrap
```

```asm
  // num = p->trapframe->a7;
  num = *(int *)0;
    80001d08:	00002683          	lw	a3,0(zero) # 0 <_entry-0x80000000>
```


