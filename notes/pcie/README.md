# PCIe / 配置空间 / BAR 教学实验

一个 RISC-V 裸机程序，在 QEMU 上真实地扫描 PCIe 总线、探测和分配 BAR、
通过 MMIO 读写 Tulip 网卡的寄存器，最后用 DMA 环形描述符队列发出一个
ARP 请求并收到应答。没有操作系统，没有驱动框架，所有地址都自己算。

```sh
xmake        # 编译 → build/cross/riscv64/release/pcie.{elf,bin}
xmake run    # 在 QEMU 里跑（Ctrl-A X 退出）
xmake clean  # 清理
```

需要 `xmake`、`riscv64-unknown-elf-gcc` 和 `qemu-system-riscv64`。
仓库里还留了一份等价的 `Makefile`，`make && make run` 同样可用。

## 选择实验

默认运行全部 6 个实验；可以用命令行参数只跑自己关心的部分：

```sh
make run EXP=5        # 只跑实验 5（ARP 收发）
make run EXP=4,5      # 跑实验 4 和 5
make run EXP=1-3      # 跑实验 1、2、3
make run EXP=none     # 只扫描 PCIe 设备，不跑实验
make run              # 默认 EXP=all
```

xmake 的写法：

```sh
xmake run pcie 5
xmake run pcie 4,5
xmake run pcie all
```

不需要重新编译，`exp` 只是 QEMU 的启动参数。

参数是怎么到裸机程序的：`make run` 给 QEMU 加了 `-append "exp=5"`，
QEMU 把它放进设备树 `/chosen/bootargs`，并在启动时把 DTB 地址放在 `a1`。
`main(uint64_t hartid, uint64_t fdt)` 里的 `cmdline_init(fdt)` 解析出
`exp=`，`exp_enabled(n)` 决定每个实验跑不跑。没有 `-append` 时默认全部运行。

只选实验 4 或 5 也没关系：程序会自动、静默地完成 BAR 分配和 bus master
使能（实验 3 做的事），否则网卡无法访问。

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
0x02    Device ID     产品号（QEMU 的 Tulip 模型 = 0x0019，21143；寄存器与 21140 兼容）
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

Tulip 网卡（QEMU 里的 21143 模型）正好两种都有：

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

## 七、Tulip 网卡内部结构

DEC 21140 "Tulip" 是 1990 年代的 100M 网卡，QEMU 完整模拟了它（实际
报的设备 ID 对应 21143，寄存器兼容）。理解它只需要抓住两条路：

```
        控制路径（CPU 主动）                        数据路径（网卡主动）
  ┌───────────────────────────┐              ┌───────────────────────────┐
  │ CSR 寄存器（BAR1，128 字节） │              │ 描述符环 + 帧缓冲区（RAM）  │
  │ CSR3 = RX 环基地址          │   读描述符    │ 16 字节/条，bit31 是 OWN   │
  │ CSR4 = TX 环基地址          │ ───────────> │ status/control/buf1/buf2   │
  │ CSR6 = SR/ST/PR            │              │ 网卡用总线主控 DMA 自己读写 │
  │ CSR1/CSR2 = 踢发送/接收     │ <─────────── │                            │
  └───────────────────────────┘              └───────────────────────────┘
```

### 7.1 CSR 寄存器

BAR1 指向 128 字节的 CSR 窗口。每个 CSR 占 8 字节：

```c
#define TULIP_CSR(n) (0x08 * (n))
```

