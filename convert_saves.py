#!/usr/bin/env python3
"""
Convert save files to work with the current data/script.src and data/TEXT.DAT.

Works for both JP->EN conversion and between different translation versions,
without needing source/script.src.

Each save file stores a TEXT.DAT read pointer (at end_marker - 0x44) and a
script.src instruction pointer (at 0x10C). We find the correct TEXT.DAT
offset by looking up the text display command near the instruction pointer
in data/script.src, validated against data/TEXT.DAT's entry table.
"""

import sys
import os
import struct
import shutil
import glob
import time

PUSH_OPCODE = 0x0001001F


def parse_text_dat_entries(text_dat_path):
    """Parse TEXT.DAT and return set of all entry start offsets.

    Each TEXT.DAT entry is: 4-byte header + null-terminated Shift-JIS string.
    Walking through sequentially gives us every valid entry offset.
    """
    with open(text_dat_path, 'rb') as f:
        data = f.read()

    offsets = set()
    pos = 0
    while pos + 4 < len(data):
        offsets.add(pos)
        pos += 4  # skip header
        null_pos = data.find(b'\x00', pos)
        if null_pos == -1:
            break
        pos = null_pos + 1

    return offsets


def find_text_ptr_from_sip(script_data, sip, entry_offsets):
    """Find the TEXT.DAT offset for a save by examining data/script.src near sip.

    The SoftPal VM pushes TEXT.DAT offsets onto a stack before text display
    syscalls. We scan backwards from sip looking for PUSH instructions whose
    operands are valid TEXT.DAT entry offsets.

    When a text command has both a character name and message (two adjacent
    PUSH+offset pairs), we prefer the message (the one further from sip),
    which is correct for ~93% of saves.
    """
    candidates = []  # (script_offset, text_dat_offset)

    for i in range(sip - 4, max(sip - 500, 0), -4):
        if i < 4:
            break
        v = struct.unpack_from('<I', script_data, i)[0]

        # Skip zero, sentinels, opcodes
        if v == 0 or v in (0x0FFFFFFF, 0xFFFFFFFF):
            continue
        if (v >> 16) == 1:  # opcode (all SoftPal opcodes have upper 16 bits = 0x0001)
            continue

        # Check: preceded by PUSH opcode and valid TEXT.DAT entry
        prev = struct.unpack_from('<I', script_data, i - 4)[0]
        if prev == PUSH_OPCODE and v in entry_offsets:
            candidates.append((i, v))
            if len(candidates) >= 3:
                break

    if not candidates:
        return None

    # If two candidates at adjacent PUSH positions (8 bytes apart = name + message
    # from the same text command), prefer the further one (message position).
    if len(candidates) >= 2:
        addr1, _ = candidates[0]   # closer to sip (name position)
        addr2, v2 = candidates[1]  # further from sip (message position)
        if addr1 - addr2 == 8:
            return v2

    return candidates[0][1]


def find_config_block(data):
    """Find the game config block by its signature pattern.

    The block has a distinctive sequence:
        0xFFFFFFFF, 21, 8, 187, 474, [max_line_width], 90, 182, 445
    Returns the offset of the max_line_width field, or -1 if not found.
    """
    sig = struct.pack('<5I', 0xFFFFFFFF, 21, 8, 187, 474)
    pos = data.find(sig)
    if pos == -1:
        return -1
    return pos + len(sig)


def convert_save(save_path, output_path, script_data, entry_offsets):
    """Convert a single save file:
    - Update the TEXT.DAT pointer using sip-based lookup in data/script.src
    - Patch max line width from 528 to 570
    """
    with open(save_path, 'rb') as f:
        data = bytearray(f.read())

    basename = os.path.basename(save_path)

    # Find the 'end' marker
    end_pos = data.rfind(b'end')
    if end_pos == -1:
        print(f"  WARNING: no 'end' marker found in {basename}, copying as-is")
        with open(output_path, 'wb') as f:
            f.write(data)
        return

    # --- Update TEXT.DAT pointer ---
    ptr_offset = end_pos - 0x44
    if ptr_offset < 0 or ptr_offset + 4 > len(data):
        print(f"  WARNING: text.dat pointer offset out of range in {basename}, copying as-is")
        with open(output_path, 'wb') as f:
            f.write(data)
        return

    old_ptr = struct.unpack_from('<I', data, ptr_offset)[0]
    sip = struct.unpack_from('<I', data, 0x10C)[0]

    if old_ptr == 0:
        print(f"  {basename}: text.dat ptr is 0 (no active text ref)")
    elif old_ptr == 0x0FFFFFFF:
        print(f"  {basename}: text.dat ptr is sentinel (no text displayed)")
    else:
        new_ptr = find_text_ptr_from_sip(script_data, sip, entry_offsets)
        if new_ptr is None:
            print(f"  {basename}: WARNING: no TEXT.DAT ref found near sip 0x{sip:X}, keeping old ptr")
        elif new_ptr == old_ptr:
            print(f"  {basename}: text.dat ptr 0x{old_ptr:X} already correct")
        else:
            struct.pack_into('<I', data, ptr_offset, new_ptr)
            print(f"  {basename}: text.dat ptr 0x{old_ptr:X} -> 0x{new_ptr:X}")

    # --- Patch max line width: 528 -> 570 ---
    mlw_offset = find_config_block(data)
    if mlw_offset == -1:
        print(f"  {basename}: WARNING: config block not found, line width not patched")
    else:
        old_mlw = struct.unpack_from('<I', data, mlw_offset)[0]
        if old_mlw == 528:
            struct.pack_into('<I', data, mlw_offset, 570)
            print(f"  {basename}: max line width 528 -> 570")
        elif old_mlw == 570:
            pass  # already correct
        else:
            print(f"  {basename}: WARNING: unexpected line width {old_mlw} at config block")

    with open(output_path, 'wb') as f:
        f.write(data)


def main():
    save_dir = sys.argv[1] if len(sys.argv) > 1 else './save'

    if not os.path.isdir(save_dir):
        print(f"Error: directory '{save_dir}' not found")
        sys.exit(1)

    script_path = 'data/script.src'
    text_dat_path = 'data/TEXT.DAT'
    for path in [script_path, text_dat_path]:
        if not os.path.isfile(path):
            print(f"Error: {path} not found (run from the game directory)")
            sys.exit(1)

    # Load script and TEXT.DAT entry table
    print("Loading data/script.src and parsing data/TEXT.DAT entries...")
    with open(script_path, 'rb') as f:
        script_data = f.read()
    entry_offsets = parse_text_dat_entries(text_dat_path)
    print(f"  {len(entry_offsets)} TEXT.DAT entries indexed")
    print()

    # Back up save directory before modifying
    files = sorted(glob.glob(os.path.join(save_dir, '*')))
    if not files:
        print(f"No files found in {save_dir}/")
        sys.exit(1)

    timestamp = time.strftime('%Y%m%d_%H%M%S')
    backup_dir = f'save_old_{timestamp}'
    print(f"Backing up {save_dir}/ to {backup_dir}/")
    shutil.copytree(save_dir, backup_dir)
    print()

    # Process files in-place
    print(f"Converting saves in {save_dir}/...")
    for f in files:
        basename = os.path.basename(f)

        if basename == 'continue.dat':
            print(f"  {basename}: skipped")
            continue

        if basename.startswith('save') and basename.endswith('.dat') and basename != 'system.dat':
            convert_save(f, f, script_data, entry_offsets)
        else:
            print(f"  {basename}: not a save file, skipped")

    print()
    print("Done.")


if __name__ == '__main__':
    main()
