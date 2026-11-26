#include <stdint.h>
#include "cmdline.h"
#include "console.h"

#define FDT_MAGIC 0xd00dfeedu

/* FDT 结构块里的 token */
#define FDT_BEGIN_NODE 1
#define FDT_END_NODE   2
#define FDT_PROP       3
#define FDT_NOP        4
#define FDT_END        9

static uint32_t exp_mask = EXP_ALL_MASK;
static char exp_text[32] = "all";

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static const uint8_t *align4(const uint8_t *p)
{
    return (const uint8_t *)(((uintptr_t)p + 3) & ~(uintptr_t)3);
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void copy_str(char *dst, const char *src, int max)
{
    int i = 0;

    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

/*
 * 在 FDT 的结构块里找名为 bootargs 的属性。
 * FDT 是大端，所有 32 位字段都要自己拼。
 */
static const char *fdt_bootargs(uint64_t fdt_addr)
{
    const uint8_t *base = (const uint8_t *)(uintptr_t)fdt_addr;
    const uint8_t *p, *end;
    uint32_t off_struct, off_strings, size_struct;

    if (!fdt_addr || be32(base) != FDT_MAGIC)
        return 0;

    off_struct  = be32(base + 8);
    off_strings = be32(base + 12);
    size_struct = be32(base + 36);

    p = base + off_struct;
    end = p + size_struct;

    while (p + 4 <= end) {
        uint32_t token = be32(p);

        p += 4;
        if (token == FDT_BEGIN_NODE) {
            while (p < end && *p)
                p++;
            p = align4(p + 1);
        } else if (token == FDT_END_NODE) {
            /* 没有参数 */
        } else if (token == FDT_PROP) {
            uint32_t len, nameoff;
            const char *name;

            if (p + 8 > end)
                break;
            len = be32(p);
            nameoff = be32(p + 4);
            p += 8;
            name = (const char *)(base + off_strings + nameoff);
            if (streq(name, "bootargs"))
                return (const char *)p;
            p = align4(p + len);
        } else if (token == FDT_NOP) {
            /* 跳过 */
        } else if (token == FDT_END) {
            break;
        } else {
            break;
        }
    }
    return 0;
}

static int parse_uint(const char **pp, int *out)
{
    const char *p = *pp;
    int v = 0, n = 0;

    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
        if (++n > 3)
            return 0;
    }
    if (!n)
        return 0;
    *pp = p;
    *out = v;
    return 1;
}

/*
 * 解析 exp= 后面的值。
 * 支持 all / none / 单个数字 / 逗号列表 / 范围（如 1-3,5）。
 */
static void parse_exp_value(const char *s, int len)
{
    char buf[32];
    const char *p;
    uint32_t mask = 0;
    int bad = 0;

    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    for (int i = 0; i < len; i++)
        buf[i] = s[i];
    buf[len] = 0;
    copy_str(exp_text, buf, sizeof(exp_text));

    if (streq(buf, "all")) {
        exp_mask = EXP_ALL_MASK;
        return;
    }
    if (streq(buf, "none")) {
        exp_mask = 0;
        return;
    }

    p = buf;
    while (*p) {
        int a, b;

        while (*p == ',' || *p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;

        if (!parse_uint(&p, &a)) {
            bad = 1;
            break;
        }

        b = a;
        if (*p == '-') {
            p++;
            if (!parse_uint(&p, &b)) {
                bad = 1;
                break;
            }
        }

        if (a > b) {
            int t = a;
            a = b;
            b = t;
        }
        for (int k = a; k <= b; k++) {
            if (k >= 1 && k <= EXP_MAX)
                mask |= 1u << k;
            else
                bad = 1;
        }

        if (*p == ',') {
            p++;
        } else if (*p && *p != ' ' && *p != '\t') {
            bad = 1;
            break;
        }
    }

    if (bad || !mask) {
        kprintf("cmdline: 无法解析 exp=%s，改为运行全部实验\n", buf);
        exp_mask = EXP_ALL_MASK;
        copy_str(exp_text, "all", sizeof(exp_text));
        return;
    }

    exp_mask = mask;
}

void cmdline_init(uint64_t fdt)
{
    const char *args = fdt_bootargs(fdt);
    const char *p;

    exp_mask = EXP_ALL_MASK;
    copy_str(exp_text, "all", sizeof(exp_text));

    if (!args)
        return;

    p = args;
    while (*p) {
        const char *start;
        int n;

        while (*p == ' ' || *p == '\t')
            p++;
        start = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;

        n = (int)(p - start);
        if (n > 4 && start[0] == 'e' && start[1] == 'x' &&
            start[2] == 'p' && start[3] == '=') {
            parse_exp_value(start + 4, n - 4);
            return;
        }
    }
}

int exp_enabled(int n)
{
    if (n < 1 || n > EXP_MAX)
        return 0;
    return (exp_mask >> n) & 1u;
}

const char *cmdline_expr(void)
{
    return exp_text;
}
