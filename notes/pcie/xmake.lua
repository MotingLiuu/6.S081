-- PCIe / 配置空间 / BAR 教学实验
--
-- 这是一个 RISC-V 裸机程序：没有操作系统、没有 libc、没有标准启动文件。
-- 因此不能走 xmake 默认的"链接 libc + crt0"流程，要自己指定：
--   * 交叉编译器（set_toolset）
--   * 链接脚本（add_files("linker.ld")）
--   * 入口符号 _start 和栈（见 start.S / linker.ld）
--
-- 常用命令：
--   xmake           编译 → build/cross/riscv64/release/pcie.elf 和 pcie.bin
--   xmake run       在 QEMU 里运行（Ctrl-A X 退出）
--   xmake clean     清理
--   xmake -v        查看完整的编译与链接命令

set_project("pcie-demo")
set_version("1.0.0")
set_xmakever("3.0.0")

set_languages("c11")
set_warnings("all")

target("pcie")
    set_kind("binary")

    -- 在 target 层级把平台和架构钉死成 cross/riscv64，
    -- 这样不用每次敲 xmake f -p cross -a riscv64。
    set_plat("cross")
    set_arch("riscv64")

    -- 直接点名交叉编译器，省得 xmake 去 /usr 下猜前缀。
    -- 前提是 riscv64-unknown-elf-gcc 在 PATH 里。
    local cross = "riscv64-unknown-elf-"
    set_toolset("cc", cross .. "gcc")
    set_toolset("as", cross .. "gcc")
    set_toolset("ld", cross .. "gcc")
    set_toolset("sh", cross .. "gcc")
    set_toolset("objcopy", cross .. "objcopy")

    -- -ffreestanding / -nostdlib / -fno-builtin：
    --   没有操作系统和 libc，别去找 printf、memset 这些东西
    -- -mcmodel=medany：代码可以落在任意 2 GiB 范围内，裸机常用
    add_cflags("-march=rv64gc", "-mabi=lp64", "-mcmodel=medany",
               "-ffreestanding", "-nostdlib", "-fno-builtin", "-fno-common",
               {force = true})
    add_asflags("-march=rv64gc", "-mabi=lp64", "-mcmodel=medany", {force = true})

    -- 链接脚本放进 add_files，xmake 会自动转成 -T linker.ld。
    -- 注意别再手动 add_ldflags("-T linker.ld")，否则传两次 ld 会报错。
    add_files("linker.ld")
    add_files("start.S", "main.c", "console.c", "pci.c", "trap.c", "tulip.c")

    -- 链接阶段同样要 -nostdlib，否则会去找 crt0.o 和 libc
    add_ldflags("-nostdlib", "-Wl,--build-id=none", {force = true})

    -- 产物叫 pcie.elf，而不是默认的无后缀名
    set_extension(".elf")

    -- QEMU -kernel 吃的是纯二进制镜像，用 objcopy 把 ELF 的 LOAD 段抽出来
    after_build(function (target)
        local elf = target:targetfile()
        local bin = elf:gsub("%.elf$", ".bin")
        os.execv(cross .. "objcopy", {"-O", "binary", elf, bin})
        print("raw binary: %s", bin)
    end)

    on_run(function (target)
        local bin = target:targetfile():gsub("%.elf$", ".bin")
        os.execv("qemu-system-riscv64", {
            "-M", "virt",
            "-bios", "none",
            "-kernel", bin,
            "-netdev", "user,id=net0",
            "-device", "tulip,netdev=net0,mac=52:54:00:12:34:56",
            "-device", "edu",
            "-display", "none",
            "-serial", "mon:stdio",
        })
    end)
