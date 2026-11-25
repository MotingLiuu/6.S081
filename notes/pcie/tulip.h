#ifndef TULIP_H
#define TULIP_H

#include <stdint.h>

/*
 * DEC 21140/21143 (Tulip) 教学驱动。
 *
 * 网卡上有两套完全不同的接口：
 *
 *   1. CSR 寄存器：CPU 通过 BAR1 的 MMIO 读写，用来控制网卡、看状态。
 *      CSR n 的偏移是 n*8（不是 n*4），16 个 CSR 一共 128 字节。
 *
 *   2. DMA 描述符环：软件在内存里准备描述符和帧缓冲区，网卡作为
 *      PCI bus master 自己去读/写它们，CPU 不需要搬运数据。
 *
 * 理解这两条路径，就理解了网卡驱动。
 *
 * 注意：DMA 还需要 PCI 配置空间 Command 寄存器的 bus master 位已经打开，
 * 以及描述符/缓冲区位于网卡能访问的物理内存里。见 main.c 的实验 3。
 */
#define TULIP_CSR(n) (0x08 * (n))

/* CSR0：总线模式 / 软复位 */
#define TULIP_CSR0_SWR (1u << 0)    /* 写 1 复位整块芯片 */

/*
 * CSR1：Transmit Poll Demand
 * CSR2：Receive Poll Demand
 * 写任意值，让对应的状态机重新扫一遍描述符环。
 */

/* CSR5：状态寄存器，大部分位是写 1 清除 */
#define TULIP_CSR5_TI (1u << 0)     /* transmit interrupt */
#define TULIP_CSR5_TU (1u << 2)     /* transmit underflow：TX 环里没有可发的描述符 */
#define TULIP_CSR5_RI (1u << 6)     /* receive interrupt */
#define TULIP_CSR5_RU (1u << 7)     /* receive unavailable：RX 环里没有空描述符 */
#define TULIP_CSR5_RS_SHIFT 17      /* 接收状态机状态 */
#define TULIP_CSR5_RS_MASK  0x7
#define TULIP_CSR5_TS_SHIFT 20      /* 发送状态机状态 */
#define TULIP_CSR5_TS_MASK  0x7

/* CSR6：操作模式 */
#define TULIP_CSR6_SR (1u << 1)     /* start receive */
#define TULIP_CSR6_PR (1u << 6)     /* promiscuous：接收所有单播帧 */
#define TULIP_CSR6_OM_SHIFT 10      /* 环回模式（0=正常） */
#define TULIP_CSR6_OM_MASK  0x3
#define TULIP_CSR6_ST (1u << 13)    /* start transmit */

/*
 * 描述符：4 个 32 位字，共 16 字节。
 *   status 的 bit31 是 OWN：
 *     OWN=1 -> 描述符归网卡
 *     OWN=0 -> 描述符归 CPU
 * 这就是驱动和硬件之间唯一的"锁"。
 */
struct tulip_desc {
    volatile uint32_t status;   /* 会被网卡 DMA 改写，必须 volatile */
    uint32_t control;
    uint32_t buf1;              /* 缓冲区物理地址（21140 只有 32 位 DMA 地址） */
    uint32_t buf2;              /* 第二个缓冲区，本驱动不用 */
};

#define TULIP_DESC_OWN (1u << 31)

/* 发送描述符 control */
#define TULIP_TDES1_FS  (1u << 29)  /* first segment：帧的第一段 */
#define TULIP_TDES1_LS  (1u << 30)  /* last segment：帧的最后一段 */
#define TULIP_TDES1_IC  (1u << 31)  /* 发送完成时置 CSR5.TI */
#define TULIP_TDES1_TER (1u << 25)  /* 环尾：下一条描述符回到 CSR4 */
#define TULIP_TDES1_SET (1u << 27)  /* setup packet：配置 16 个 MAC 过滤表项 */

/* 接收描述符 control */
#define TULIP_RDES1_RER (1u << 25)  /* 环尾：下一条描述符回到 CSR3 */
#define TULIP_RDES1_SIZE(n) ((uint32_t)(n) & 0x7ff)

/* 接收描述符 status（硬件写回） */
#define TULIP_RDES0_LS (1u << 8)    /* last segment */
#define TULIP_RDES0_FS (1u << 9)    /* first segment */
#define TULIP_RDES0_FL_SHIFT 16     /* 帧长，包含 4 字节 FCS */
#define TULIP_RDES0_FL_MASK  0x3fff

#define TULIP_RING_SIZE 4
#define TULIP_BUF_SIZE  1600

/*
 * QEMU 启动命令里的 mac= 必须和这里一致。
 * 真实驱动会通过 CSR9 的串行 EEPROM 接口把 MAC 读出来。
 */
#define TULIP_MAC_BYTES { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 }

uint32_t tulip_read(uint32_t base, int csr);
void     tulip_write(uint32_t base, int csr, uint32_t val);

void tulip_init(uint32_t base);                    /* 复位 + 建环 + 开收/发 */
int  tulip_send_frame(const void *frame, int len); /* 0 成功，-1 失败 */
int  tulip_recv_frame(void *frame, int maxlen);    /* 帧长；-1 表示暂时没有 */
void tulip_dump(void);                             /* 打印 CSR 和描述符环 */

#endif
