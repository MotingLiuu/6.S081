CC      = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
CFLAGS  = -march=rv64gc -mabi=lp64 -mcmodel=medany \
          -ffreestanding -nostdlib -fno-builtin -fno-common \
          -Wall -Wextra -Wno-unused-parameter -g -O0 -MMD -MP

OBJS = start.o main.o console.o pci.o trap.o tulip.o

all: pcie.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

pcie.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) -T linker.ld -Wl,--build-id=none -o $@ $(OBJS)

pcie.bin: pcie.elf
	$(OBJCOPY) -O binary $< $@

run: pcie.bin
	qemu-system-riscv64 -M virt -bios none -kernel pcie.bin \
	    -netdev user,id=net0 \
	    -device tulip,netdev=net0,mac=52:54:00:12:34:56 \
	    -device edu \
	    -display none -serial mon:stdio

clean:
	rm -f *.o *.d *.elf *.bin

-include $(OBJS:.o=.d)

.PHONY: all run clean
