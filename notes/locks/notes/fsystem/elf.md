# EFL Introduction

Take `user/_init` as an example. Here are the result of `riscv64-unknown-elf-readelf -SW user/_init` and `riscv64-unknown-elf-readelf -lW user/_init`.


## Sections in EFL

```txt

There are 18 section headers, starting at offset 0x8fe0:

Section Headers:
  [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        0000000000000000 001000 0009b2 00  AX  0   0  2
  [ 2] .rodata           PROGBITS        00000000000009b8 0019b8 000099 00   A  0   0  8
  [ 3] .data             PROGBITS        0000000000001000 002000 000010 00  WA  0   0  8
  [ 4] .bss              NOBITS          0000000000001010 002010 000020 00  WA  0   0  8
  [ 5] .debug_info       PROGBITS        0000000000000000 002010 001524 00      0   0  1
  [ 6] .debug_abbrev     PROGBITS        0000000000000000 003534 0006bf 00      0   0  1
  [ 7] .debug_loc        PROGBITS        0000000000000000 003bf3 0027bf 00      0   0  1
  [ 8] .debug_aranges    PROGBITS        0000000000000000 0063c0 0000f0 00      0   0 16
  [ 9] .debug_line       PROGBITS        0000000000000000 0064b0 001673 00      0   0  1
  [10] .debug_str        PROGBITS        0000000000000000 007b23 00047c 01  MS  0   0  1
  [11] .comment          PROGBITS        0000000000000000 007f9f 000019 01  MS  0   0  1
  [12] .riscv.attributes RISCV_ATTRIBUTES 0000000000000000 007fb8 000074 00      0   0  1
  [13] .debug_frame      PROGBITS        0000000000000000 008030 000588 00      0   0  8
  [14] .debug_ranges     PROGBITS        0000000000000000 0085b8 000050 00      0   0  1
  [15] .symtab           SYMTAB          0000000000000000 008608 000768 18     16  30  8
  [16] .strtab           STRTAB          0000000000000000 008d70 0001b7 00      0   0  1
  [17] .shstrtab         STRTAB          0000000000000000 008f27 0000b5 00      0   0  1
```

The sections that contribute to the user's program's memo are

| Section | Virtual Address Range (end exclusive) | Size  | Purpose                  |
|---------|---------------------------------------|-------|--------------------------|
| `.text` | `0x0000–0x09b2`                       | 0x9b2 | Instructions             |
| `.rodata` | `0x09b8–0x0a51`                     | 0x99  | Read-only constants      |
| `.data` | `0x1000–0x1010`                       | 0x10  | Initialized writable data |
| `.bss`  | `0x1010–0x1030`                      | 0x20  | Zero-initialized data    |

## Address versus Offset 

For `.data`
```txt
Address = 0x1000
Offset = 0x2000
```

The `.data` bytes are stored at file offset 0x2000, but belong at virtual address 0x1000 when loaded.

## Program Headers

```txt
Elf file type is EXEC (Executable file)
Entry point 0xbc
There are 4 program headers, starting at offset 64

Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  RISCV_ATTRIBUT 0x007fb8 0x0000000000000000 0x0000000000000000 0x000074 0x000000 R   0x1
  LOAD           0x001000 0x0000000000000000 0x0000000000000000 0x000a51 0x000a51 R E 0x1000
  LOAD           0x002000 0x0000000000001000 0x0000000000001000 0x000010 0x000030 RW  0x1000
  GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0x10

 Section to Segment mapping:
  Segment Sections...
   00     .riscv.attributes
   01     .text .rodata
   02     .data .bss
   03
```

Section-to-segment mapping:

```txt
Segment 0: .riscv.attributes
Segment 1: .text .rodata
Segment 2: .data .bss
Segment 3: no sections
```

## Fields

The `PhysAddr` field is not used in User Prog. `kexec` only use `VirtAddr` and `MemSize` to load from elf to memo.

## Segments

`Segment0`: RISCV_ATTRIBUTES

```txt
Offset = 0x7fb8
FileSiz = 0x74
MemSiz = 0
```

Architecture metadata, not program code/data. xv6 skips it because it is not LOAD.

----

`Segment1`: LOAD,read + execute

```txt
Offset   = 0x1000
VirtAddr = 0
FileSiz  = 0xa51
MemSiz   = 0xa51
Flags    = R E
Align    = 0x1000
```

The loader copies:

```txt
ELF file [0x1000, 0x1a51)
|
User VA  [0x0000, 0x0a51)
```

This segment includes:

```txt
VA 0x0000 ┌──────────────────┐
          │ .text            │ 0x9b2 bytes
VA 0x09b2 ├──────────────────┤
          │ alignment gap    │ 6 bytes
VA 0x09b8 ├──────────────────┤
          │ .rodata          │ 0x99 bytes
VA 0x0a51 └──────────────────┘
```

----

`Segment2`: LOAD, read+write

```txt
Offset   = 0x2000
VirtAddr = 0x1000
FileSiz  = 0x10
MemSiz   = 0x30
Flags    = RW
```

The loader creates 0x30 bytes of memo, but copies only 0x10 bytes from the file.

```txt
VA 0x1000 ┌──────────────────┐
          │ .data            │ 0x10 bytes copied from file
VA 0x1010 ├──────────────────┤
          │ .bss             │ 0x20 bytes initialized to zero
VA 0x1030 └──────────────────┘
```

```txt
MemSiz - FileSiz = 0x30 - 0x10 = 0x20 = .bss size
```

## Why .bss and .debug_info share file offset 0x2010

```txt
.bss         NOBITS    Offset 0x2010
.debug_info  PROGBITS  Offset 0x2010
```

There is no overlap of stored contents: .bss occupies no file bytes. Its offset does not reserve 0x20 bytes in the file, so .debug_info can start there.
