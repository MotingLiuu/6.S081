#include <stdint.h>
#include "pci.h"
#include "console.h"
#include "mmio.h"

/*
 * 配置空间地址 = ECAM 基址 + (bus << 20) + (dev << 15) + (func << 12) + offset
 *
 * 为什么是这些位数？
 *   offset 12 位 -> 每个 function 有 4 KiB 配置空间
 *   func    3 位 -> 每个 device 最多 8 个 function
 *   dev     5 位 -> 每条总线最多 32 个 device
 *   bus     8 位 -> 最多 256 条总线
 * 合计 12+3+5+8 = 28 位，正好 256 MiB。
 */
uint32_t pci_ecam_addr(int bus, int dev, int func, int off)
{
    return PCI_ECAM_BASE | ((uint32_t)bus << 20) | ((uint32_t)dev << 15) |
           ((uint32_t)func << 12) | ((uint32_t)off & 0xffc);
}

/*
 * 所有访问都走 32 位，再用移位取出需要的字节/半字。
 *
 * 这不是偷懒。QEMU virt 的 ECAM 实现只接受 4 字节访问：一条 2 字节的
 * 配置读会被当成"读对齐的 4 字节然后取低 16 位"，于是 vendor ID 和
 * device ID 读出来一模一样。改成 32 位读 + 移位后立刻正确。
 * 真实的 PCIe 规范允许 1/2/4 字节访问，但 32 位是最保险的写法。
 */
uint32_t pci_read32(int bus, int dev, int func, int off)
{
    return mmio_read32(pci_ecam_addr(bus, dev, func, off));
}

uint16_t pci_read16(int bus, int dev, int func, int off)
{
    uint32_t v = pci_read32(bus, dev, func, off & ~0x3);

    return (uint16_t)(v >> ((off & 0x2) * 8));
}

uint8_t pci_read8(int bus, int dev, int func, int off)
{
    uint32_t v = pci_read32(bus, dev, func, off & ~0x3);

    return (uint8_t)(v >> ((off & 0x3) * 8));
}

void pci_write32(int bus, int dev, int func, int off, uint32_t val)
{
    mmio_write32(pci_ecam_addr(bus, dev, func, off), val);
}

void pci_write16(int bus, int dev, int func, int off, uint16_t val)
{
    int shift = (off & 0x2) * 8;
    uint32_t old = pci_read32(bus, dev, func, off & ~0x3);
    uint32_t mask = 0xffffu << shift;

    pci_write32(bus, dev, func, off & ~0x3,
                (old & ~mask) | ((uint32_t)val << shift));
}

static int bar_off(int idx)
{
    return PCI_BAR0 + idx * 4;
}

void pci_read_header(struct pci_device *d)
{
    int b = d->addr.bus, dev = d->addr.dev, f = d->addr.func;

    d->vendor      = pci_read16(b, dev, f, PCI_VENDOR_ID);
    d->device      = pci_read16(b, dev, f, PCI_DEVICE_ID);
    d->command     = pci_read16(b, dev, f, PCI_COMMAND);
    d->status      = pci_read16(b, dev, f, PCI_STATUS);
    d->prog_if     = pci_read8(b, dev, f, PCI_PROG_IF);
    d->subclass    = pci_read8(b, dev, f, PCI_SUBCLASS);
    d->class_code  = pci_read8(b, dev, f, PCI_CLASS);
    d->header_type = pci_read8(b, dev, f, PCI_HEADER_TYPE);
    d->irq_line    = pci_read8(b, dev, f, PCI_IRQ_LINE);
    d->irq_pin     = pci_read8(b, dev, f, PCI_IRQ_PIN);

    for (int i = 0; i < 6; i++) {
        d->bar[i]      = pci_read32(b, dev, f, bar_off(i));
        d->bar_size[i] = 0;
        d->bar_addr[i] = 0;
    }
}

/*
 * BAR 探测（以 32 位内存 BAR 为例）：
 *
 *   1. 保存 BAR 原值
 *   2. 写入 0xffffffff
 *   3. 读回来 —— 硬件只保存"地址位"，低位（大小对应的位）固定为 0
 *      所以读回值形如 1111...1111 0000 0000，低位有几个 0，窗口就是多大
 *   4. 抹掉低 4 位标志位，取反加一 = 大小
 *   5. 把原值写回去
 *
 * 这一步"写 0xffffffff"会临时改掉 BAR 里的地址。本实验里设备的地址译码
 * 还没打开（Command 寄存器 = 0），所以不会误伤；真实系统也要求在使能
 * 译码之前完成 BAR 探测和分配。
 *
 * 注意 32 位和 64 位 BAR 要分开算大小：32 位 BAR 取反只能在 32 位内做，
 * 否则会得到 0xffffffff00000010 这种荒唐的大小。
 */
