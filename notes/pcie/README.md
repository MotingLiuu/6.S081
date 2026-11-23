# PCIe / 配置空间 / BAR 教学实验

一个 RISC-V 裸机程序，在 QEMU 上真实地扫描 PCIe 总线、探测和分配 BAR、
然后通过 MMIO 读写 Tulip 网卡的寄存器。没有操作系统，没有驱动框架，
所有地址都自己算。

```sh
xmake        # 编译 → build/cross/riscv64/release/pcie.{elf,bin}
xmake run    # 在 QEMU 里跑（Ctrl-A X 退出）
xmake clean  # 清理
```

需要 `xmake`、`riscv64-unknown-elf-gcc` 和 `qemu-system-riscv64`。
仓库里还留了一份等价的 `Makefile`，`make && make run` 同样可用。

---

## 一、先建立整体图像

一块 x86 或 RISC-V 电脑的主板大致是这样：

```
        CPU
         │
    ┌────┴─────┐
    │ 内存控制器 │─── RAM
    └────┬─────┘
         │  系统总线 / 互连
    ┌────┴──────────────────────┐
    │      PCIe 控制器（Root    │
    │      Complex，含桥）       │
    └────┬──────────────────────┘
         │  PCIe 链路（点对点串行）
    ┌────┴────┐   ┌─────────┐
    │  网卡    │   │  显卡    │ ...
    └─────────┘   └─────────┘
```

关键点：**CPU 只会"读/写某个物理地址"这一种操作**。它不认识网卡、不认识
硬盘。要控制设备，只有两条路：

1. 读写设备暴露出来的那段物理地址（MMIO）
2. 特殊的总线周期（x86 的 `in`/`out` 指令访问 I/O 空间；RISC-V 没有）

所以"驱动"本质上就是：**搞清楚该往哪个地址写什么值**。

## 二、PCIe 是什么

PCIe = Peripheral Component Interconnect Express，是 CPU 连接外设的
**总线标准**。它规定了：

- 物理上：用几根差分线、跑多快（x1/x4/x16 通道）
- 电气上：信号怎么编码
- **协议上：设备怎么被发现、地址怎么分配、数据怎么搬**

它是 PCI/PCI-X 的后继。软件上几乎完全兼容 PCI —— 同样的配置空间布局、
同样的 BAR 机制。区别主要是物理层换成了串行点对点链路，以及增加了
更复杂的错误报告和热插拔能力。

传统 PCI 是**共享总线**（所有设备挂在一根线上，要仲裁谁先说话）。
PCIe 是**点对点**：每个设备有独立链路，通过交换机（switch）连到
Root Complex。软件模型上看还是一条"总线"，所以叫 PCIe 总线。

## 三、配置空间是什么

这是整个体系的入口。每个 PCIe 设备（更准确说每个 **function**）内部
都有一块 **4 KiB 的寄存器区**，叫配置空间。它不是设备的功能寄存器，
而是一张**身份卡 + 地址申请表**：

```
偏移    内容
0x00    Vendor ID     厂商号（DEC=0x1011, Intel=0x8086, Red Hat=0x1b36）
0x02    Device ID     产品号（Tulip 21140 = 0x0019）
0x04    Command       译码开关（见下）
0x06    Status
0x08    Revision / Programming Interface
0x09    Class Code    设备类型（0x02=网络, 0x03=显示, 0x01=存储）
0x0e    Header Type
0x10    BAR0          ┐
0x14    BAR1          │ 6 个地址窗口申请表
0x18    BAR2          │
0x1c    BAR3          │
0x20    BAR4          │
0x24    BAR5          ┘
0x2c    Subsystem Vendor/ID
0x3c    IRQ Line / Pin
```

**怎么访问它？** 这是最容易困惑的地方。

老 PCI 用两个 I/O 端口（x86 上 `0xCF8` 写地址、`0xCFC` 读写数据），叫
CAM。PCIe 引入了 **ECAM**（Enhanced Configuration Access Mechanism）：
把**所有设备的配置空间平铺成一段连续物理内存**。于是访问配置空间就是
普通的 load/store：

```
配置空间地址 = ECAM 基址
             + (bus  << 20)     // 8 位总线号
             + (dev  << 15)     // 5 位设备号
             + (func << 12)     // 3 位功能号
             + offset           // 12 位，4 KiB
```

在 QEMU `-M virt` 上 ECAM 基址是 `0x30000000`，这段映射大小是 256 MiB
（`2^28`）。`pci.c` 的 `pci_ecam_addr()` 就是这一行公式。

扫描时对每个 (bus, dev, func) 读 Vendor ID：读到 `0xffff` 说明这个槽位
没人（PCI 约定：无人应答返回全 1）。

> 实践坑：QEMU virt 的 ECAM **只支持 4 字节访问**。你用 2 字节读
> Vendor ID，它会把 4 字节读出来取低半部分，于是 Device ID 读出来和
> Vendor ID 一样。`pci.c` 里所有访问都统一走 32 位再移位，就是这个原因。

