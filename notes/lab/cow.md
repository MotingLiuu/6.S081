# COW LAB

## PROBLEMS

### Problem 1: `fork1()` in `sh.c` doesn't passed

What happened:

`sh.c` code
```c
    } else {
      printf("DEBUG: befor fork1()\n");
      if (fork1() == 0) {
        printf("DEBUG: fork1 passed\n");
 
```

terminal output
```txt
$ cowtest
DEBUG: befor fork1()
usertrap(): unexpected scause 0xf pid=3
            sepc=0x10ea stval=0x4f38
usertrap(): unexpected scause 0xf pid=2
            sepc=0x2 stval=0x4f78
usertrap(): unexpected scause 0xf pid=1
            sepc=0x7ba stval=0x3f68
panic: init exiting
Q
```

----

When Trace with GDB

```txt
DEBUG: befor fork1()
usertrap(): unexpected scause 0xf pid=3
            sepc=0x10ea stval=0x4f38
```

was printed. After executed `release(&np->lock);`

```c
  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}
```

----

sh.asm

```asm
00000000000010e8 <printf>:

void
printf(const char *fmt, ...)
{
    10e8:	711d                	addi	sp,sp,-96
    10ea:	ec06                	sd	ra,24(sp)
    10ec:	e822                	sd	s0,16(sp)
    10ee:	1000                	addi	s0,sp,32
    10f0:	e40c                	sd	a1,8(s0)
    10f2:	e810                	sd	a2,16(s0)
    10f4:	ec14                	sd	a3,24(s0)
    10f6:	f018                	sd	a4,32(s0)
    10f8:	f41c                	sd	a5,40(s0)
    10fa:	03043823          	sd	a6,48(s0)
    10fe:	03143c23          	sd	a7,56(s0)
  va_list ap;

  va_start(ap, fmt);
    1102:	00840613          	addi	a2,s0,8
    1106:	fec43423          	sd	a2,-24(s0)
  vprintf(1, fmt, ap);
    110a:	85aa                	mv	a1,a0
    110c:	4505                	li	a0,1
    110e:	cf1ff0ef          	jal	dfe <vprintf>
}
```

----

Analysis

`pid = 3` means `usertrap(): unexpected scause 0xf pid=3` is caused in child process.















