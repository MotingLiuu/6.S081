#include <stdint.h>
#include "console.h"
#include "mmio.h"
#include "trap.h"

volatile int trap_expected;
volatile int trap_taken;
volatile uint64_t trap_cause;
volatile uint64_t trap_epc;
volatile uint64_t trap_addr;

/* trap_entry 在 start.S 里，用 mret 返回时 mepc 由这里算好。 */
uint64_t trap_report(uint64_t cause, uint64_t epc, uint64_t tval)
{
    trap_cause = cause;
    trap_epc = epc;
    trap_addr = tval;
    trap_taken = 1;

    if (!trap_expected) {
        kprintf("\n!! 未预期的异常: %s mcause=%lu mepc=%p mtval=%p\n",
                trap_name(cause), cause, (void *)epc, (void *)tval);
        for (;;)
            ;
    }

    /* 跳过出错的那条指令：RISC-V 指令 2 字节或 4 字节 */
    uint16_t insn = *(volatile uint16_t *)epc;
    return epc + ((insn & 0x3) == 0x3 ? 4 : 2);
}

void trap_init(void)
{
    extern void trap_entry(void);
    __asm__ volatile("csrw mtvec, %0" : : "r"(trap_entry));
}

const char *trap_name(uint64_t cause)
{
    switch (cause) {
    case 0: return "instruction address misaligned";
    case 1: return "instruction access fault";
    case 2: return "illegal instruction";
    case 3: return "breakpoint";
    case 4: return "load address misaligned";
    case 5: return "load access fault";
    case 6: return "store address misaligned";
    case 7: return "store access fault";
    default: return "other trap";
    }
}

int try_read32(uint64_t addr, uint32_t *out)
{
    __asm__ volatile("" ::: "memory");
    trap_expected = 1;
    trap_taken = 0;

    uint32_t val = mmio_read32(addr);

    __asm__ volatile("" ::: "memory");
    trap_expected = 0;

    if (trap_taken)
        return -1;

    *out = val;
    return 0;
}
