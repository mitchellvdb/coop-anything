"""Read-only proof for the normal camping-skill actor/selected-skill indices.

Uses exact supported EXE bytes; no native execution, saves, Steam or dependencies.
"""
from pathlib import Path
import hashlib
import struct
import sys

exe = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(exe).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe = struct.unpack_from('<I', exe, 0x3c)[0]
table = pe + 24 + struct.unpack_from('<H', exe, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', exe, table + i * 40) for i in range(struct.unpack_from('<H', exe, pe + 6)[0])]
text = next(s for s in sections if s[0].rstrip(b'\0') == b'.text')


def read(va, size):
    rva = va - 0x140000000
    assert text[2] <= rva and rva + size <= text[2] + text[3]
    offset = text[4] + rva - text[2]
    return exe[offset:offset + size]


def exact(va, hex_bytes):
    expected = bytes.fromhex(hex_bytes)
    assert read(va, len(expected)) == expected, hex(va)


def call(va, target):
    raw = read(va, 5)
    assert raw[0] == 0xe8 and va + 5 + struct.unpack_from('<i', raw, 1)[0] == target, hex(va)


# Normal button supplies party index in argument x, distinct from y/z on stack.
exact(0x14070af42, '45 8b ca')             # r9d = r10d (x)
exact(0x14070af45, '49 8d 57 08')          # party = raid+8
call(0x14070af49, 0x1405c7420)
exact(0x1405c7455, '45 8b f1')             # preserve x in r14d
exact(0x1405c7470, '8b 95 f0 02 00 00')    # y -> edx
exact(0x1405c7481, '48 8b 07 4a 8b 34 f0') # actor = party.begin[x], no bound here
call(0x1405c748f, 0x1405d3b80)
exact(0x1405c7494, '4c 8b e0')             # r12 = skill lookup result
exact(0x1405c74d4, '41 8b 44 24 50')       # dereference result without NULL guard
# Lookup checks y against the selected vector, returns NULL if out of range.
exact(0x1405d3b87, '44 8b ca')
exact(0x1405d3b8c, '4d 8b 83 98 13 00 00')
exact(0x1405d3b93, '49 8b 83 a0 13 00 00')
exact(0x1405d3ba1, '4c 3b c8 73 6c')
exact(0x1405d3c12, '48 8b c1 48 83 c4 08 c3')
print('PASS camping ABI: exact normal button/native call chain; actor rank indexes party directly, selected skill lookup may return NULL; adapter must validate both against live vectors; read-only')
