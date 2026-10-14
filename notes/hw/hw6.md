# After the CPU executes csrw satp, t1, the CPU will translate 0x80006096, which holds the next instruction, using the kernel page table (t1 holds the address of the kernel page table; see trampoline.S). How come that this address 0x80006096 in both the user and the kernel page table translates to a physical address that holds sfence.vma?

The trampoline.S is mapped into to same va in both the user and the kernel page table.
The va of sfence.vma points to the same physical address by translation of both the user and the kernel page table.
