#include <stdint.h>
#include <stddef.h>
#include "cmdline.h"
#include "console.h"
#include "mmio.h"
#include "pci.h"
#include "trap.h"
#include "tulip.h"

#define MAX_DEV 32

static const char *class_name(uint8_t cls)
{
    switch (cls) {
    case 0x00: return "old device";
    case 0x01: return "mass storage";
    case 0x02: return "network";
    case 0x03: return "display";
    case 0x06: return "bridge";
    default:   return "unknown";
    }
}

static int ilog2(uint64_t v)
{
    int n = 0;

    while (v > 1) {
        v >>= 1;
        n++;
    }
    return n;
}

static void print_header(const struct pci_device *d)
{
    struct pci_addr a = d->addr;

    kprintf("\n  [%02x:%02x.%x] vendor=%04x device=%04x  %s (class %02x:%02x)\n",
            a.bus, a.dev, a.func, d->vendor, d->device,
            class_name(d->class_code), d->class_code, d->subclass);

    /* 32 位读配置空间，自己拆成字节 */
    uint8_t raw[64];
    for (int i = 0; i < 64; i += 4) {
        uint32_t v = pci_read32(a.bus, a.dev, a.func, i);
        raw[i + 0] = v;
        raw[i + 1] = v >> 8;
        raw[i + 2] = v >> 16;
        raw[i + 3] = v >> 24;
    }
    kprintf("    配置空间前 64 字节:\n");
    hexdump(raw, 64);
}

static void print_bars(const struct pci_device *d)
{
    for (int i = 0; i < 6; i++) {
        uint32_t raw = d->bar[i];
        uint64_t size = d->bar_size[i];

        if (raw == 0 && size == 0)
            continue;

        if (raw & 1)
            kprintf("    BAR%d: I/O  原始值=%08x  大小=%lu 字节\n", i, raw, size);
        else
            kprintf("    BAR%d: MEM  原始值=%08x  大小=%lu 字节 (2^%d)\n",
                    i, raw, size, size ? ilog2(size) : 0);
    }
}

/*
 * 实验 1：读配置空间。
 * 这一步不需要给设备分配任何地址，因为配置空间本身在固定的 ECAM 窗口里。
 */
static void demo_config_space(struct pci_device *devs, int n)
{
    kprintf("=========================================================\n");
    kprintf(" 实验 1：配置空间是什么\n");
    kprintf("=========================================================\n");
    kprintf("配置空间 = 每个 PCIe 设备都自带的一张 4 KiB 登记表。\n");
    kprintf("在 QEMU virt 上它被映射到物理地址 %p，叫 ECAM。\n",
            (void *)PCI_ECAM_BASE);
    kprintf("  ECAM + (bus<<20) + (dev<<15) + (func<<12) + offset\n");
    kprintf("表里写着：厂商 ID、设备 ID、设备类型、以及 6 个 BAR。\n");

    for (int i = 0; i < n; i++)
        print_header(&devs[i]);
}

/*
 * 实验 2：BAR 探测。
 * 设备自己不知道应该占用哪段物理地址 —— 那是系统软件决定的。
 * BAR 就是设备身上"我要一段地址"的可写寄存器。
 */
static void demo_bar_probe(struct pci_device *devs, int n)
{
    kprintf("\n=========================================================\n");
    kprintf(" 实验 2：BAR 是什么（此时还没分配地址）\n");
    kprintf("=========================================================\n");

    for (int i = 0; i < n; i++) {
        kprintf("devs %d\n", i);
        print_bars(&devs[i]);
    }

    kprintf("\n『原始值』就是 BAR 寄存器现在的完整内容。\n");
    kprintf("低 4 位是标志（1=I/O, 2=64 位地址），其余是地址。\n");
    kprintf("『大小』是往 BAR 写全 1 再读回来算出来的，见 pci.c。\n");
}

static void demo_mmio_window(struct pci_device *devs, int n)
{
    kprintf("\n=========================================================\n");
    kprintf(" 实验 3：PCIe MMIO 窗口 + 给 BAR 分配地址\n");
    kprintf("=========================================================\n");
    kprintf("MMIO 窗口 = 一段专门留给 PCIe 设备的物理地址：%p - %p。\n",
            (void *)PCI_MMIO_BASE, (void *)PCI_MMIO_END);
    kprintf("CPU 读写这个范围，PCIe 控制器会把访问转成 PCIe 事务发出去。\n\n");

    pci_assign_bars(devs, n, 1);

    /*
     * Command 寄存器 bit0/bit1 是地址译码开关。
     * 就算 BAR 里写了地址，不开这两位设备也不会应答。
     */
    kprintf("\n地址写进 BAR 还不够，还要打开 Command 寄存器的译码位：\n");
    pci_enable_decoding(devs, n, 1);
}

