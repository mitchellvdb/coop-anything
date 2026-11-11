"""Read-only proof of logical Stagecoach candidate fingerprint fields.
No native execution, process launch, save access, installed writes or network.
"""
from pathlib import Path
import argparse
import hashlib
import struct

p = argparse.ArgumentParser()
p.add_argument('exe', type=Path)
args = p.parse_args()
data = args.exe.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe + 4] == b'PE\0\0'
optional = pe + 24
assert struct.unpack_from('<H', data, optional)[0] == 0x20b
table = optional + struct.unpack_from('<H', data, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', data, table + i * 40)
            for i in range(struct.unpack_from('<H', data, pe + 6)[0])]


def read(rva, n):
    for _, _, start, size, offset in sections:
        if start <= rva and rva + n <= start + size:
            return data[offset + rva - start:offset + rva - start + n]
    raise AssertionError(hex(rva))


def exact(rva, expected):
    expected = bytes.fromhex(expected)
    assert read(rva, len(expected)) == expected, hex(rva)


def call(rva, target):
    code = read(rva, 5)
    assert code[0] == 0xe8, hex(rva)
    assert rva + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(rva)
    return rva + 5


# Native Stagecoach callback locates Entry by stable ID and only marks availability.
exact(0x6c3c92, '8b 9a 7c 13 00 00')
exact(0x6c3cd5, '48 8b 47 68 48 8b 4f 70')
exact(0x6c3ce2, '39 90 7c 13 00 00')
exact(0x6c3cea, '48 05 b0 15 00 00')
exact(0x6c3cf7, 'c6 80 a8 15 00 00 00')
# Base Hero save names the fixed character-name payload at +8.
exact(0x47dc63, '4d 8d 46 08')
# HeroEntry save uses class name+8, length40h; XP and equipment levels follow.
exact(0x5d1b05, '4c 8b 86 20 13 00 00 49 83 c0 08')
exact(0x5d1b13, '41 b9 40 00 00 00')
exact(0x5d1b29, '4c 8d 86 28 13 00 00')
exact(0x5d1c13, '4c 8d 86 2c 13 00 00')
exact(0x5d1c2d, '4c 8d 86 30 13 00 00')
# Native class restriction compares the stable class ID at class+D4.
exact(0x4bd1eb, '48 8b 91 20 13 00 00 8b 8a d4 00 00 00')
# The serialized quirks node uses Entry+1380..1388, stride38.
exact(0x5d1dec, '48 8d 15 05 9c 89 00')
exact(0x5d1df9, '4c 8b b6 88 13 00 00 48 8b 9e 80 13 00 00')
exact(0x5d1e13, '48 8b 13')  # quirk definition is the persistent node name
exact(0x5d1e1f, '4c 8d 43 29')
exact(0x5d1e36, '4c 8d 43 2a')
exact(0x5d1e4d, '4c 8d 43 2c')
exact(0x5d1e64, '4c 8d 43 20')
exact(0x5d1e7b, '4c 8d 43 24')
exact(0x5d1e92, '4c 8d 43 28')
exact(0x5d1ea9, '4c 8d 43 30')
exact(0x5d1ec6, '48 83 c3 38')
# The loader hashes the quirk name and compares definition+40; same38-byte row.
exact(0x5d2893, '48 6b c8 38')
exact(0x5d2897, '49 8b 04 0a 39 50 40')
# Selected indices are stored separately from learned skill levels.
exact(0x5d1f00, '48 8b 96 c8 13 00 00 48 8b 86 d0 13 00 00')
exact(0x5d1f8a, '48 8b 96 98 13 00 00 48 8b 86 a0 13 00 00')
exact(0x5ccaf0, '48 8b 97 e0 13 00 00 48 8b 87 e8 13 00 00')
exact(0x5ccb70, '48 8b 8f b0 13 00 00 48 8b 87 b8 13 00 00')
print('PASS Stagecoach logical fingerprint ABI: candidate stride/ID/availability, name/class, XP/equipment, skills and persisted quirk fields; pinned EXE, read-only')
