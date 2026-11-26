    .section .text.entry
    .globl _start
_start:
    la      sp, _stack_top
    /* a0=hartid, a1=FDT 地址由 QEMU 按 RISC-V 启动协议放好，直接传给 main */
    call    main
1:
    wfi
    j       1b

    /*
     * M 模式陷阱入口。mtvec 只有一个地址，所有异常都先落到这里。
     *
     * 这里必须保存全部 caller-saved 寄存器：trap_report() 会调用
     * kprintf()，而 kprintf 完全可能用掉 a0-a7 和 t0-t6。
     * 不保存就会把"被打断的代码"正在使用的值改掉。
     */
    .align  4
    .globl  trap_entry
trap_entry:
    addi    sp, sp, -144
    sd      ra,   0(sp)
    sd      t0,   8(sp)
    sd      t1,  16(sp)
    sd      t2,  24(sp)
    sd      t3,  32(sp)
    sd      t4,  40(sp)
    sd      t5,  48(sp)
    sd      t6,  56(sp)
    sd      a0,  64(sp)
    sd      a1,  72(sp)
    sd      a2,  80(sp)
    sd      a3,  88(sp)
    sd      a4,  96(sp)
    sd      a5, 104(sp)
    sd      a6, 112(sp)
    sd      a7, 120(sp)

    csrr    a0, mcause
    csrr    a1, mepc
    csrr    a2, mtval
    call    trap_report

    csrw    mepc, a0          # trap_report 返回"应该返回到哪条指令"

    ld      ra,   0(sp)
    ld      t0,   8(sp)
    ld      t1,  16(sp)
    ld      t2,  24(sp)
    ld      t3,  32(sp)
    ld      t4,  40(sp)
    ld      t5,  48(sp)
    ld      t6,  56(sp)
    ld      a0,  64(sp)
    ld      a1,  72(sp)
    ld      a2,  80(sp)
    ld      a3,  88(sp)
    ld      a4,  96(sp)
    ld      a5, 104(sp)
    ld      a6, 112(sp)
    ld      a7, 120(sp)
    addi    sp, sp, 144
    mret
