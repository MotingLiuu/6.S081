#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/*
 * QEMU -M virt 把 PCIe 的配置空间映射成一段普通内存：
 *   ECAM (Enhanced Configuration Access Mechanism) 基址 0x30000000。
 * 访问配置空间 = 往这段内存读写，不需要 x86 那种 in/out 指令。
 */
#define PCI_ECAM_BASE 0x30000000UL
#define PCI_MMIO_BASE 0x40000000UL
#define PCI_MMIO_END  0x80000000UL

/* 配置空间头部里我们关心的字段偏移 */
#define PCI_VENDOR_ID   0x00
#define PCI_DEVICE_ID   0x02
#define PCI_COMMAND     0x04
#define PCI_STATUS      0x06
#define PCI_REVISION    0x08
#define PCI_PROG_IF     0x09
#define PCI_SUBCLASS    0x0a
#define PCI_CLASS       0x0b
#define PCI_HEADER_TYPE 0x0e
#define PCI_BAR0        0x10
#define PCI_SUBSYS_VEN  0x2c
#define PCI_SUBSYS_ID   0x2e
#define PCI_IRQ_LINE    0x3c
#define PCI_IRQ_PIN     0x3d

/* Command 寄存器里控制"设备是否响应某类地址"的位 */
#define PCI_CMD_IO_SPACE   (1u << 0)
#define PCI_CMD_MEM_SPACE  (1u << 1)
#define PCI_CMD_BUS_MASTER (1u << 2)

struct pci_addr {
    int bus, dev, func;
};

struct pci_device {
    struct pci_addr addr;
    uint16_t vendor, device;
    uint16_t command, status;
    uint8_t class_code, subclass, prog_if, header_type;
    uint8_t irq_line, irq_pin;

    uint32_t bar[6];        /* BAR 寄存器的原始内容 */
    uint64_t bar_size[6];   /* 探测出来的窗口大小 */
    uint64_t bar_addr[6];   /* 我们分配给它的物理地址，0 = 未分配 */
};

uint32_t pci_ecam_addr(int bus, int dev, int func, int off);
uint32_t pci_read32(int bus, int dev, int func, int off);
uint16_t pci_read16(int bus, int dev, int func, int off);
uint8_t  pci_read8(int bus, int dev, int func, int off);
void     pci_write32(int bus, int dev, int func, int off, uint32_t val);
void     pci_write16(int bus, int dev, int func, int off, uint16_t val);

void pci_read_header(struct pci_device *d);
void pci_probe_bars(struct pci_device *d);
int  pci_scan(struct pci_device *out, int max);

#endif
