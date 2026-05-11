CC = gcc
AS = as
LD = ld
CFLAGS = -m64 -ffreestanding -nostdlib -mno-red-zone -mno-sse -mno-sse2 -Wall -Wextra -O2
ASFLAGS = --64
LDFLAGS = -m elf_x86_64 -T linker.ld

OBJS = boot.o kernel.o fb.o input.o gui.o apps.o progs.o $(patsubst progs/%.c,progs/%.o,$(wildcard progs/*.c))

all: lunarsnow.elf

lunarsnow.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o lunarsnow.elf lunarsnow.iso initrd.tar
	rm -f progs/*.o
	rm -rf iso/

run: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 512M

run-vnc: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 512M -vnc :0

run-sdl: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 512M -display sdl

run-gtk: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 512M -display gtk

iso: lunarsnow.iso

GRUB_MKRESCUE = $(shell command -v grub2-mkrescue || command -v grub-mkrescue || echo grub-mkrescue)

initrd.tar: $(wildcard initrd/*)
	tar cf $@ -C initrd $(notdir $^)

lunarsnow.iso: lunarsnow.elf initrd.tar
	mkdir -p iso/boot/grub
	cp lunarsnow.elf iso/boot/
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\nset gfxpayload=800x600x32\nmenuentry "LunarSnow OS" {\n  multiboot2 /boot/lunarsnow.elf\n  module2 /boot/initrd.tar\n  boot\n}' > iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o lunarsnow.iso iso/

run-iso: lunarsnow.iso
	qemu-system-x86_64 -cdrom lunarsnow.iso -m 512M

# Usage: make remote REPO=org/repo
remote:
	git remote add origin https://github.com/$(REPO).git
	git push -u origin main
