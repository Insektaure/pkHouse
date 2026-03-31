#!/usr/bin/env python3
"""
Inject .pk3 files into a pkHouse FRLG bank file.

Usage:
    python3 pk3_to_bank.py <bank_file> <pk3_file> [pk3_file2 ...]

Creates the bank file if it doesn't exist.
Injects each .pk3 into the first empty slot.
"""

import struct
import sys
import os
import glob

# Bank format constants (from bank.h)
MAGIC       = b'PKHOUSE\x00'
VERSION_FRLG = 5
HEADER_SIZE  = 16
BOX_COUNT    = 14
SLOTS_PER_BOX = 30
SLOT_SIZE    = 80   # PokeCrypto::SIZE_3STORED
BOX_NAME_SIZE = 16
TOTAL_SLOTS  = BOX_COUNT * SLOTS_PER_BOX  # 420

def create_empty_bank():
    """Create an empty FRLG bank in memory."""
    header = MAGIC + struct.pack('<II', VERSION_FRLG, 0)
    slots = b'\x00' * (TOTAL_SLOTS * SLOT_SIZE)
    names = b'\x00' * (BOX_COUNT * BOX_NAME_SIZE)
    return bytearray(header + slots + names)

def load_bank(path):
    """Load an existing bank or create a new one."""
    if not os.path.exists(path):
        print(f"Creating new FRLG bank: {path}")
        return create_empty_bank()

    with open(path, 'rb') as f:
        data = bytearray(f.read())

    if data[:8] != MAGIC:
        print(f"Error: {path} is not a valid pkHouse bank file")
        sys.exit(1)

    version = struct.unpack_from('<I', data, 8)[0]
    if version != VERSION_FRLG:
        print(f"Error: bank version {version} is not FRLG (expected {VERSION_FRLG})")
        sys.exit(1)

    print(f"Loaded existing FRLG bank: {path}")
    return data

def find_empty_slot(bank):
    """Find the first empty slot. Returns (box, slot) or None."""
    for i in range(TOTAL_SLOTS):
        offset = HEADER_SIZE + i * SLOT_SIZE
        # A slot is empty if all bytes are zero (PID=0 and species=0)
        slot_data = bank[offset:offset + SLOT_SIZE]
        if all(b == 0 for b in slot_data):
            box = i // SLOTS_PER_BOX
            slot = i % SLOTS_PER_BOX
            return (box, slot)
    return None

def inject_pk3(bank, pk3_path):
    """Inject a .pk3 file into the first empty slot."""
    with open(pk3_path, 'rb') as f:
        pk3_data = f.read()

    if len(pk3_data) < SLOT_SIZE:
        print(f"  Error: {pk3_path} is too small ({len(pk3_data)} bytes, need {SLOT_SIZE})")
        return False

    # Only use the first 80 bytes (stored format, ignore party stats if present)
    pk3_data = pk3_data[:SLOT_SIZE]

    # Quick sanity check: PID at 0x00 should be non-zero for a valid Pokemon
    pid = struct.unpack_from('<I', pk3_data, 0)[0]
    if pid == 0:
        print(f"  Error: {pk3_path} has PID=0 (empty/invalid)")
        return False

    # Species at 0x20 (decrypted+unshuffled PKHeX format)
    species = struct.unpack_from('<H', pk3_data, 0x20)[0]

    pos = find_empty_slot(bank)
    if pos is None:
        print(f"  Error: bank is full, cannot inject {pk3_path}")
        return False

    box, slot = pos
    offset = HEADER_SIZE + (box * SLOTS_PER_BOX + slot) * SLOT_SIZE
    bank[offset:offset + SLOT_SIZE] = pk3_data

    print(f"  Injected species {species} (PID=0x{pid:08X}) -> Box {box+1}, Slot {slot+1}")
    return True

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <bank_file> <pk3_file> [pk3_file2 ...]")
        print(f"  Creates the bank if it doesn't exist.")
        print(f"  Supports glob patterns: *.pk3")
        sys.exit(1)

    bank_path = sys.argv[1]
    pk3_patterns = sys.argv[2:]

    # Expand glob patterns
    pk3_files = []
    for pattern in pk3_patterns:
        expanded = glob.glob(pattern)
        if expanded:
            pk3_files.extend(sorted(expanded))
        else:
            pk3_files.append(pattern)  # keep as-is so the warning triggers

    bank = load_bank(bank_path)
    injected = 0

    for pk3_path in pk3_files:
        if not os.path.exists(pk3_path):
            print(f"  Warning: {pk3_path} not found, skipping")
            continue
        if inject_pk3(bank, pk3_path):
            injected += 1

    if injected > 0:
        with open(bank_path, 'wb') as f:
            f.write(bank)
        print(f"\nDone: {injected} Pokemon injected, bank saved to {bank_path}")
    else:
        print("\nNo Pokemon injected.")

if __name__ == '__main__':
    main()
