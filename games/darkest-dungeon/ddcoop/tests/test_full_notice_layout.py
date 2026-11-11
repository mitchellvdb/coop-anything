"""Read-only native evidence for loot's informational inventory-full dialog.
No game execution, save access, DLL injection or third-party decoder required.
"""
from pathlib import Path
import hashlib, struct, sys
data = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe = struct.unpack_from('<I', data, 0x3c)[0]
table = pe + 24 + struct.unpack_from('<H', data, pe+20)[0]
sections = [struct.unpack_from('<8sIIII', data, table+i*40) for i in range(struct.unpack_from('<H',data,pe+6)[0])]
def read(va,n):
    rva=va-0x140000000
    for _,v,base,size,pos in sections:
        if base<=rva and rva+n<=base+size:return data[pos+rva-base:pos+rva-base+n]
    raise AssertionError(hex(va))
def exact(va,hexbytes): assert read(va,len(bytes.fromhex(hexbytes)))==bytes.fromhex(hexbytes),hex(va)
def call(at,target):
    b=read(at,5);assert b[0]==0xe8 and at+5+struct.unpack_from('<i',b,1)[0]==target
def rip(at,target):
    b=read(at,7);assert at+7+struct.unpack_from('<i',b,3)[0]==target
assert read(0x140ec8a68,len(b'not_enough_room_confirm\0'))==b'not_enough_room_confirm\0'
# Take-all and one-item routes, not a general-purpose modal/Yes button.
rip(0x140716e5d,0x140ec8a68);call(0x140716e64,0x1407ebc60)
rip(0x140717406,0x140ec8a68);call(0x140717422,0x1407c6fc0)
# Both callers provide an empty std::function (+38 in params), so OK cannot
# execute a gameplay choice. The helper forwards the name to Add unchanged.
exact(0x1407ebcaf,'48c78424a800000000000000')
exact(0x1407ebccb,'4c8d442470');call(0x1407ebcd3,0x1407c6fc0)
exact(0x1407173fa,'48c78424b000000000000000')
exact(0x14071741a,'4c8d442478')
# Add copies the supplied name into the Entry's first 512 bytes. Constructor
# leaves the following button vector at +200/+208/+210.
exact(0x1407c7014,'4c8bc3');exact(0x1407c7017,'ba00020000')
exact(0x1407c701c,'488d8dd0000000');exact(0x1407c7105,'488d95d0000000')
call(0x1407c710f,0x1407c7200)
exact(0x1407c7082,'488b4f38')
# Show passes the last actual ring entry to ShowEntry; not a guessed pointer.
exact(0x1407c6b8b,'48837f2000')
exact(0x1407c6d3f,'488b4710');exact(0x1407c6d43,'48ffc8')
exact(0x1407c6d46,'48234718');exact(0x1407c6d4a,'488b5708')
exact(0x1407c6d4e,'488b14c2');call(0x1407c6d55,0x1407c7640)
# Exact OK std::function dispatch. The handler optionally invokes +48, then
# brackets native removal-count increment with backend begin/end. Reusing it
# with the proven empty callback preserves normal native dialog lifetime.
assert struct.unpack('<Q',read(0x140edd930+16,8))[0]==0x1407c9530
exact(0x1407c9539,'488b4948');exact(0x1407c9548,'488b5b08')
exact(0x1407c954c,'488b4b28');exact(0x1407c9556,'ff4330')
# Native state enumeration demonstrates why phase33 is not proof that an
# input29 order at the same round/turn is obsolete.
for index,label in [(29,b'bs_turn_select_skill\0'),(33,b'bs_turn_presentation\0')]:
    ptr=struct.unpack('<Q',read(0x140deaff8+(index-29)*8,8))[0]
    assert read(ptr,len(label))==label
print('PASS inventory-full/phase ABI: exact loot-only name/callers, empty callbacks, native Entry ring/show/OK lifetime, battle29/33 enum; pinned EXE, read-only')