## 四、BAR 是什么，有什么用

**BAR = Base Address Register**，配置空间里 6 个 32 位寄存器。

问题在于：设备需要一段物理地址来做自己的寄存器窗口（Tulip 网卡的 CSR、
显卡的显存、NVMe 的队列门铃……），但设备**上电时并不知道该用哪个地址**。
地址是由系统软件（BIOS/UEFI/内核）统一分配的，就像 DHCP 分配 IP。

所以 BAR 是一个**双向的寄存器**：

- 系统软件**写**它：告诉设备"你的窗口从 0x40000000 开始"
- 系统软件**读**它：确认设备记住了
- 设备自己则用它做地址译码：收到的访问落在窗口内就应答

### 怎么知道窗口要多大

设备不能直接说"我要 128 字节"，因为 BAR 里没有"大小"字段。它用的方法
很巧妙，利用只读位：

1. 系统软件往 BAR 写 `0xFFFFFFFF`
2. 设备只保存**地址位**，低 N 位（N 对应窗口大小）硬连线为 0
3. 读回来得到类似 `1111_1111_1111_1111_1111_1111_1000_0000`
4. 低位有 7 个 0 → 窗口是 `2^7 = 128` 字节
5. **把原值写回去**（重要！否则设备地址就毁了）

`pci.c` 的 `bar_size()` 就是这段。注意两个细节：

- 低 4 位是标志位，不参与大小计算，要先抹掉
- 32 位 BAR 必须在 32 位内取反。若用 64 位取反会得到
  `0xffffffff00000080` 这种荒唐结果

### 三种 BAR

| 低 2 位 | 类型 | 说明 |
|---|---|---|
| `00` | 32 位 MMIO | 最常见 |
| `10` | 64 位 MMIO | 占**两个** BAR 槽位（低位在 BARn，高位在 BARn+1） |
| `01` | I/O 空间 | 老式端口 I/O，只有 x86 有意义，PCIe 里已废弃 |

Tulip 网卡（QEMU 里的 21140）正好两种都有：

- BAR0 = I/O BAR，128 字节
- BAR1 = 32 位 MMIO BAR，128 字节 ← 网卡 CSR 就通过它访问

## 五、PCIe MMIO 窗口是什么

现在有三个问题：BAR 大小知道了，可地址从哪儿来？

现代系统里有一段物理地址是**专门预留给 PCIe 设备的**，叫 MMIO 窗口。
主板固件通过 ACPI/设备树把它告诉操作系统。在 QEMU `-M virt` 上，设备树
里写着：

```
pci@30000000 {
    reg    = <0x00 0x30000000 0x00 0x10000000>;   // ECAM: 256 MiB
    ranges = <0x01000000 0x00 0x00000000
              0x00       0x03000000
              0x00       0x00010000>,             // I/O 窗口:  0x03000000, 64 KiB
             <0x02000000 0x00 0x40000000
              0x00       0x40000000
              0x00       0x40000000>,             // 32 位 MEM: 0x40000000, 1 GiB
             <0x03000000 0x04 0x00000000
              0x04       0x00000000
              0x04       0x00000000>;             // 64 位 MEM: 0x4_00000000, 16 GiB
}
```

`ranges` 每项是「子空间类型 + 子地址 → 父地址 + 长度」。三行分别是
I/O、32 位内存、64 位内存三个窗口。

**32 位 MMIO 窗口 = 物理地址 `0x40000000` – `0x80000000`，共 1 GiB。**
这就是本实验给设备分配 BAR 的地方（设备 BAR 大多是 32 位的，只能落在
4 GiB 以下的地址）。64 位窗口在 16 GiB 处，供需要 64 位 BAR 的设备使用。

所以完整的分配流程是：

```
1. 扫描配置空间，找到设备
2. 探测每个 BAR 大小
3. 在窗口内挑一段大小合适、按大小对齐的空闲地址
4. 写入 BAR
5. 打开 Command 寄存器的 bit1 (Memory Space Enable)
6. 此后 CPU 访问那段地址，就真的通到设备寄存器了
```

注意 **第 5 步不能忘**。BAR 里写了地址但 Command 的译码位是 0，设备仍然
装死。本实验跑之前 `Command=0000`，写完变成 `0007`（I/O + MMIO + bus master）。

### 窗口外 vs 窗口内没人认领

有意思的对照实验（`demo_negative()`）：

- 读 `0x00000000`：物理地址不存在 → **access fault 异常**
- 读 `0x50000000`：在 MMIO 窗口内但没设备认领 → 读到 `0xffffffff`

后者是 PCIe 控制器的行为：它替无人应答的设备做了答复（unassigned
request），返回全 1。这也是为什么扫描时判断 `vendor == 0xffff` 表示
"这里没设备"。

