CC = gcc
AS = as
LD = ld
CFLAGS = -m64 -ffreestanding -nostdlib -mno-red-zone -mno-sse -mno-sse2 -Wall -Wextra -Os
ASFLAGS = --64
LDFLAGS = -m elf_x86_64 -T linker.ld

OBJS = boot.o kernel.o fb.o input.o gui.o apps.o progs.o disk.o fat.o $(patsubst progs/%.c,progs/%.o,$(wildcard progs/*.c))

all: lunarsnow.elf

lunarsnow.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o lunarsnow.elf lunarsnow.iso initrd.tar fat_disk.raw
	rm -f progs/*.o
	rm -rf iso/

fat_disk.raw: $(wildcard initrd/*) create_fat_disk.sh
	bash create_fat_disk.sh

# QEMU base config (override QEMU_OPTS= for extra flags)
QEMU_BASE = -cdrom lunarsnow.iso -m 512M -drive file=fat_disk.raw,format=raw,if=ide -boot order=d

run: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) $(QEMU_OPTS)

run-vnc: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -vnc :0 $(QEMU_OPTS)

run-sdl: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -display sdl $(QEMU_OPTS)

run-gtk: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -display gtk $(QEMU_OPTS)

# UEFI boot (OVMF)
OVMF_CODE = /usr/share/edk2/ovmf/OVMF_CODE.fd
OVMF_VARS = /usr/share/edk2/ovmf/OVMF_VARS.fd

run-uefi: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -bios $(OVMF_CODE) $(QEMU_OPTS)

run-uefi-vnc: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -vnc :0 -bios $(OVMF_CODE) $(QEMU_OPTS)

run-uefi-sdl: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -display sdl -bios $(OVMF_CODE) $(QEMU_OPTS)

run-uefi-gtk: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) -display gtk -bios $(OVMF_CODE) $(QEMU_OPTS)

# UEFI boot with writable varstore
OVMF_VARS_COPY = /tmp/ovmf_vars.fd
run-uefi-vars: lunarsnow.iso fat_disk.raw
	cp $(OVMF_VARS) $(OVMF_VARS_COPY)
	qemu-system-x86_64 $(QEMU_BASE) \
	  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	  -drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
	  $(QEMU_OPTS)

iso: lunarsnow.iso

GRUB_MKRESCUE = $(shell command -v grub2-mkrescue || command -v grub-mkrescue || echo grub-mkrescue)

initrd.tar: $(wildcard initrd/*)
	tar cf $@ -C initrd $(notdir $^)

lunarsnow.iso: lunarsnow.elf initrd.tar
	mkdir -p iso/boot/grub
	cp lunarsnow.elf iso/boot/
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\ninsmod all_video\nset gfxmode=800x600\nmenuentry "LunarSnow OS" {\n  set gfxpayload=800x600x32\n  multiboot2 /boot/lunarsnow.elf\n  module2 /boot/initrd.tar\n  boot\n}' > iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o lunarsnow.iso iso/

run-iso: lunarsnow.iso fat_disk.raw
	qemu-system-x86_64 $(QEMU_BASE) $(QEMU_OPTS)

# Usage: make usb DEVICE=/dev/sdX  (e.g. /dev/sdb — CUIDADO: apaga tudo!)
usb: lunarsnow.iso
	@echo "Writing lunarsnow.iso to $(DEVICE)..."
	dd if=lunarsnow.iso of=$(DEVICE) bs=1M status=progress
	@echo "Done. Boot from $(DEVICE) on real hardware."

# Usage: make remote REPO=org/repo
remote:
	git remote add origin https://github.com/$(REPO).git
	git push -u origin main
