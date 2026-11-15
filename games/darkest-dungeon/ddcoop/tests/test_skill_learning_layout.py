"""Read-only ABI proof for skill learning after a Guild purchase, exact Steam EXE. Never executes game code.

hamlet_upgrade_refresh calls 5d3c20 (camping levels, +13b0) and 5d41b0 (combat levels, +13e0) on every copy, then selects
a newly learned skill by the Guild callbacks' own tests: camping 673210 (fewer than four selected, more learned than
selected, then 5d3dd0) and combat 683900 (5d43e0(hero, index), selected count 5d4d40 below limit 5d50f0, then 5d4f70).
Usage: python tests/test_skill_learning_layout.py <path to supported Darkest.exe>
"""
from pathlib import Path
import hashlib
import struct
import sys

b = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
base = 0x140000000
pe = struct.unpack_from('<I', b, 60)[0]
q = pe + 24 + struct.unpack_from('<H', b, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', b, q + 40 * i) for i in range(struct.unpack_from('<H', b, pe + 6)[0])]


def read(va, size):
    for _, _, rva, n, raw in sections:
        if rva <= va - base and va - base + size <= rva + n:
            return b[raw + va - base - rva:raw + va - base - rva + size]
    raise AssertionError(hex(va))


def call(va, target):
    code = read(va, 5)
    assert code[0] == 0xe8, hex(va)
    assert va + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(va)


# Combat levels 5d41b0(hero): +13e0 resized to the class's combat skill count (+1320 class, +bd0..bd8, 24-byte entries).
assert read(0x1405d41cf, 7).hex() == '488b8120130000'
assert read(0x1405d41e3, 7).hex() == '4881c1e0130000'
call(0x1405d4200, 0x1403c8740)
# Camping levels 5d3c20(hero): +13b0 resized to the class's camping skill count (+10f8..1100, 4-byte entries).
assert read(0x1405d3c41, 7).hex() == '4881c1b0130000'
call(0x1405d3c5a, 0x1403c8740)
# Combat Guild callback 683900: hero = functor+10, index = functor+18; levels first, then the auto-select tests.
assert read(0x140683924, 4).hex() == '488b4910'
call(0x140683928, 0x1405d41b0)
assert read(0x14068392d, 7).hex() == '488b7710' + '8b5718'  # rsi = hero, edx = index
call(0x1406839cb, 0x1405d43e0)   # rcx = hero, edx = index still
call(0x1406839d7, 0x1405d4d40)   # selected count
call(0x1406839e1, 0x1405d50f0)   # limit
assert read(0x1406839e6, 4).hex() == '3bd8730c'  # cmp ebx,eax; jae: select only below the limit
call(0x1406839f1, 0x1405d4f70)   # hero_toggle_combat(hero, index)
# Camping Guild callback 673210: levels, then fewer than four selected and more learned than selected.
assert read(0x140673231, 4).hex() == '488b4910'
call(0x140673235, 0x1405d3c20)
assert read(0x140673250, 4).hex() == '4883ff04'  # cmp rdi,4 (selected count)
assert read(0x1406732c2, 3).hex() == '413bfa'  # cmp edi,r10d: selected below learned
call(0x1406732cd, 0x1405d3dd0)   # hero_toggle_camping(hero, index)
# 5d43e0 reads the index as its second argument (edx) against the class's combat skills.
assert read(0x1405d43e0, 2).hex() == '4053' and read(0x1405d4411, 2).hex() == '8bd2'
print('PASS skill learning ABI: 5d41b0 combat/+13e0 and 5d3c20 camping/+13b0 levels; Guild callbacks 683900/673210 refresh first, '
      'then select a newly learned skill by 5d43e0 + 5d4d40<5d50f0 (combat) or <4 selected and more learned (camping); no game execution')
