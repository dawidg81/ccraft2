#!/usr/bin/env python3
"""
Convert ccraft2 level files from old format to new format.
Old format: CCRMCLVL header + mode + dimensions + chunked block array
New format: Raw block data stream (only non-zero blocks), each block 7 bytes: x(2) + z(2) + y(2) + id(1)
"""

import os
import struct
from pathlib import Path

def read_old_level(filepath):
    """Read old level format and return dimensions and blocks"""
    try:
        with open(filepath, 'rb') as f:
            # Magic bytes
            magic = f.read(8)
            if magic != b'CCRMCLVL':
                print(f"  WARNING: Invalid magic in {filepath}, skipping")
                return None
            
            # World mode
            mode = struct.unpack('B', f.read(1))[0]
            
            # World boundaries (3 shorts, big-endian)
            bounds = f.read(6)
            sizeX = struct.unpack('>H', bounds[0:2])[0]
            sizeY = struct.unpack('>H', bounds[2:4])[0]
            sizeZ = struct.unpack('>H', bounds[4:6])[0]
            
            # Read block array in chunk order
            blocks = {}  # (x, y, z) -> id
            chunksX = (sizeX + 15) // 16
            chunksY = (sizeY + 15) // 16
            chunksZ = (sizeZ + 15) // 16
            
            for cy in range(chunksY):
                for cz in range(chunksZ):
                    for cx in range(chunksX):
                        for ly in range(16):
                            for lz in range(16):
                                for lx in range(16):
                                    block_id = struct.unpack('B', f.read(1))[0]
                                    wx = cx * 16 + lx
                                    wy = cy * 16 + ly
                                    wz = cz * 16 + lz
                                    
                                    if wx < sizeX and wy < sizeY and wz < sizeZ:
                                        if block_id != 0x00:  # Only store non-air blocks
                                            blocks[(wx, wy, wz)] = block_id
            
            return {
                'sizeX': sizeX,
                'sizeY': sizeY,
                'sizeZ': sizeZ,
                'blocks': blocks
            }
    except Exception as e:
        print(f"  ERROR reading file: {e}")
        return None

def write_new_level(filepath, data):
    """Write level in new format (raw block data)"""
    with open(filepath, 'wb') as f:
        # Write each non-zero block as 7 bytes: x(2) + z(2) + y(2) + id(1)
        for (x, y, z), block_id in sorted(data['blocks'].items()):
            # Pack: x (big-endian short) + z (big-endian short) + y (big-endian short) + id (byte)
            buf = struct.pack('>HHH', x, z, y) + struct.pack('B', block_id)
            f.write(buf)

def convert_file(filepath):
    """Convert a single level file from old to new format"""
    print(f"Converting {filepath}...")
    
    old_data = read_old_level(filepath)
    if old_data is None:
        return False
    
    block_count = len(old_data['blocks'])
    print(f"  Found {block_count} blocks (dimensions: {old_data['sizeX']}x{old_data['sizeY']}x{old_data['sizeZ']})")
    
    # Backup the old file
    backup_path = filepath + '.bak'
    if not os.path.exists(backup_path):
        try:
            with open(filepath, 'rb') as src:
                with open(backup_path, 'wb') as dst:
                    dst.write(src.read())
            print(f"  Old file backed up to {backup_path}")
        except Exception as e:
            print(f"  WARNING: Could not backup file: {e}")
    
    # Write new format
    try:
        write_new_level(filepath, old_data)
        print(f"  Successfully converted to new format")
        return True
    except Exception as e:
        print(f"  ERROR writing new format: {e}")
        return False

def main():
    print("=== ccraft2 Level File Format Converter ===\n")
    
    # Find all .lvl files
    lvl_files = []
    
    # Main maps directory
    maps_dir = Path('maps')
    if maps_dir.exists():
        for lvl in sorted(maps_dir.glob('*.lvl')):
            # Skip .bak and .old files
            name = str(lvl)
            if not (name.endswith('.bak') or name.endswith('.old')):
                lvl_files.append(name)
    
    # Backups directory
    backups_dir = Path('maps/backups')
    if backups_dir.exists():
        for subdir in sorted(backups_dir.iterdir()):
            if subdir.is_dir():
                for lvl in sorted(subdir.glob('*.lvl')):
                    name = str(lvl)
                    if not (name.endswith('.bak') or name.endswith('.old')):
                        lvl_files.append(name)
    
    if not lvl_files:
        print("No .lvl files found!")
        return
    
    print(f"Found {len(lvl_files)} level files to convert:\n")
    
    converted = 0
    failed = 0
    
    for lvl_file in lvl_files:
        try:
            if convert_file(lvl_file):
                converted += 1
            else:
                failed += 1
        except Exception as e:
            print(f"  FATAL ERROR: {e}")
            failed += 1
        print()
    
    print("=" * 40)
    print(f"Conversion Complete")
    print(f"Successfully converted: {converted}")
    print(f"Failed: {failed}")
    print("=" * 40)

if __name__ == '__main__':
    main()