| CSR | 名字 | 作用 |
|---|---|---|
| 0 | Bus Mode | bit0 SWR：写 1 软复位整块芯片 |
| 1 | Transmit Poll Demand | 写任意值，让发送状态机重扫 TX 环 |
| 2 | Receive Poll Demand | 写任意值，让接收状态机重扫 RX 环 |
| 3 | RX List Base | 接收描述符环的物理地址 |
| 4 | TX List Base | 发送描述符环的物理地址 |
| 5 | Status | TI/TU/RI/RU 等状态（写 1 清除）；bit17-19/20-22 是收/发状态机状态 |
| 6 | Operation Mode | bit1 SR 开接收，bit13 ST 开发送，bit6 PR 混杂模式 |
| 7 | Interrupt Mask | 本实验全 0，用轮询 |
| 9 | Boot ROM / MII | 串行 EEPROM 和 PHY 管理 |
| 12 | SIA Status | 链路/自协商状态（QEMU 直接返回“已完成”） |

注意 **CSR3 是接收、CSR4 是发送**，和 E1000 的 TDBAL/RDBAL 顺序相反，
很容易记反。

### 7.2 描述符和 OWN 位

网卡不直接读 CPU 的变量，它只认内存里的描述符数组。每个描述符 16 字节：

| 偏移 | 字段 | 含义 |
|---|---|---|
| 0 | status | bit31 = OWN；接收时还包含 FS/LS/帧长 |
| 4 | control | 发送：长度 + FS/LS/IC/TER；接收：缓冲区大小 + RER |
| 8 | buf1 | 帧缓冲区的物理地址（21140 只有 32 位 DMA 地址） |
| 12 | buf2 | 第二块缓冲区，本驱动不用 |

**OWN 位是唯一的交接标志**：

- `OWN=1`：描述符归网卡，CPU 不能动。
- `OWN=0`：网卡用完了，描述符归 CPU。

因为两边是异步的，填描述符的顺序很重要：先写 `control`/`buf1`/`buf2`，
执行 `fence`，最后再写 `status`（OWN=1）。否则网卡可能看到 OWN 已置位，
却读到半截描述符。`tulip.c` 里的 `dma_barrier()` 就是这件事。

### 7.3 发送一帧的完整流程

1. 把以太网帧复制到物理连续的内存（`tx_buf`）。
2. 在 TX 环里选一条空闲描述符（OWN=0），填 `buf1=物理地址`、
   `control=长度 | FS | LS | IC`。
3. 写 `status=OWN`，执行 `fence`。
4. 写 CSR1（Transmit Poll Demand），或者把 CSR4 重新写一遍。
5. 网卡自己 DMA 读描述符和缓冲区，把帧发到链路上；完成后把 status 的
   OWN 清 0。
6. CPU 轮询 OWN==0，回收描述符。

`FS`（first segment）和 `LS`（last segment）表示一帧的开始和结束。一帧
可以拆成多个描述符（scatter/gather），本驱动一帧就用一条。

### 7.4 接收一帧的完整流程

1. 初始化时给每条 RX 描述符填好缓冲区地址和大小，`OWN=1` 交给网卡。
2. 网卡收到帧，用 DMA 把数据写进 `buf1` 指向的内存。
3. 网卡写回 status：清 OWN，置 `FS|LS`，并在 bit16-29 写入帧长 `FL`。
4. CPU 轮询到 OWN==0，读 status 和缓冲区。
5. CPU 处理完，重新填好描述符并置 OWN=1，写 CSR2 唤醒接收状态机。

`FL` 包含 4 字节 FCS，但 DMA 写进缓冲区的数据不含 FCS，所以驱动里要
`len = FL - 4`。这也是为什么抓到的帧长度比 `FL` 少 4。

### 7.5 环怎么绕回去

描述符环是线性的物理内存，硬件需要知道哪里是“环尾”：

- TX 描述符 control 的 bit25 = TER（Transmit End of Ring）
- RX 描述符 control 的 bit25 = RER（Receive End of Ring）

处理完带 TER/RER 的描述符之后，硬件把当前指针重置回 CSR4/CSR3。本驱动
把第 4 条描述符设成环尾，所以 4 条描述符循环使用。

### 7.6 过滤、MAC 和 EEPROM

- 单播帧要过 MAC 过滤表。QEMU 的模型默认 `CSR6_PR=1`（混杂模式），所以
  实验里所有帧都能收。
