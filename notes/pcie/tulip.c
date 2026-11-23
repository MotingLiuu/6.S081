#include <stdint.h>
#include "tulip.h"
#include "mmio.h"

uint32_t tulip_read(uint32_t base, int csr)
{
    return mmio_read32((uint64_t)base + TULIP_CSR(csr));
}

void tulip_write(uint32_t base, int csr, uint32_t val)
{
    mmio_write32((uint64_t)base + TULIP_CSR(csr), val);
}
