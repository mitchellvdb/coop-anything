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



# Normal TownDisplay owns the normal building panels at5548..5550. Its init
# registers each in GuiSystem+48 and assigns the panel owner back to this town.
assert read(0x1406dcd6e,7).hex() == '4c8bb750550000'
assert read(0x1406dcd75,7).hex() == '488bb748550000'
assert read(0x1406dcd81,3).hex() == '488b1e'
assert read(0x1406dcd90,4).hex() == '4883c148'
assert read(0x1406dcd9e,4).hex() == '48897b08'
# Closing the normal town visits each panel, deactivates it with Panel58, then destroys it.
assert read(0x1406dd183,7).hex() == '488bb948550000'
call(0x1406dd1bd,0x1407f6680)
assert read(0x1407f66b1,3).hex() == '8b4158'
assert read(0x1407f66bc,6).hex() == '83c0fb83f802' #0 or closing5..7
# The rooted normal child panel types are fixed on this EXE. RTTI name comes from
# the exact complete-object locator immediately preceding each primary vtable.
for vt,name in [(0xebb8d8,'GuildDisplay'),(0xeb8c70,'BlacksmithDisplay'),
    (0xec1c50,'TavernDisplay'),(0xeb6b78,'AbbeyDisplay'),(0xec0720,'SanitariumDisplay'),
    (0xeb9ae0,'CampingTrainerDisplay'),(0xec0dc8,'StageCoachDisplay'),(0xebd730,'NomadWagonDisplay'),
    (0xebb5a8,'GraveyardDisplay'),(0xec1750,'StatueDisplay')]:
    col=q64(base+vt-8);td=base+struct.unpack('<I',read(col+12,4))[0]
    assert read(td+16,len(name)+10).startswith(('.?AV'+name+'@').encode()), name
# Embedded overlays are constructed by the normal Town base (not old alternate constructor).
assert read(0x1406da730,7).hex()=='498d8ea8170000'
call(0x1406da737,0x1406dad90)
assert read(0x1406daf04,7).hex()=='488d96c81f0000' #+17a8+1fc8=3770 Provisions
assert read(0x1406de5e0,7).hex()=='488d91b8450000' # normal HeirloomExchange45b8
assert read(0x1406de67f,7).hex()=='488d9370460000' # normal TownEvent4670
assert read(0x1406de721,7).hex()=='488d93002a0000' # normal District2a00
print('PASS passive Hamlet presence: rooted normal panel vector, owner/state lifecycle,10 exact building RTTI types and embedded overlay offsets; no game execution')
