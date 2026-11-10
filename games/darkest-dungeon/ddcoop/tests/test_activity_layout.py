"""Exact supported-build proof of activity preview/setter/drop/commit separation.
Read-only: no process launch, native calls, saves, Steam or installed writes.
"""
from pathlib import Path
import hashlib, struct, sys
raw = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(raw).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe = struct.unpack_from('<I', raw, 0x3c)[0]
table = pe + 24 + struct.unpack_from('<H', raw, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', raw, table + 40*i) for i in range(struct.unpack_from('<H', raw, pe+6)[0])]
def read(rva, n):
    for _, _, va, size, offset in sections:
        if va <= rva and rva+n <= va+size:
            return raw[offset+rva-va:offset+rva-va+n]
    raise AssertionError(hex(rva))
def exact(rva, expected):
    expected = bytes.fromhex(expected)
    assert read(rva, len(expected)) == expected, hex(rva)
def call(rva, target):
    expected = read(rva, 5)
    assert expected[0] == 0xe8 and rva+5+struct.unpack_from('<i', expected, 1)[0] == target, hex(rva)
def ptr(rva): return struct.unpack('<Q', read(rva, 8))[0] - 0x140000000
# Known safe copied prologues: two complete five-byte MOVs, or three pushes+SUB.
exact(0x7e1dc0, '40 56 41 55 41 57 48 83 ec 30')
exact(0x7e1fd0, '48 89 5c 24 10 48 89 74 24 20')
exact(0x7e1c90, '48 89 5c 24 10 48 89 74 24 20')
# The source drag callbacks contain inline gameplay clear writes before SetHero.
# Their hook prologues also contain whole instructions with no relative operands.
exact(0x7e2630, '48 89 5c 24 10 48 89 6c 24 18')
exact(0x7e2740, '40 53 41 56 48 83 ec 28 4c 8b f1')
assert ptr(0xee0500+0x30) == 0x7e1dc0
assert ptr(0xee0500) == 0x7e1c90
assert ptr(0xee04c0+0x20) == 0x7e1fd0
assert ptr(0xee0500+0x8) == 0x7e2530
assert ptr(0xee0500+0x10) == 0x7e2600
assert ptr(0xee0500+0x18) == 0x7e2740
assert ptr(0xee0500+0x20) == 0x7e2630
# Generic drag manager: accepted=1 invokes source-finish; refuse=0 restores;
# special=2 skips both, so an intercepted target must choose deliberately.
exact(0x28a1e5,'83 f8 01 75 1f')
exact(0x28a1ff,'41 ff 50 18')
exact(0x28a209,'85 c0 75 18')
exact(0x28a222,'ff 50 10')
exact(0x28a259,'ff 50 20')
# Ordinary HeroDragData is the exact three-method type; +8 stores Hero*.
assert ptr(0xeddca0) == 0x7ce710
exact(0x7ce850, '48 8b 43 08 8b 80 7c 13 00 00')
# Native drop receives widget+0x10, so the setter is invoked on this-0x10.
call(0x7e2002, 0x43aa10)
exact(0x7e2096, '48 8d 4e f0')
call(0x7e209a, 0x7e1f00) # committed model occupancy
call(0x7e20a7, 0x7e32d0) # widget's selected hero -> roster entry
exact(0x7e20ac, '8b 18') # fault address: no NULL check after resolver
exact(0x7e2124, '49 8b 5e 08 48 8b 46 f0 48 8b d3 48 8d 4e f0 ff 50 30')
# Resolver checks widget+28 first and may return NULL, independently of model slot.
exact(0x7e32db, '48 39 41 28 74 62')
exact(0x7e3318, '8b 91 7c 13 00 00')
exact(0x7e3323, '39 91 84 13 00 00')
# For six stress activities, preview notification is a no-op virtual, not payment.
for vt in [0xe9d728,0xe9d618,0xe9d6a0,0xe9aea8,0xe9ad28,0xe9adb0]:
    assert ptr(vt+0x38) == 0x2340a0
exact(0x2340a0,'c2 00 00')
# Setter writes UI Hero* and visible/enabled flags; clearing also undoes model data.
exact(0x7e1e95,'4c 89 7e 28')
exact(0x7e1ea5,'45 8b 87 7c 13 00 00')
exact(0x7e1eaf,'c6 46 0c 01 c6 46 20 01')
exact(0x7e1e48,'83 bb b0 15 00 00 01')
call(0x7e1e5a,0x58b500)
exact(0x7e1e5f,'4c 89 ab c0 15 00 00')
exact(0x7e1edc,'4c 89 2c d1 44 88 6c d1 14')
# ChangeLocation writes immediately, then notifies roster observers. Therefore a
# release postcheck must inspect the actual location rather than assume a queue.
exact(0x58b587,'44 89 87 b0 15 00 00')
exact(0x7e267d,'4f 89 24 c8 47 88 64 c8 14')
exact(0x7e2798,'4f 89 24 c8 47 88 64 c8 14')
# Explicit accept checks eligibility, debits funds, marks location2 and slot committed.
call(0x53610a,0x535e50)
call(0x5361c5,0x568600)
exact(0x536218,'41 b8 02 00 00 00')
call(0x536224,0x58b500)
exact(0x536229,'48 89 b7 c0 15 00 00')
exact(0x536272,'44 89 3c df c7 44 df 04 01 00 00 00')
print('PASS activity ABI: exact setter/drop/dtor boundaries, six stress preview no-ops, widget/model split and observed7e20ac NULL dereference; commit/payment/clear fields proved read-only')

call(0x7e1929,0x536300)
call(0x7e1936,0x5363a0)
