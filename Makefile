CC = gcc
AS = as
LD = ld
CFLAGS = -m32 -ffreestanding -nostdlib -Wall -Wextra -O2
ASFLAGS = --32
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = boot.o kernel.o fb.o input.o gui.o apps.o progs.o $(patsubst progs/%.c,progs/%.o,$(wildcard progs/*.c))

all: lunarsnow.elf

lunarsnow.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o lunarsnow.elf lunarsnow.iso
	rm -f progs/*.o
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

GRUB_MKRESCUE = $(shell command -v grub2-mkrescue || command -v grub-mkrescue || echo grub-mkrescue)

lunarsnow.iso: lunarsnow.elf
	mkdir -p iso/boot/grub
	cp lunarsnow.elf iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "LunarSnow OS" {\n  multiboot /boot/lunarsnow.elf\n  boot\n}' > iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o lunarsnow.iso iso/

run-iso: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 64

# Usage: make remote REPO=org/repo
remote:
	git remote add origin https://github.com/$(REPO).git
	git push -u origin main
