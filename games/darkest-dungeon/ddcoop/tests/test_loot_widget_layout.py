"""Read-only proof of OverlayLoot's rooted InventoryBar and distinct UI Item copies."""
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


def q64(va):
    return struct.unpack('<Q', read(va, 8))[0]


def call(va, target):
    code = read(va, 5)
    assert code[0] == 0xe8, hex(va)
    assert va + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(va)


def lea(va, target):
    code = read(va, 7)
    assert code[:3] in (bytes.fromhex('488d05'), bytes.fromhex('4c8d05')), hex(va)
    assert va + 7 + struct.unpack_from('<i', code, 3)[0] == target, hex(va)



# Native drag pickup copies the widget's selected Item (+30), not its guessed displayed index.
assert read(0x1407ee399,4).hex()=='488b5730'
call(0x1407ee3a6,0x1403f5fc0)
# Loot Draw keeps Raid+5140 as its model wrapper, canonical Inventory at +100.
assert read(0x140715ea4,7).hex()=='488d8740510000'
assert read(0x14071617d,11).hex()=='488b75d04c8dbe00010000'
# Its separate InventoryBar at overlay+8 owns UI inventory at+8 and widget array at+10..18.
assert read(0x1407167b2,19).hex()=='4d8b5e084c8be7498b4b084c8b51084c8b4910'
assert read(0x140716814,4).hex()=='498b4b10'
assert read(0x14071681b,4).hex()=='498b4318'
assert read(0x14071682b,4).hex()=='4c8b24d1'
# Verify actual widget selected-item read and drag flag without relying on a raw global search.
assert read(0x14071682f,9).hex()=='4180bc24c000000000'
assert read(0x14071683a,5).hex()=='498b442430'
print('PASS loot widget ABI: native selected Item copy, exact current overlay InventoryBar/widget roots, canonical Raid+5240 inventory kept distinct; no game execution')
