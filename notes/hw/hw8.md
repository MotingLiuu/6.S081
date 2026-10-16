## vm.c:copyout() calls vmfault when dstva isn't mapped. Describe a scenario in which dstva isn't mapped? 

When lazy alloc. If dstva is not mapped, vmfault() would apply a new page by `kalloc` and map this page to dstva by `mappages` when va < `p->sz`. if not, return 0.

## That is, give a list of system calls that a process can make to triggers this case.

All system call that need to write into user's memo (lazy alloc). 
