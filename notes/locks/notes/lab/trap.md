# Trap

## Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?

```asm
printf("%d %d\n", f(8)+1, 13);
2c:	4635                	li	a2,13
2e:	45b1                	li	a1,12
30:	00001517          	auipc	a0,0x1
34:	8c050513          	addi	a0,a0,-1856 # 8f0 <malloc+0xfa>
38:	706000ef          	jal	73e <printf>
```

1. Register a0, a1, a2 contains arguments to funtions.
2. Register a2 holds 13.

## Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)

```asm
38:	706000ef          	jal	73e <printf>
```

There is no call to g and f. Compiler already compiler the result to 12.

## At what address is the function printf located?

```asm
000000000000073e <printf>:
```

## What value is in the register ra just after the jalr to printf in main?

0x3c

```asm
3c: 4501 li a0, 0
```

## Run the following code

```c
unsigned int i = 0x00646c72;
printf("H%x Wo%s", 57616, (char *) &i);
```
What is the output? Here's an ASCII table that maps bytes to characters.
The output depends on that fact that the RISC-V is little-endian. If the RISC-V were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?

The output is 
```txt
$ test
HE110 World$ jh
```

yes, set i to 0x726c6400
no, no need to change 57616

## In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?

```c
printf("x=%d y=%d", 3);
```
      
The program has undefined behavior because printf expects two integer arguments, but only one is provided.
It may print a garbage value after y= because printf attempts to read a second integer argument that was never passed.
