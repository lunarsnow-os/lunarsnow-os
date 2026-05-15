#!/bin/bash
# Create a 128MB disk with FAT32 + SnowFS partitions for QEMU testing
OUT="${1:-fat_disk.raw}"
SIZE_MB=128
SECTORS=$((SIZE_MB * 1024 * 1024 / 512))
PART1_START=2048
PART1_SIZE=65536
PART2_START=$((PART1_START + PART1_SIZE))

rm -f "$OUT" /tmp/fat_part.img

# Create raw image
dd if=/dev/zero of="$OUT" bs=512 count=$SECTORS status=none

# Create MBR partition table with sfdisk
printf 'label: dos\nunit: sectors\n\n%d, %d, 0x0C, *\n%d, %d, 0xDA, -\n' \
    $PART1_START $PART1_SIZE $PART2_START $((SECTORS - PART2_START)) \
    | sfdisk "$OUT" >/dev/null 2>&1

# Format FAT32 partition
dd if="$OUT" of=/tmp/fat_part.img bs=512 skip=$PART1_START count=$PART1_SIZE status=none
mkfs.vfat -F32 /tmp/fat_part.img >/dev/null 2>&1

# Copy files to FAT32
if [ -d initrd ]; then
    for f in initrd/*; do
        mcopy -i /tmp/fat_part.img -s "$f" ::/ 2>/dev/null || true
    done
fi
echo "This file is only on the FAT32 disk (not in initrd)" | mcopy -i /tmp/fat_part.img - ::/disk_info.txt 2>/dev/null || true

# Overlay FAT32 partition back
dd if=/tmp/fat_part.img of="$OUT" bs=512 seek=$PART1_START conv=notrunc status=none
rm -f /tmp/fat_part.img

# Format SnowFS partition using mksnowfs tool
TOOL_DIR=$(dirname "$0")/tools
if [ -x "$TOOL_DIR/mksnowfs" ]; then
    "$TOOL_DIR/mksnowfs" "$OUT" "$PART2_START" "$((SECTORS - PART2_START))"
else
    echo "Warning: tools/mksnowfs not found, skipping SnowFS format"
fi

echo "Created $OUT ($SIZE_MB MB: FAT32+LBA $PART1_START + SnowFS+LBA $PART2_START)"
