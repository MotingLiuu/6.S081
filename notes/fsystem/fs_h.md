# `kernel/fs.h` — On-disk file system format

This header is shared by the kernel *and* user programs. It defines every
on-disk structure and the disk layout itself.

```
[ boot block | super block | log | inode blocks | free bit map | data blocks ]
```

Both the kernel and `mkfs` use this header, so anything added here must
stay in sync with the on-disk image that `mkfs` produces.

## Disk layout

| Block(s)   | Contents                                                       |
| ---------- | -------------------------------------------------------------- |
| 0          | boot block — first 1024 bytes; QEMU loads the ELF kernel from here |
| 1          | super block — describes the rest of the layout                 |
| 2 …        | log blocks (write-ahead log for crash safety)                  |
|            | inode blocks — array of `struct dinode`                         |
|            | free bit map — one bit per data block, 1 = free                 |
|            | data blocks — file / directory contents                        |

> The boot block is just the ELF image of the kernel — QEMU `-kernel`
> loads it into memory and jumps to its entry point. It is *not* parsed
> as a file system structure; the file system proper starts at the
> super block.

## Constants

| Macro       | Value              | Meaning                                                  |
| ----------- | ------------------ | -------------------------------------------------------- |
| `ROOTINO`   | `1`                | Inode number of the root directory                       |
| `BSIZE`     | `1024`             | Block size in bytes (every block, everywhere)            |
| `FSMAGIC`   | `0x10203040`       | Magic number written to `superblock.magic` to identify the FS type |
| `NDIRECT`   | `12`               | Number of direct block pointers in `dinode.addrs[]`      |
| `NINDIRECT` | `BSIZE / sizeof(uint)` = 256 | Number of block numbers the single indirect block can hold |
| `MAXFILE`   | `NDIRECT + NINDIRECT` = 268 | Maximum file size in blocks (≈ 268 KB at BSIZE=1024) |
| `DIRSIZ`    | `14`               | Max bytes of a directory entry's name field              |
| `IPB`       | `BSIZE / sizeof(struct dinode)` | Inodes packed per block                          |
| `BPB`       | `BSIZE * 8` = 8192 | Bitmap bits per block                                    |

## `struct superblock`

Describes the disk layout. `mkfs` computes it and writes it to block 1;
the kernel keeps one copy in memory (`kernel/fs.c`'s `sb`) and trusts
`FSMAGIC` to decide whether the image looks like an xv6 file system.

```c
struct superblock {
  uint magic;       // Must be FSMAGIC
  uint size;        // Size of file system image (blocks)
  uint nblocks;     // Number of data blocks
  uint ninodes;     // Number of inodes
  uint nlog;        // Number of log blocks
  uint logstart;    // Block number of first log block
  uint inodestart;  // Block number of first inode block
  uint bmapstart;   // Block number of first free map block
};
```

Layout math (per the `mkfs` algorithm):

```
logstart    = 2
inodestart  = logstart + nlog
bmapstart   = inodestart + ninodes / IPB
datastart   = bmapstart + nblocks / BPB
```

## `struct dinode` — on-disk inode

The *on-disk* inode. Distinct from the in-memory `struct inode` in
`kernel/file.h`, which caches extra fields like refcount and locks.

```c
struct dinode {
  short type;              // T_FILE / T_DIR / T_DEVICE (kernel/stat.h)
  short major;             // Major device number (T_DEVICE only)
  short minor;             // Minor device number (T_DEVICE only)
  short nlink;             // Number of directory links to this inode
  uint size;               // File size in bytes
  uint addrs[NDIRECT + 1]; // Block numbers (12 direct + 1 indirect)
};
```

### File size

- `addrs[0 .. NDIRECT-1]` → 12 direct block numbers.
- `addrs[NDIRECT]` → one *indirect* block; the kernel reads that block
  to get up to `NINDIRECT = 256` more block numbers.
- Max file = `MAXFILE * BSIZE = 268 * 1024 = 268 KB`.

### `major` / `minor`

Only meaningful for `T_DEVICE` inodes (e.g. `/dev/console`, `/dev/null`).
xv6's `devsw[]` table is indexed by `major` to pick the driver; `minor`
is passed through for the driver to interpret. Created with
`mknod /dev/console 1 0`, which writes `type=T_DEVICE, major=1, minor=0`.

### `nlink`

How many directory entries point at this inode. The kernel refuses to
free an inode while `nlink > 0`.

## Inode / block location macros

These turn logical numbers into on-disk block numbers using the
in-memory super block.

```c
#define IPB           (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

#define BPB           (BSIZE * 8)
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)
```

- `IBLOCK(i, sb)` — block that holds inode `i`.
- `BBLOCK(b, sb)` — which bitmap block tracks data block `b`. Each
  bitmap block covers `BPB = 8192` data blocks, so this only changes
  every 8192 blocks.

## `struct dirent` — directory entry

A directory is just a file of `dirent`s. `name` is fixed-width and
NUL-padded, so `read()` returns a packed array with no per-entry
length field.

```c
struct dirent {
  ushort inum;             // Inode number; 0 means the entry is unused
  char   name[DIRSIZ];     // Up to 14 chars, NUL-padded
};
```

`inum == 0` is the "deleted slot" marker used by `unlink` — the entry
stays on disk but is skipped during lookup.

## Inode 1 is the root directory

By convention, `ROOTINO = 1` (not 0; 0 means "no inode"). At boot the
kernel reads the super block, walks to `sb.inodestart`, and treats
inode 1 as the root. A directory is just a file with `type == T_DIR`
whose data blocks are an array of `dirent`s; every directory *must*
contain at least `.` and `..`.