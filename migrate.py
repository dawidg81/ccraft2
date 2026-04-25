#!/usr/bin/env python3
"""
migrate.py - Convert ccraft2 .lvl files from old format to new CCRMCLVL format.

Old format: sequence of 7-byte entries (x:2, y:2, z:2, block_id:1), air not stored.
New format: CCRMCLVL magic, 1-byte mode, 3x2-byte dimensions, full block array in chunk order.

Usage:
    python3 migrate.py                  # migrate all .lvl files in maps/ and maps/backups/
    python3 migrate.py path/to/file.lvl # migrate a single file
"""

import sys
import os
import struct
import shutil

WORLD_SIZE_X = 256
WORLD_SIZE_Y = 64
WORLD_SIZE_Z = 256

MAGIC = b"CCRMCLVL"


def is_new_format(path: str) -> bool:
    with open(path, "rb") as f:
        header = f.read(8)
    return header == MAGIC


def migrate_file(path: str) -> bool:
    """
    Migrate a single .lvl file in-place (original is backed up as .lvl.old).
    Returns True on success, False on failure.
    """
    if is_new_format(path):
        print(f"  [SKIP] {path} - already in new format")
        return True

    # Read all 7-byte entries
    blocks = [0x00] * (WORLD_SIZE_X * WORLD_SIZE_Y * WORLD_SIZE_Z)

    def idx(x, y, z):
        return x + z * WORLD_SIZE_X + y * WORLD_SIZE_X * WORLD_SIZE_Z

    file_size = os.path.getsize(path)
    if file_size % 7 != 0:
        print(f"  [FAIL] {path} - file size {file_size} is not a multiple of 7, may be corrupt")
        return False

    with open(path, "rb") as f:
        while True:
            entry = f.read(7)
            if len(entry) == 0:
                break
            if len(entry) != 7:
                print(f"  [FAIL] {path} - truncated entry at end of file")
                return False
            x = (entry[0] << 8) | entry[1]
            y = (entry[2] << 8) | entry[3]
            z = (entry[4] << 8) | entry[5]
            block_id = entry[6]

            if x >= WORLD_SIZE_X or y >= WORLD_SIZE_Y or z >= WORLD_SIZE_Z:
                print(f"  [WARN] {path} - block out of bounds at ({x},{y},{z}), skipping")
                continue

            blocks[idx(x, y, z)] = block_id

    # Back up original
    backup_path = path + ".old"
    shutil.copy2(path, backup_path)

    # Write new format
    chunks_x = (WORLD_SIZE_X + 15) // 16
    chunks_y = (WORLD_SIZE_Y + 15) // 16
    chunks_z = (WORLD_SIZE_Z + 15) // 16

    with open(path, "wb") as f:
        f.write(MAGIC)                          # 8 bytes magic
        f.write(b"\x00")                        # 1 byte mode (finite)
        f.write(struct.pack(">HHH",             # 6 bytes dimensions
                            WORLD_SIZE_X,
                            WORLD_SIZE_Y,
                            WORLD_SIZE_Z))

        # Block array in chunk order
        buf = bytearray()
        for cy in range(chunks_y):
            for cz in range(chunks_z):
                for cx in range(chunks_x):
                    for ly in range(16):
                        for lz in range(16):
                            for lx in range(16):
                                wx = cx * 16 + lx
                                wy = cy * 16 + ly
                                wz = cz * 16 + lz
                                if wx < WORLD_SIZE_X and wy < WORLD_SIZE_Y and wz < WORLD_SIZE_Z:
                                    buf.append(blocks[idx(wx, wy, wz)])
                                else:
                                    buf.append(0x00)  # padding outside world bounds
        f.write(buf)

    old_size = os.path.getsize(backup_path)
    new_size = os.path.getsize(path)
    saving = old_size - new_size
    sign = "-" if saving >= 0 else "+"
    print(f"  [OK]   {path}  ({old_size} -> {new_size} bytes, {sign}{abs(saving)} bytes)")
    return True


def find_lvl_files(root: str):
    result = []
    for dirpath, _, filenames in os.walk(root):
        for fname in filenames:
            if fname.endswith(".lvl"):
                result.append(os.path.join(dirpath, fname))
    return result


def main():
    if len(sys.argv) >= 2:
        targets = sys.argv[1:]
    else:
        targets = find_lvl_files("maps")
        if not targets:
            print("No .lvl files found under maps/")
            return

    ok = 0
    fail = 0
    skip = 0

    for path in targets:
        if not os.path.isfile(path):
            print(f"  [FAIL] {path} - file not found")
            fail += 1
            continue
        print(f"Migrating: {path}")
        if is_new_format(path):
            print(f"  [SKIP] already in new format")
            skip += 1
        elif migrate_file(path):
            ok += 1
        else:
            fail += 1

    print(f"\nDone. {ok} migrated, {skip} skipped, {fail} failed.")
    if ok > 0:
        print("Originals backed up as <filename>.lvl.old")


if __name__ == "__main__":
    main()