## 六、硬件访问在 C 里长什么样

就一句：**往地址写，从地址读**。`mmio.h` 里的封装：

```c
static inline uint32_t mmio_read32(uint64_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write32(uint64_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}
```

**`volatile` 不能省**。没有它，编译器认为"读一个从没被写过的变量"是
无意义的，可能直接删掉，或者把相邻两次写合并 —— 而对硬件来说，两次
写是不同的操作，合并就错了。

## 七、Tulip 网卡

DEC 21140 "Tulip" 是 1990 年代的 100M 网卡。QEMU 完整模拟了它。
它的寄存器叫 CSR0–CSR15，**间隔 8 字节**（不是 4）：

```c
#define TULIP_CSR(n) (0x08 * (n))
```

所以 CSR5 在 `base + 0x28`。本实验实测（QEMU 21140 的 reset 值）：

```
CSR5 = f0000000   状态
CSR6 = 32000040   操作模式
CSR7 = f3fe0000   中断屏蔽
```

往 CSR6 的 bit13（ST，Start Transmit）写 1 再读回来，值变成 `32002040`
—— 差异正好是 `0x2000`，说明这个位被硬件保存了。这就是 MMIO 写生效的
直接证据。

> 这是 xv6 网络实验（lab net）的主角。在那个 lab 里你要做的正是：扫描
> 配置空间找 21140、分配 BAR、打开 bus master、建 DMA 环形队列、
> 收发以太网帧。这个小程序把前面几步单独拆出来了。

## 八、代码结构

```
pcie/
├── xmake.lua       构建脚本（交叉编译器、链接脚本、objcopy、QEMU 启动）
├── Makefile        等价的 make 版本，二选一
├── linker.ld       链接脚本，代码放在 0x80000000
├── start.S         入口、栈、M 模式陷阱入口
├── main.c          五个实验的主流程
├── console.c/.h    UART 输出 + 迷你 printf + hexdump
├── mmio.h          volatile 读写封装
├── pci.c/.h        ECAM 访问、扫描、BAR 探测
├── trap.c/.h       陷阱处理，用来安全地试读地址
└── tulip.c/.h      Tulip CSR 读写
```

## 九、实验输出长什么样

```
扫描完成：找到 3 个 PCIe 设备/功能

  [00:00.0] vendor=1b36 device=0008  bridge (class 06:00)
  [00:01.0] vendor=1011 device=0019  network (class 02:00)   ← Tulip
  [00:02.0] vendor=1234 device=11e8  old device (class 00:ff) ← edu 教学设备

    BAR0: I/O  原始值=00000001  大小=128 字节
    BAR1: MEM  原始值=00000000  大小=128 字节 (2^7)
    BAR0: MEM  原始值=00000000  大小=1048576 字节 (2^20)     ← edu

  [00:01.0] BAR1: 分配 0x0000000040000000 -> 读回 40000000  OK
  [00:02.0] BAR0: 分配 0x0000000040100000 -> 读回 40100000  OK

  [00:01.0] Command=0000  I/O=0  MMIO=0  总线主控=0
  写入后 Command=0007，网卡从现在开始应答 MMIO 访问。

  Tulip 的 CSR 寄存器块现在在 0x0000000040000000
  CSR5 (状态)     = f0000000
  CSR6 (操作模式) = 32000040
  CSR7 (中断屏蔽) = f3fe0000
  往 CSR6 的 bit13 (ST) 写 1 后读回 = 32002040
```

对照 `-device help` 里的 `1011:0019` 和 QEMU monitor 的 `info pci`，
所有数字都能对上。

## 十、动手改

想加深理解，可以试：

1. 把 `pci.c` 里的 `pci_read16` 改回真正的 16 位 MMIO 读
   （`mmio_read16`），观察 Device ID 怎么变成错的 —— 理解 ECAM 的限制
2. 把 BAR 分配地址从 `0x40000000` 改成 `0x40000004`（不按大小对齐），
   看 QEMU 是不是拒绝
3. 分配完地址但**不**打开 Command bit1，再读 CSR5，看会发生什么
4. 加 `-device e1000`，用同样的扫描代码看 Intel 网卡的 BAR（`8086:100e`）
5. 用 `-device edu`（QEMU 教学设备，`1234:11e8`）练习：它的 BAR 是
   1 MiB，读写它的寄存器会有可见的反应

## 十一、常用命令

```sh
# 看 QEMU 分配的真实 BAR 地址（需要 guest 自己没分配时显示 not mapped）
echo "info pci" | qemu-system-riscv64 -M virt -device tulip -display none -serial none -monitor stdio

# 看设备树的 PCIe 窗口
qemu-system-riscv64 -M virt,dumpdtb=- -display none -serial none | dtc -I dtb -O dts -

# 看设备支持哪些
qemu-system-riscv64 -device help | grep -i -E "tulip|e1000|edu"
```
