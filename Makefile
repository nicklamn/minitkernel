# Makefile
# Best compatibility with Arch Linux, and the AUR packages: i386-elf-gcc i386-elf-binutils installed.
# This needs to be changed on macOS.
AS            = i386-elf-as
CC            = i386-elf-gcc
LD            = i386-elf-ld
MKDIR         = mkdir -p

# Same for macOS and Linux
ASFLAGS       = --32
CFLAGS        = -m32 -march=i386 -ffreestanding -O2 -Wall -Wextra -msoft-float -fno-stack-protector -nostdinc -fno-pie -fno-omit-frame-pointer -I./include/
LDFLAGS       = -m32 -m elf_i386 -T linker.ld -nostdlib
GRUB_MKRESCUE = grub-mkrescue

all: clean kernel.elf

boot.o: boot.s
	$(AS) $(ASFLAGS) -c $< -o $@

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: boot.o kernel.o
	$(LD) $(LDFLAGS) -o $@ $^

run:
	qemu-system-i386 -kernel kernel.elf -accel tcg -serial stdio

clean:
	rm -f *.o kernel.elf