static void demo_tulip(struct pci_device *devs, int n)
{
    uint32_t base = 0;

    for (int i = 0; i < n; i++)
        if (devs[i].vendor == 0x1011 && devs[i].bar_addr[1])
            base = (uint32_t)devs[i].bar_addr[1];

    kprintf("\n=========================================================\n");
    kprintf(" 实验 4：通过 MMIO 读 Tulip 网卡寄存器\n");
    kprintf("=========================================================\n");

    if (!base) {
        kprintf("没找到可用的 Tulip 内存 BAR。\n");
        return;
    }

    kprintf("Tulip 的 CSR 寄存器块现在在 %p。\n", (void *)(uint64_t)base);
    kprintf("每个 CSR 间隔 8 字节，CSR5 就在 base+0x28。\n\n");

    uint32_t csr5 = tulip_read(base, 5);
    kprintf("  CSR5 (状态)     = %08x\n", csr5);

    uint32_t csr6 = tulip_read(base, 6);
    kprintf("  CSR6 (操作模式) = %08x\n", csr6);

    kprintf("  CSR7 (中断屏蔽) = %08x\n", tulip_read(base, 7));

    kprintf("\n往 CSR6 的 bit13 (ST, 启动发送) 写 1，再读回来：\n");
    tulip_write(base, 6, csr6 | (1u << 13));
    uint32_t after = tulip_read(base, 6);
    kprintf("  CSR6 = %08x  (原来 %08x，差异 %08x)\n", after, csr6, after ^ csr6);
    kprintf("写进去的位被硬件保存了下来 —— 这就是 MMIO 写。\n");
}

/* ---- 实验 5 用的小工具：裸机没有 libc，字节序/拷贝都手写 ---- */

static void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static uint16_t get_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, int n)
{
    while (n-- > 0)
        *dst++ = *src++;
}

static void zero_bytes(uint8_t *p, int n)
{
    while (n-- > 0)
        *p++ = 0;
}

