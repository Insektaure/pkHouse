#!/bin/bash
# strip_box_names.sh - Remove box name data from a pkHouse bank file.
# Restores the file to the old format (header + slot data only).
#
# Usage: ./strip_box_names.sh <bank_file.bin>

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <bank_file.bin>"
    exit 1
fi

FILE="$1"

if [ ! -f "$FILE" ]; then
    echo "Error: File not found: $FILE"
    exit 1
fi

# Verify magic header
MAGIC=$(head -c 8 "$FILE" | tr -d '\0')
if [ "$MAGIC" != "PKHOUSE" ]; then
    echo "Error: Not a valid pkHouse bank file (bad magic)"
    exit 1
fi

# Read version (bytes 8-11, little-endian u32)
VERSION=$(od -An -tu4 -j8 -N4 "$FILE" | tr -d ' ')

case "$VERSION" in
    1) BOXES=32; SLOT_SIZE=344; SLOTS_PER_BOX=30 ;;  # VERSION_32BOX, SIZE_9PARTY
    2) BOXES=40; SLOT_SIZE=344; SLOTS_PER_BOX=30 ;;  # VERSION_40BOX, SIZE_9PARTY
    3) BOXES=32; SLOT_SIZE=360; SLOTS_PER_BOX=30 ;;  # VERSION_LA, SIZE_8APARTY
    4) BOXES=40; SLOT_SIZE=260; SLOTS_PER_BOX=25 ;;  # VERSION_LGPE, SIZE_6PARTY
    *)
        echo "Error: Unknown bank version: $VERSION"
        exit 1
        ;;
esac

HEADER_SIZE=16
DATA_SIZE=$((BOXES * SLOTS_PER_BOX * SLOT_SIZE))
EXPECTED_SIZE=$((HEADER_SIZE + DATA_SIZE))
NAME_BLOCK=$((BOXES * 16))
FULL_SIZE=$((EXPECTED_SIZE + NAME_BLOCK))
FILE_SIZE=$(stat -c%s "$FILE" 2>/dev/null || stat -f%z "$FILE")

if [ "$FILE_SIZE" -eq "$EXPECTED_SIZE" ]; then
    echo "File has no box name data. Nothing to strip."
    exit 0
fi

if [ "$FILE_SIZE" -ne "$FULL_SIZE" ]; then
    echo "Warning: Unexpected file size ($FILE_SIZE bytes)"
    echo "  Expected without names: $EXPECTED_SIZE bytes"
    echo "  Expected with names:    $FULL_SIZE bytes"
    echo "Proceeding anyway..."
fi

# Create backup
cp "$FILE" "${FILE}.bak"
echo "Backup created: ${FILE}.bak"

# Truncate to remove name block
head -c "$EXPECTED_SIZE" "${FILE}.bak" > "$FILE"

echo "Stripped $NAME_BLOCK bytes of box name data"
echo "  Before: $FILE_SIZE bytes"
echo "  After:  $EXPECTED_SIZE bytes"
