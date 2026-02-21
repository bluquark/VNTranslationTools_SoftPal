#!/usr/bin/env python3
"""
Convert save files made against the original Japanese TEXT.DAT (source/)
to work with the partially translated TEXT.DAT (data/).

Builds a JP->EN TEXT.DAT offset mapping by comparing source/script.src
with data/script.src. The translated script has TEXT.DAT references
patched to point to appended English strings. Save files store a
TEXT.DAT read pointer (at end_marker - 0x44) that needs remapping.

Untranslated pointers are left as-is since the original JP data is
preserved at the same offsets in the EN TEXT.DAT.
"""

import sys
import os
import struct
import shutil
import glob
import time


def build_text_mapping(jp_script_path, en_script_path):
    """Build JP->EN TEXT.DAT offset mapping from script.src diffs.

    VNTextPatch appends translated strings to the end of TEXT.DAT and
    patches the 4-byte operands in script.src to point there. By diffing
    the two scripts we get a complete mapping of original -> translated
    TEXT.DAT offsets.
    """
    with open(jp_script_path, 'rb') as f:
        jp_script = f.read()
    with open(en_script_path, 'rb') as f:
        en_script = f.read()

    assert len(jp_script) == len(en_script), "script.src files must be same size"

    # Known non-TEXT.DAT diff positions (config constants patched by VNTextPatch):
    #   0x02605C = FontYSpacingBetweenLines
    #   0x026084 = MaxLineWidth
    skip_offsets = {0x02605C, 0x026084}

    mapping = {}
    for i in range(0, len(jp_script) - 3, 4):
        if i in skip_offsets:
            continue
        jp_val = struct.unpack_from('<I', jp_script, i)[0]
        en_val = struct.unpack_from('<I', en_script, i)[0]
        if jp_val != en_val:
            mapping[jp_val] = en_val

    return mapping


def find_config_block(data):
    """Find the game config block by its signature pattern.

    The block has a distinctive sequence:
        0xFFFFFFFF, 21, 8, 187, 474, [max_line_width], 90, 182, 445
    Returns the offset of the max_line_width field, or -1 if not found.
    """
    # Signature: the 5 values before max_line_width
    sig = struct.pack('<5I', 0xFFFFFFFF, 21, 8, 187, 474)
    pos = data.find(sig)
    if pos == -1:
        return -1
    return pos + len(sig)  # offset of the max_line_width field


def convert_save(save_path, output_path, mapping):
    """Convert a single save file:
    - Remap the TEXT.DAT pointer (at end_marker - 0x44)
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

    # --- Remap TEXT.DAT pointer ---
    ptr_offset = end_pos - 0x44
    if ptr_offset < 0 or ptr_offset + 4 > len(data):
        print(f"  WARNING: text.dat pointer offset out of range in {basename}, copying as-is")
        with open(output_path, 'wb') as f:
            f.write(data)
        return

    old_ptr = struct.unpack_from('<I', data, ptr_offset)[0]

    if old_ptr == 0:
        print(f"  {basename}: text.dat ptr is 0 (no active text ref)")
    elif old_ptr == 0x0FFFFFFF:
        print(f"  {basename}: text.dat ptr is sentinel 0x0FFFFFFF (no text displayed)")
    elif old_ptr in mapping:
        new_ptr = mapping[old_ptr]
        struct.pack_into('<I', data, ptr_offset, new_ptr)
        print(f"  {basename}: remapped 0x{old_ptr:X} -> 0x{new_ptr:X}")
    else:
        print(f"  {basename}: ptr 0x{old_ptr:X} not in mapping (untranslated, keeping as-is)")

    # --- Patch max line width: 528 -> 570 ---
    mlw_offset = find_config_block(data)
    if mlw_offset == -1:
        print(f"  {basename}: WARNING: config block not found, line width not patched")
    else:
        old_mlw = struct.unpack_from('<I', data, mlw_offset)[0]
        if old_mlw == 528:
            struct.pack_into('<I', data, mlw_offset, 570)
            print(f"  {basename}: max line width 528 -> 570 at 0x{mlw_offset:X}")
        else:
            print(f"  {basename}: WARNING: expected 528 at config block, found {old_mlw}")

    with open(output_path, 'wb') as f:
        f.write(data)


def main():
    save_dir = sys.argv[1] if len(sys.argv) > 1 else './save_jp'

    if not os.path.isdir(save_dir):
        print(f"Error: directory '{save_dir}' not found")
        sys.exit(1)

    jp_script = 'source/script.src'
    en_script = 'data/script.src'
    for path in [jp_script, en_script]:
        if not os.path.isfile(path):
            print(f"Error: {path} not found (run from FHTest2/)")
            sys.exit(1)

    # Build JP -> EN TEXT.DAT offset mapping
    print("Building TEXT.DAT offset mapping from script.src diffs...")
    mapping = build_text_mapping(jp_script, en_script)
    print(f"  {len(mapping)} translated text entries found")
    print()

    # Prepare save/ directory
    os.makedirs('save', exist_ok=True)

    # Move existing save/ contents to old_$time/
    existing = glob.glob('save/*')
    if existing:
        timestamp = time.strftime('%Y%m%d_%H%M%S')
        old_dir = f'save_old_{timestamp}'
        print(f"Moving existing save/ contents to {old_dir}/")
        os.makedirs(old_dir, exist_ok=True)
        for f in existing:
            shutil.move(f, old_dir)
        print()

    # Process files
    files = sorted(glob.glob(os.path.join(save_dir, '*')))
    if not files:
        print(f"No files found in {save_dir}/")
        sys.exit(1)

    print(f"Converting saves from {save_dir}/ to save/...")
    for f in files:
        basename = os.path.basename(f)
        dest = os.path.join('save', basename)

        if basename == 'continue.dat':
            print(f"  {basename}: skipped (continue.dat)")
            continue

        if basename.startswith('save') and basename.endswith('.dat') and basename != 'system.dat':
            convert_save(f, dest, mapping)
        else:
            shutil.copy2(f, dest)
            print(f"  {basename}: copied as-is")

    print()
    print("Done.")


if __name__ == '__main__':
    main()