static void print_mac(const uint8_t *m)
{
    kprintf("%02x:%02x:%02x:%02x:%02x:%02x",
            m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void print_ip(const uint8_t *p)
{
    kprintf("%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
}

static void print_eth_frame(const uint8_t *f, int len)
{
    uint16_t type;

    if (len < 14) {
        kprintf("  帧太短（%d 字节）\n", len);
        return;
    }

    kprintf("  目的 MAC  ");
    print_mac(f + 0);
    kprintf("\n  源   MAC  ");
    print_mac(f + 6);

    type = get_be16(f + 12);
    kprintf("\n  EtherType %04x", type);
    if (type == 0x0806)
        kprintf(" (ARP)");
    else if (type == 0x0800)
        kprintf(" (IPv4)");
    kprintf("\n");

    if (type == 0x0806 && len >= 42) {
        uint16_t op = get_be16(f + 20);

        kprintf("  ARP %s\n",
                op == 1 ? "request" : op == 2 ? "reply" : "?");
        kprintf("  发送方    ");
        print_mac(f + 22);
        kprintf("  ");
        print_ip(f + 28);
        kprintf("\n  目标      ");
        print_mac(f + 32);
        kprintf("  ");
        print_ip(f + 38);
        kprintf("\n");
    }
}

/*
 * 构造一个广播 ARP 请求：
 *   谁有 10.0.2.2（QEMU user-mode 网络的网关）？请告诉 10.0.2.15。
 * 源 MAC 必须和 QEMU -device tulip,mac=... 里的一致。
 */
static int build_arp_request(uint8_t *f)
{
    static const uint8_t mac[6] = TULIP_MAC_BYTES;
    static const uint8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const uint8_t spa[4] = {10, 0, 2, 15};
    static const uint8_t tpa[4] = {10, 0, 2, 2};

    zero_bytes(f, 60);                  /* 以太网最短帧 60 字节（不含 FCS） */

    // note
    // Ethernet Header
    copy_bytes(f + 0, bcast, 6);        /* 目的：广播 */
    copy_bytes(f + 6, mac, 6);          /* 源：我们的 MAC */
    put_be16(f + 12, 0x0806);           /* EtherType: ARP */

    // note
    // ARP Packet
    put_be16(f + 14, 0x0001);           /* ARP htype: Ethernet */
    put_be16(f + 16, 0x0800);           /* ARP ptype: IPv4 */
    f[18] = 6;                          /* hlen */
    f[19] = 4;                          /* plen */
    put_be16(f + 20, 0x0001);           /* op: request */
    copy_bytes(f + 22, mac, 6);         /* sender MAC */
    copy_bytes(f + 28, spa, 4);         /* sender IP 10.0.2.15 */
    copy_bytes(f + 38, tpa, 4);         /* target IP 10.0.2.2 */

    return 60;
}

static void demo_network(struct pci_device *devs, int n)
{
    uint8_t frame[60];
    uint8_t rx[2048];
    uint32_t base = 0;
    int len;

    for (int i = 0; i < n; i++)
        if (devs[i].vendor == 0x1011 && devs[i].bar_addr[1])
            base = (uint32_t)devs[i].bar_addr[1];

    kprintf("\n=========================================================\n");
    kprintf(" 实验 5：DMA 环形队列 + 真实的 ARP 收发\n");
    kprintf("=========================================================\n");

    if (!base) {
        kprintf("没找到可用的 Tulip 内存 BAR。\n");
        return;
    }

    kprintf("Tulip 从外到内有三层：\n");

    kprintf("控制路径（CPU 主动）：CSR3/CSR4 告诉网卡环在哪，\n");
    kprintf("                      CSR6 的 SR/ST 开收/发，CSR1 踢发送。\n");
    kprintf("数据路径（网卡主动）：网卡按描述符里的物理地址自己 DMA 读写，\n");
    kprintf("                      描述符的 OWN 位就是双方交接缓冲区的标志。\n\n");

    tulip_init(base);

    kprintf("[初始化后] RX 的 4 个描述符全是 OWN=1，等网卡填入；TX 全空闲\n");
    tulip_dump();

    len = build_arp_request(frame);
    kprintf("\n[发送] 广播 ARP：谁有 10.0.2.2？请告诉 10.0.2.15\n");
    if (tulip_send_frame(frame, len) != 0) {
        kprintf("发送失败（TX 描述符一直没被网卡归还）\n");
        return;
    }
    kprintf("[发送后] 网卡已经把 TX 描述符的 OWN 清 0\n");

    /*
     * 在 QEMU user-mode 网络里，网关 10.0.2.2 会回答 ARP。
     * 应答是网卡通过 DMA 直接写进 RX 缓冲区的，CPU 只看 OWN。
     */
    for (int spin = 0; spin < 10000000; spin++) {
        len = tulip_recv_frame(rx, sizeof(rx));
        if (len >= 0)
            break;
    }

    if (len < 0) {
        kprintf("没收到 ARP 应答（检查 QEMU 启动参数里的网络后端）\n");
        return;
    }

    kprintf("\n[接收] 收到 %d 字节，DMA 已经把帧放进内存:\n", len);
    hexdump(rx, len);
    print_eth_frame(rx, len);

    kprintf("\n[重新挂好 RX 描述符]\n");
    tulip_dump();
    kprintf("  TU=1 是 TX 环已空的正常状态，RI=1 是刚收到帧；\n");
    kprintf("  这两个位写 1 可清除，本驱动轮询时不依赖它们。\n");
}

static void demo_negative(void)
{
    uint32_t v;

    kprintf("\n=========================================================\n");
    kprintf(" 实验 6：访问没有被任何设备认领的地址\n");
    kprintf("=========================================================\n");

    if (try_read32(0x80000000UL, &v) == 0)
        kprintf("  读 RAM   0x80000000 -> %08x\n", v);

    if (try_read32(0x00000000UL, &v) != 0)
        kprintf("  读       0x00000000 -> access fault（这段地址不存在）\n");

    /*
     * 0x50000000 在 PCIe MMIO 窗口里，但没有任何设备的 BAR 覆盖它。
     * 这种访问不会 fault，而是由 PCIe 控制器返回 0xffffffff
     * （它替没人应答的设备做了答复）。
     */
    if (try_read32(0x50000000UL, &v) == 0)
        kprintf("  读       0x50000000 -> %08x（窗口内，但没有设备认领）\n", v);

    kprintf("\n对比：窗口外的地址会直接 fault，窗口内没人认领的地址读到全 1。\n");
}

/*
 * QEMU 按 RISC-V 启动协议把 hartid 放在 a0、设备树地址放在 a1。
 * start.S 不动这两个寄存器，所以这里能直接收到它们。
 */
void main(uint64_t hartid, uint64_t fdt)
{
    static struct pci_device devs[MAX_DEV];
    int need_mmio;

    (void)hartid;
    trap_init();
    cmdline_init(fdt);

    kprintf("\n");
    kprintf("########## PCIe / 配置空间 / BAR 教学实验 ##########\n");
    kprintf("QEMU -M virt + tulip 网卡\n");
    kprintf("实验选择: %s\n\n", cmdline_expr());

    int n = pci_scan(devs, MAX_DEV);
    kprintf("扫描完成：找到 %d 个 PCIe 设备/功能\n", n);

    need_mmio = exp_enabled(3) || exp_enabled(4) || exp_enabled(5);

    if (exp_enabled(1))
        demo_config_space(devs, n);
    if (exp_enabled(2))
        demo_bar_probe(devs, n);

    if (exp_enabled(3)) {
        demo_mmio_window(devs, n);
    } else if (need_mmio) {
        /* 实验 4/5 需要 BAR 和 bus master，即使没选实验 3 也要静默完成 */
        pci_assign_bars(devs, n, 0);
        pci_enable_decoding(devs, n, 0);
    }

    if (exp_enabled(4))
        demo_tulip(devs, n);
    if (exp_enabled(5))
        demo_network(devs, n);
    if (exp_enabled(6))
        demo_negative();

    kprintf("\n########## 实验结束 ##########\n");
    kprintf("在 run 模式下按 Ctrl-A X 退出 QEMU\n");
}