- 真实驱动会关闭 PR，先发一个 192 字节的 **setup frame**（`TDES1_SET`）
  把 16 个 MAC 过滤表项写进去。
- MAC 地址存在 93C46 串行 EEPROM 里，通过 CSR9 的 CS/SK/DI/DO 位敲时序
  读出来。本实验为了简洁，直接用 `TULIP_MAC_BYTES` 写死，和 QEMU `mac=`
  参数保持一致。
- 21140 还有 SIA/PHY 相关寄存器（CSR12-15），负责 10M/100M、全双工、
  自协商。QEMU 直接伪造“自协商完成”，所以这里不展开。

> 真实硬件上，DMA 还依赖 PCI 配置空间 Command 寄存器的 bus master 位
> （见 `demo_mmio_window()`），并且要在 DMA 前后做 cache flush/invalidate
> 或者使用非缓存内存。QEMU 没有 cache 模型，这些工作在本实验里都省略了。

## 八、实验 5：一次真实的 ARP 往返

只跑这个实验：

```sh
make run EXP=5
# 或
xmake run pcie 5
```

`demo_network()` 做的是：

1. `tulip_init()`：软复位，建 4 条 TX + 4 条 RX 描述符，打开 SR/ST。
2. 构造 60 字节广播 ARP 请求：谁有 10.0.2.2？告诉 10.0.2.15。
3. `tulip_send_frame()`：填 TX 描述符、置 OWN、写 CSR1 踢发送。
4. 轮询 RX 描述符的 OWN 位。
5. QEMU 的 user-mode 网络（SLIRP）模拟网关 10.0.2.2，回一个 ARP reply。
6. 网卡把 reply 通过 DMA 写进 RX 缓冲区，CPU 解析并打印。

关键点：**整个过程中 CPU 没有碰过网卡的发送/接收数据通路**。CPU 只是写
了几条描述符和 CSR，帧的搬运是网卡自己做的。这就是“总线主控 DMA”。

> QEMU 注意：`-device tulip` 只创建网卡，不会自动接网络后端。必须写成
> `-netdev user,id=net0 -device tulip,netdev=net0,mac=...`，否则包发出去
> 没人接，ARP 不会有应答。仓库里的 `make run` / `xmake run` 已经这样配好。

实验输出（节选）：

```
[初始化后] RX 的 4 个描述符全是 OWN=1，等网卡填入；TX 全空闲
  CSR3(RX环)=0x0000000080005630  CSR4(TX环)=0x00000000800055f0
  CSR5=f0660000  RS=3 TS=6 TU=0 RI=0 RU=0
  CSR6=00002042  SR=1 ST=1 PR=1
  RX[0] status=80000000 control=00000640 buf1=0x0000000080005cb0

[发送] 广播 ARP：谁有 10.0.2.2？请告诉 10.0.2.15
[发送后] 网卡已经把 TX 描述符的 OWN 清 0

[接收] 收到 64 字节，DMA 已经把帧放进内存:
  0000:  52 54 00 12  34 56 52 55  0a 00 02 02  08 06 00 01
  0010:  08 00 06 04  00 02 52 55  0a 00 02 02  0a 00 02 02
  0020:  52 54 00 12  34 56 0a 00  02 0f 00 00  00 00 00 00

  目的 MAC  52:54:00:12:34:56
  源   MAC  52:55:0a:00:02:02
  EtherType 0806 (ARP)
  ARP reply
  发送方    52:55:0a:00:02:02  10.0.2.2
  目标      52:54:00:12:34:56  10.0.2.15
```

`CSR5` 里的 RS=3 表示接收状态机在 `WAIT RECEIVE`，TS=6 表示发送状态机在
`SUSPENDED`（环里暂时没活干）。收到帧后 `RI=1`，发送环空了以后 `TU=1`，
都是正常状态，写 1 即可清除。

## 九、代码结构

