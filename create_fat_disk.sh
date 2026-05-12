#!/bin/bash
# Create a 64MB FAT32 disk image for QEMU testing
# Uses parted + mkfs.vfat + mcopy (no loop devices needed)
OUT="${1:-fat_disk.raw}"
SIZE_MB=64
SECTORS=$((SIZE_MB * 1024 * 1024 / 512))
PART_START=2048
PART_SECTORS=$((SECTORS - PART_START))

rm -f "$OUT" /tmp/fat_part.img

# Create raw image
dd if=/dev/zero of="$OUT" bs=512 count=$SECTORS status=none

# Create MBR partition table
parted -s "$OUT" mklabel msdos
parted -s "$OUT" mkpart primary fat32 ${PART_START}s 100%

# Extract partition area, format it
dd if="$OUT" of=/tmp/fat_part.img bs=512 skip=$PART_START count=$PART_SECTORS status=none
mkfs.vfat -F32 /tmp/fat_part.img >/dev/null 2>&1

# Copy files using mcopy
if [ -d initrd ]; then
    for f in initrd/*; do
        mcopy -i /tmp/fat_part.img -s "$f" ::/ 2>/dev/null || true
    done
fi
echo "This file is only on the FAT32 disk (not in initrd)" | mcopy -i /tmp/fat_part.img - ::/disk_info.txt 2>/dev/null || true

# Overlay partition back into image
dd if=/tmp/fat_part.img of="$OUT" bs=512 seek=$PART_START conv=notrunc status=none
rm -f /tmp/fat_part.img

echo "Created $OUT ($SIZE_MB MB, FAT32, partition at LBA $PART_START)"
