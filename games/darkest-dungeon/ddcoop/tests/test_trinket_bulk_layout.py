"""Read-only normal Unequip All model/confirmation and current RealmInventory layout proof."""
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



# RealmInventory Yes functor's call slot reaches6ae150; it uses normal campaign +e88, then the model helper.
assert q64(0x140ebf0b0+16)==0x1406ae150
assert read(0x1406ae16a,5).hex()=='bf880e0000'
assert read(0x1406ae194,6).hex()=='488bd7488bc8'
call(0x1406ae19a,0x14058ce70)
assert read(0x1406ae19f,10).hex()=='c7833805000001000000' # panel dirty+538
call(0x1406ae1c9,0x140585a60)
# Normal TownDisplay constructs its base at+17a8, RealmInventory at base+28b0 =+4058.
assert read(0x1406da730,7).hex()=='498d8ea8170000'
call(0x1406da737,0x1406dad90)
assert read(0x1406daffd,7).hex()=='488d8eb0280000'
call(0x1406db004,0x1406a4e50)
lea(0x1406a4eb4,0x140ebef28)
# Roster helper visits the live vector, skips status3, transfers via Inventory Add, clears and refreshes each hero.
assert read(0x14058ce7e,8).hex()=='4c8b6120488b7928'
assert read(0x14058cec4,7).hex()=='83bbb015000003'
call(0x14058cf9c,0x1405df5b0)
call(0x14058cfd5,0x1405de910)
assert read(0x14058d05a,7).hex()=='c6832c16000001'
call(0x14058d064,0x140481a40)
print('PASS Unequip All ABI: confirmed normal caller -> Roster58ce70, native estate transfer/dead exclusion/clear/actor refresh; current RealmInventory Town+4058 and dirty+538; no game execution')
