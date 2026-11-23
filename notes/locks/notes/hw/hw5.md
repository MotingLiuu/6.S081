# How is the kernel able to write to physical memory addresses, rather than virtual memo address? what line of code allows this work?

This is because the identity mapping from `extext` to `PHYSTOP`.

This works because the kernel page identity maps physical RAM from `extext` to `PHYSTOP`.

```c
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
```

maps each kernel virtual address in this range to the physical address with the same numeric value. 
