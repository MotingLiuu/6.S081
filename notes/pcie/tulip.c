#include <stdint.h>
#include "tulip.h"
#include "console.h"
#include "mmio.h"

static struct tulip_desc tx_ring[TULIP_RING_SIZE] __attribute__((aligned(16)));
static struct tulip_desc rx_ring[TULIP_RING_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buf[TULIP_RING_SIZE][TULIP_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t rx_buf[TULIP_RING_SIZE][TULIP_BUF_SIZE] __attribute__((aligned(16)));

static uint32_t tulip_base;
static int tx_head;

static void copy_bytes(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    while (n--)
        *d++ = *s++;
}

static void zero_desc(struct tulip_desc *d)
{
    d->status = 0;
    d->control = 0;
    d->buf1 = 0;
    d->buf2 = 0;
}

/*
 * 描述符写完 -> fence -> 置 OWN，顺序不能反。
 * 否则网卡可能看到 OWN=1，却读到还没写完的 buf1/control。
 */
static void dma_barrier(void)
{
    __asm__ volatile("fence" ::: "memory");
}

uint32_t tulip_read(uint32_t base, int csr)
{
    return mmio_read32((uint64_t)base + TULIP_CSR(csr));
}

void tulip_write(uint32_t base, int csr, uint32_t val)
{
    mmio_write32((uint64_t)base + TULIP_CSR(csr), val);
}

/*
 * 把一个 RX 描述符重新交给网卡。
 * 先填 control/buf1，最后写 status（OWN），再 fence。
 */
static void rx_rearm(int i)
{
    struct tulip_desc *d = &rx_ring[i];

    d->status = 0;
    d->control = TULIP_RDES1_SIZE(TULIP_BUF_SIZE) |
                 (i == TULIP_RING_SIZE - 1 ? TULIP_RDES1_RER : 0);
    d->buf1 = (uint32_t)(uintptr_t)rx_buf[i];
    d->buf2 = 0;
    dma_barrier();
    d->status = TULIP_DESC_OWN;
    dma_barrier();

    /* 接收状态机可能因为没有描述符而停下了，踢它一脚 */
    tulip_write(tulip_base, 2, 1);
}

void tulip_init(uint32_t base)
{
    tulip_base = base;

    /* 软复位，丢掉上电/上一个实验留下的状态 */
    tulip_write(base, 0, TULIP_CSR0_SWR);
    /* CSR0=0：32 位地址、小端描述符（QEMU 的默认） */
    tulip_write(base, 0, 0);

    /* 本驱动用轮询，不开中断 */
    tulip_write(base, 7, 0);

    for (int i = 0; i < TULIP_RING_SIZE; i++) {
        zero_desc(&tx_ring[i]);
        zero_desc(&rx_ring[i]);
    }
    tx_head = 0;

    /* 每个 RX 描述符指向一块自己的缓冲区，环尾用 RER 绕回环首 */
    for (int i = 0; i < TULIP_RING_SIZE; i++) {
        rx_ring[i].control = TULIP_RDES1_SIZE(TULIP_BUF_SIZE) |
                             (i == TULIP_RING_SIZE - 1 ? TULIP_RDES1_RER : 0);
        rx_ring[i].buf1 = (uint32_t)(uintptr_t)rx_buf[i];
        rx_ring[i].buf2 = 0;
    }

    dma_barrier();

    /* 全部填好之后，再统一把 OWN 交给网卡 */
    for (int i = 0; i < TULIP_RING_SIZE; i++)
        rx_ring[i].status = TULIP_DESC_OWN;

    dma_barrier();

    /* CSR3 = 接收环物理地址，CSR4 = 发送环物理地址 */
    tulip_write(base, 3, (uint32_t)(uintptr_t)rx_ring);
    tulip_write(base, 4, (uint32_t)(uintptr_t)tx_ring);

    /*
     * SR = 开始接收，ST = 开始发送。
     * PR = promiscuous，实验里不折腾 MAC 过滤表，先收下所有帧。
     */
    tulip_write(base, 6, TULIP_CSR6_SR | TULIP_CSR6_ST | TULIP_CSR6_PR);

    /* 清掉使能过程中产生的 TU/RI/RU 等状态位 */
    tulip_write(base, 5, 0xffffffff);
}

int tulip_send_frame(const void *frame, int len)
{
    struct tulip_desc *d = &tx_ring[tx_head];

    if (len <= 0 || len > TULIP_BUF_SIZE)
        return -1;
    if (d->status & TULIP_DESC_OWN)
        return -1;

    copy_bytes(tx_buf[tx_head], frame, (uint32_t)len);
    dma_barrier();

    /*
     * 一帧放在一个缓冲区里：
     *   FS + LS 表示这是完整的一帧，IC 让完成后置 CSR5.TI，
     *   最后一个描述符再加 TER，让硬件绕回环首。
     */
    d->control = (uint32_t)len | TULIP_TDES1_FS | TULIP_TDES1_LS |
                 TULIP_TDES1_IC |
                 (tx_head == TULIP_RING_SIZE - 1 ? TULIP_TDES1_TER : 0);
    d->buf1 = (uint32_t)(uintptr_t)tx_buf[tx_head];
    d->buf2 = 0;

    dma_barrier();
    d->status = TULIP_DESC_OWN;     /* 交给网卡 */
    dma_barrier();

    /* CSR1 = Transmit Poll Demand：网卡来扫 TX 环 */
    tulip_write(tulip_base, 1, 1);

    /*
     * 等网卡把 OWN 清 0。
     * 在 QEMU 里这一步是同步完成的；真实硬件是异步 DMA，
     * 所以这里必须轮询（或者用中断）。
     */
    for (uint32_t spin = 0; spin < 10000000; spin++) {
        if (!(d->status & TULIP_DESC_OWN)) {
            dma_barrier();
            tx_head = (tx_head + 1) % TULIP_RING_SIZE;
            return 0;
        }
    }
    return -1;
}

int tulip_recv_frame(void *frame, int maxlen)
{
    for (int i = 0; i < TULIP_RING_SIZE; i++) {
        struct tulip_desc *d = &rx_ring[i];
        uint32_t status = d->status;
        int len;

        if (status & TULIP_DESC_OWN)
            continue;

        /* 看到 OWN=0 之后，DMA 写入的帧数据才是有效的 */
        dma_barrier();

        /* 缓冲区足够大，正常一帧一定完整落在同一个描述符里 */
        if (!(status & TULIP_RDES0_FS) || !(status & TULIP_RDES0_LS)) {
            rx_rearm(i);
            continue;
        }

        /*
         * 21140 的 FL 字段包含 4 字节 FCS，但 DMA 写进缓冲区的
         * 数据不含 FCS，所以要减 4。
         */
        len = (int)((status >> TULIP_RDES0_FL_SHIFT) & TULIP_RDES0_FL_MASK) - 4;
        if (len < 0)
            len = 0;
        if (len > maxlen)
            len = maxlen;
        copy_bytes(frame, rx_buf[i], (uint32_t)len);

        /* 看完就把描述符还给网卡，准备接下一帧 */
        rx_rearm(i);
        return len;
    }
    return -1;
}

void tulip_dump(void)
{
    uint32_t csr5 = tulip_read(tulip_base, 5);
    uint32_t csr6 = tulip_read(tulip_base, 6);

    kprintf("  CSR3(RX环)=%p  CSR4(TX环)=%p\n",
            (void *)(uintptr_t)tulip_read(tulip_base, 3),
            (void *)(uintptr_t)tulip_read(tulip_base, 4));
    kprintf("  CSR5=%08x  RS=%d TS=%d TU=%d RI=%d RU=%d\n", csr5,
            (int)((csr5 >> TULIP_CSR5_RS_SHIFT) & TULIP_CSR5_RS_MASK),
            (int)((csr5 >> TULIP_CSR5_TS_SHIFT) & TULIP_CSR5_TS_MASK),
            !!(csr5 & TULIP_CSR5_TU), !!(csr5 & TULIP_CSR5_RI),
            !!(csr5 & TULIP_CSR5_RU));
    kprintf("  CSR6=%08x  SR=%d ST=%d PR=%d\n", csr6,
            !!(csr6 & TULIP_CSR6_SR), !!(csr6 & TULIP_CSR6_ST),
            !!(csr6 & TULIP_CSR6_PR));

    for (int i = 0; i < TULIP_RING_SIZE; i++) {
        kprintf("  TX[%d] status=%08x control=%08x buf1=%p\n", i,
                tx_ring[i].status, tx_ring[i].control,
                (void *)(uintptr_t)tx_ring[i].buf1);
        kprintf("  RX[%d] status=%08x control=%08x buf1=%p\n", i,
                rx_ring[i].status, rx_ring[i].control,
                (void *)(uintptr_t)rx_ring[i].buf1);
    }
}