static uint64_t bar_size(int b, int dev, int f, int idx, uint32_t *raw_out)
{
    uint32_t save_lo = pci_read32(b, dev, f, bar_off(idx));
    uint32_t save_hi = 0;
    uint64_t size = 0;

    pci_write32(b, dev, f, bar_off(idx), 0xffffffffu);
    uint32_t lo = pci_read32(b, dev, f, bar_off(idx));

    int io    = lo & 0x1;
    int type  = (lo >> 1) & 0x3;              /* 0=32 位, 2=64 位 */
    int is_64 = !io && type == 0x2 && idx < 5;

    if (is_64) {
        save_hi = pci_read32(b, dev, f, bar_off(idx + 1));
        pci_write32(b, dev, f, bar_off(idx + 1), 0xffffffffu);
    }

    if (is_64) {
        uint32_t hi = pci_read32(b, dev, f, bar_off(idx + 1));
        uint64_t mask = ((uint64_t)hi << 32) | (lo & ~0xfu);

        if (mask)
            size = ~mask + 1;
    } else if (!io) {
        uint32_t mask = lo & ~0xfu;           /* 低位 4 位是标志位，不参与 */

        if (mask)
            size = (uint32_t)(~mask + 1);     /* 在 32 位内取反 */
    } else {
        uint32_t mask = lo & ~0x3u;           /* I/O BAR 只有低 2 位是标志 */

        if (mask)
            size = (uint32_t)(~mask + 1);
    }

    pci_write32(b, dev, f, bar_off(idx), save_lo);
    if (is_64)
        pci_write32(b, dev, f, bar_off(idx + 1), save_hi);

    *raw_out = save_lo;
    return size;
}

void pci_probe_bars(struct pci_device *d)
{
    int b = d->addr.bus, dev = d->addr.dev, f = d->addr.func;

    for (int i = 0; i < 6; i++) {
        uint32_t raw;
        uint64_t size = bar_size(b, dev, f, i, &raw);

        d->bar[i]      = raw;
        d->bar_size[i] = size;

        /* 64 位 BAR 占两个槽位，高 32 位不单独处理 */
        if (!(raw & 1) && ((raw >> 1) & 0x3) == 0x2)
            i++;
    }
}

/* 扫描总线，返回找到的设备数 */
int pci_scan(struct pci_device *out, int max)
{
    int n = 0;

    for (int bus = 0; bus < 256 && n < max; bus++)
        for (int dev = 0; dev < 32 && n < max; dev++)
            for (int func = 0; func < 8 && n < max; func++) {
                uint16_t vendor = pci_read16(bus, dev, func, PCI_VENDOR_ID);

                /* 没有设备时读回 0xffff，这是 PCI 的"没人应答"约定 */
                if (vendor == 0xffff || vendor == 0x0000)
                    continue;

                out[n].addr.bus  = bus;
                out[n].addr.dev  = dev;
                out[n].addr.func = func;
                pci_read_header(&out[n]);
                pci_probe_bars(&out[n]);
                n++;

                /* function 0 的 header type bit7 为 0 表示单功能设备 */
                if (func == 0 && !(out[n - 1].header_type & 0x80))
                    break;
            }
    return n;
}

void pci_assign_bars(struct pci_device *devs, int n, int verbose)
{
    uint64_t next = PCI_MMIO_BASE;

    for (int i = 0; i < n; i++) {
        struct pci_device *d = &devs[i];

        for (int b = 0; b < 6; b++) {
            uint32_t raw = d->bar[b];
            uint64_t size = d->bar_size[b];

            if (size == 0)
                continue;
            if (raw & 1) {
                if (verbose)
                    kprintf("  [%02x:%02x.%x] BAR%d 是 I/O BAR，本实验跳过\n",
                            d->addr.bus, d->addr.dev, d->addr.func, b);
                continue;
            }

            /* 地址必须按大小对齐 */
            next = (next + size - 1) & ~(size - 1);

            /* 保留低 4 位标志，只替换地址部分 */
            uint32_t value = (uint32_t)next | (raw & 0xf);
            pci_write32(d->addr.bus, d->addr.dev, d->addr.func,
                        PCI_BAR0 + b * 4, value);

            /* 读回验证，确认设备真的记住了 */
            uint32_t check = pci_read32(d->addr.bus, d->addr.dev,
                                        d->addr.func, PCI_BAR0 + b * 4);

            if (verbose)
                kprintf("  [%02x:%02x.%x] BAR%d: 分配 %p -> 读回 %08x  %s\n",
                        d->addr.bus, d->addr.dev, d->addr.func, b,
                        (void *)next, check, check == value ? "OK" : "失败");

            d->bar_addr[b] = next;
            next += size;
        }
    }
}

void pci_enable_decoding(struct pci_device *devs, int n, int verbose)
{
    for (int i = 0; i < n; i++) {
        struct pci_device *d = &devs[i];
        uint16_t cmd;

        if (d->vendor != 0x1011)
            continue;

        cmd = pci_read16(d->addr.bus, d->addr.dev, d->addr.func,
                         PCI_COMMAND);
        if (verbose)
            kprintf("  [%02x:%02x.%x] Command=%04x  I/O=%d  MMIO=%d  总线主控=%d\n",
                    d->addr.bus, d->addr.dev, d->addr.func, cmd,
                    !!(cmd & PCI_CMD_IO_SPACE), !!(cmd & PCI_CMD_MEM_SPACE),
                    !!(cmd & PCI_CMD_BUS_MASTER));

        /*
         * bit0: 设备响应 I/O BAR 的访问
         * bit1: 设备响应 Memory BAR 的访问
         * bit2: 允许设备主动发起 PCI transaction（DMA）
         */
        pci_write16(d->addr.bus, d->addr.dev, d->addr.func, PCI_COMMAND,
                    cmd | PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE |
                        PCI_CMD_BUS_MASTER);

        cmd = pci_read16(d->addr.bus, d->addr.dev, d->addr.func, PCI_COMMAND);
        if (verbose)
            kprintf("  写入后 Command=%04x，网卡从现在开始应答 MMIO 访问。\n",
                    cmd);
    }
}
