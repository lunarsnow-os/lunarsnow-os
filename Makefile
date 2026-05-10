CC = gcc
AS = as
LD = ld
CFLAGS = -m32 -ffreestanding -nostdlib -Wall -Wextra -O2
ASFLAGS = --32
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = boot.o kernel.o

all: lunarsnow.elf

lunarsnow.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o lunarsnow.elf lunarsnow.iso
	rm -rf iso/

run: lunarsnow.elf
	qemu-system-x86_64 -kernel lunarsnow.elf -m 64

run-vnc: lunarsnow.elf
	qemu-system-x86_64 -kernel lunarsnow.elf -m 64 -vnc :0

run-sdl: lunarsnow.elf
	qemu-system-x86_64 -kernel lunarsnow.elf -m 64 -display sdl

run-gtk: lunarsnow.elf
	qemu-system-x86_64 -kernel lunarsnow.elf -m 64 -display gtk

iso: lunarsnow.iso

lunarsnow.iso: lunarsnow.elf
	mkdir -p iso/boot/grub
	cp lunarsnow.elf iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "LunarSnow OS" {\n  multiboot /boot/lunarsnow.elf\n  boot\n}' > iso/boot/grub/grub.cfg
	grub2-mkrescue -o lunarsnow.iso iso/ 2>/dev/null

run-iso: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 64