```
pcie/
├── xmake.lua       构建脚本（交叉编译器、链接脚本、objcopy、QEMU 启动）
├── Makefile        等价的 make 版本，二选一
├── linker.ld       链接脚本，代码放在 0x80000000
├── start.S         入口、栈、M 模式陷阱入口；把 QEMU 的 a0/a1 传给 main
├── main.c          六个实验的主流程 + 实验选择
├── cmdline.c/.h    从设备树 /chosen/bootargs 解析 exp=...
├── console.c/.h    UART 输出 + 迷你 printf + hexdump
├── mmio.h          volatile 读写封装
├── pci.c/.h        ECAM 访问、扫描、BAR 探测/分配
├── trap.c/.h       陷阱处理，用来安全地试读地址
└── tulip.c/.h      Tulip 驱动：CSR、DMA 描述符环、发送/接收
```

## 十、实验输出长什么样

实验 1-4 的输出（节选）：

```
实验选择: all
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

实验 5 的 DMA/ARP 输出见上一节。对照 `-device help` 里的 `1011:0019` 和
QEMU monitor 的 `info pci`，所有数字都能对上。

## 十一、动手改

想加深理解，可以试：

1. 把 `pci.c` 里的 `pci_read16` 改回真正的 16 位 MMIO 读
   （`mmio_read16`），观察 Device ID 怎么变成错的 —— 理解 ECAM 的限制
2. 把 BAR 分配地址从 `0x40000000` 改成 `0x40000004`（不按大小对齐），
   看 QEMU 是不是拒绝
3. 分配完地址但**不**打开 Command bit1，再读 CSR5，看会发生什么
4. 加 `-device e1000`，用同样的扫描代码看 Intel 网卡的 BAR（`8086:100e`）
5. 用 `-device edu`（QEMU 教学设备，`1234:11e8`）练习：它的 BAR 是
   1 MiB，读写它的寄存器会有可见的反应
6. 把 `TULIP_RING_SIZE` 改成 2，或让 `tulip_send_frame()` 连续发两帧再
   回收，观察 `CSR5.TU`、描述符 OWN 位和环尾 `TER` 的行为
7. 关掉 `CSR6_PR`，先发一个 192 字节的 setup frame（`TDES1_SET`）把
   `TULIP_MAC_BYTES` 写进过滤表，再试 ARP —— 观察广播帧和单播帧的差异
8. 用 CSR9 的 CS/SK/DI/DO 位读 93C46 EEPROM，把写死的 MAC 换成真实读取
9. 升级成 UDP：构造一个 DHCP Discover，或给 10.0.2.2 发 ICMP echo，
   观察 QEMU user-mode 网络的回复
10. 把轮询换成中断：打开 CSR7 的 RI/TI，接 RISC-V PLIC 的 PCIe INTx

## 十二、常用命令

```sh
# 只跑 ARP 实验；exp=all 跑全部，exp=4,5 跑多个
qemu-system-riscv64 -M virt -bios none -kernel pcie.bin \
    -append "exp=5" \
    -netdev user,id=net0 \
    -device tulip,netdev=net0,mac=52:54:00:12:34:56 \
    -device edu -display none -serial mon:stdio

# make / xmake 包装好的写法
make run EXP=5
xmake run pcie 5

# 看 QEMU 分配的真实 BAR 地址（需要 guest 自己没分配时显示 not mapped）
echo "info pci" | qemu-system-riscv64 -M virt -device tulip -display none -serial none -monitor stdio

# 看设备树的 PCIe 窗口
qemu-system-riscv64 -M virt,dumpdtb=- -display none -serial none | dtc -I dtb -O dts -

# 看设备支持哪些
qemu-system-riscv64 -device help | grep -i -E "tulip|e1000|edu"
```

> `-device tulip` 只是创建网卡，不会自动接后端。想真正收发，必须用
> `-netdev user,id=net0 -device tulip,netdev=net0` 或 `-nic user,model=tulip`。
