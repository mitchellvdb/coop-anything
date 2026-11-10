"""Read-only proof of ordinary prop-choice routes and the optional obstacle actor.
Does not execute game code, access saves or change installed files.
"""
from pathlib import Path
import argparse,hashlib,struct
p=argparse.ArgumentParser();p.add_argument('exe',type=Path);args=p.parse_args()
data=args.exe.read_bytes()
assert hashlib.sha256(data).hexdigest()=='f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe=struct.unpack_from('<I',data,0x3c)[0]
table=pe+24+struct.unpack_from('<H',data,pe+20)[0]
sections=[struct.unpack_from('<8sIIII',data,table+i*40) for i in range(struct.unpack_from('<H',data,pe+6)[0])]
def read(rva,n):
 for name,virtual_size,base,size,offset in sections:
  if base<=rva and rva+n<=base+size:return data[offset+rva-base:offset+rva-base+n]
 raise AssertionError('Unmapped pinned range')
def equal(rva,hexbytes):assert read(rva,len(bytes.fromhex(hexbytes)))==bytes.fromhex(hexbytes),hex(rva)
def rel(rva,opcode):
 b=read(rva,5);assert b[0]==opcode
 return rva+5+struct.unpack_from('<i',b,1)[0]

# Native overlay initialization clears the optional Hero*. Ordinary obstacle
# OPEN sets ready2 and d70 but leaves d78 unchanged; NULL is a normal state.
equal(0x70ce68,'45 33 ff') # r15 =0
equal(0x70da3c,'4c 89 bf 78 0d 00 00') # actor=NULL
equal(0x7748bf,'48 8d 8b b8 20 00 00') # display+20b8 overlay
equal(0x7748cb,'45 33 c9') # native ordinary open's fourth argument=0
assert rel(0x7748d4,0xe8)==0x712b70
equal(0x712c22,'8b 83 88 0d 00 00 c7 83 88 0d 00 00 02 00 00 00 48 89 b3 70 0d 00 00')
assert rel(0x712c39,0xe9)==0x712e19
equal(0x712e19,'89 83 8c 0d 00 00') # previous mode only, then epilogue

# Hand callback has no selected-hero fallback. It passes d78 unchanged into
# Prop::Interact, then native result execution/destruction and fade3.
equal(0x714770,'48 89 5c 24 10 57 48 83 ec 70') # whole hook prologue
equal(0x71477d,'4c 8b 41 08')
equal(0x714781,'41 83 b8 88 0d 00 00 02')
equal(0x71478f,'49 8b 88 70 0d 00 00')
equal(0x714799,'4d 8b 80 78 0d 00 00 33 d2 ff 50 28')
equal(0x71484d,'c7 81 88 0d 00 00 03 00 00 00')
assert struct.unpack('<Q',read(0xeaa188+0x28,8))[0]==0x140607e60
equal(0x607e81,'49 8b d1')
assert rel(0x607e84,0xe9)==0x4afd40 # actor remains r8

# Tool functor -> captured-item body; inventory subtraction precedes Interact.
equal(0x714640,'48 83 c1 08')
assert rel(0x714644,0xe9)==0x70ff50
assert rel(0x70ffe2,0xe8)==0x5e15d0
equal(0x70ffea,'48 8b 88 70 0d 00 00')
equal(0x70fff4,'48 8b 80 78 0d 00 00')
equal(0x71000d,'41 ff 52 30')
equal(0x7100b8,'c7 81 88 0d 00 00 03 00 00 00')

# Curio hand/selected-item button calls7111e0, which explicitly falls back to
# raid.selected+438 if its optional captured actor is NULL.
equal(0x71436d,'48 8b 49 08')
assert rel(0x71437e,0xe8)==0x7111e0
equal(0x711227,'4c 8b a1 78 0d 00 00 4d 85 e4 75 0e')
equal(0x71123a,'4c 8b a0 38 04 00 00')

# Player item drag uses the overlay-owned Item copy7c0, not a bag-slot address.
equal(0x710fbd,'48 8d 93 c0 07 00 00 48 8b cb')
assert rel(0x710fc7,0xe8)==0x712e40
for rva in (0x714270,0x714540):
 equal(rva,'48 89 5c 24 10 57 48 83 ec 70')
assert rel(0x7142f4,0xe8)==0x35c620 # curio X fades, no Interact
assert rel(0x7145c4,0xe8)==0x35c620 # obstacle X fades, no Interact

# The earlier CurioInteractionItemSlot::OnDrop is the safe point before any
# optimistic consumption. Its embedded position is proved by native ctor.
equal(0x75693f,'48 8d 99 80 07 00 00') # overlay+780 slot
assert 0x756963+7+struct.unpack_from('<i',read(0x756963,7),3)[0]==0xec85b0
assert struct.unpack('<Q',read(0xec85b0+0x20,8))[0]==0x14070bf70
equal(0x70bf70,'48 89 5c 24 10 55 56 57') # eight whole trampoline bytes
equal(0x70bf78,'48 81 ec f0 05 00 00')
equal(0x70bf91,'49 8b f8 48 8b e9') # drag=r8, slot=rcx
assert struct.unpack('<Q',read(0xeddd10,8))[0]==0x1407ce880
assert rel(0x7ce880,0xe9)==0x694950 # InventoryItemDragData
assert struct.unpack('<Q',read(0xeddd48,8))[0]==0x1407ce9a0
assert rel(0x7ce9a0,0xe9)==0x6949c0 # EquipmentDragData
equal(0x70c010,'48 8d 57 08') # owning dragged Item, not its source pointer
equal(0x70c01f,'48 8d 4d 40') # slot Item at+40 =>overlay+7c0
assert rel(0x70c028,0xe8)==0x3f5fc0 # copy whole native Item
equal(0x70c02d,'c6 85 e8 05 00 00 01') # flag =>overlay+d68
equal(0x70c034,'8b 44 24 30 ff c8 89 44 24 30') # decrement before late Update
equal(0x70c057,'48 8d 4f 08')
assert rel(0x70c05b,0xe8)==0x3f5fc0 # overwrite dragged remaining Item
equal(0x70c0d6,'b8 01 00 00 00') # accepted native result

# Manager calls Drop+20 with drag inr8.1 invokes source Finish+18,0 invokes
# Restore+10, other values skip both. Restore can overwrite a synchronous
# authoritative consume, so a shared hook must suppress matching source cleanup
# or return a no-source-callback outcome and itself restore presentation flags.
equal(0x28a1c6,'4c 8b 40 08')
equal(0x28a1d6,'41 ff 51 20')
equal(0x28a1e5,'83 f8 01 75 1f')
equal(0x28a1ff,'41 ff 50 18')
equal(0x28a209,'85 c0 75 18')
equal(0x28a222,'ff 50 10')
equal(0x7ed1f4,'41 8b 46 08 2b 06 01 01') # restore dragCount-canonicalCount

# HeroUseItem429750 routes eligible items to a currently open curio before the
# ordinary hero-use branches. This decision is independent of the hero argument.
equal(0x429890,'48 83 bf 58 27 00 00 00') # overlay+6a0 present
equal(0x42989a,'8b 87 40 2e 00 00 ff c8 83 f8 01') # mode1..2
equal(0x4298a7,'41 8b d1') # item type into EDX
assert rel(0x4298aa,0xe8)==0x428fe0
equal(0x4298af,'84 c0 74 2d') # false ->ordinary hero use
equal(0x4298c7,'48 81 c1 b8 20 00 00 49 8b d5') # overlay, canonical Item
assert rel(0x4298d1,0xe8)==0x712e40
equal(0x428fea,'45 8b c8 44 8b c2') # (unusedRCX,typeEDX,idR8D)->matchtype/id
equal(0x428ff0,'74 5e') # two common curio item types accepted unconditionally
equal(0x428ff8,'74 56')
equal(0x429000,'75 47') # third type conditionally matches, others false
assert rel(0x42903a,0xe8)==0x4b12e0
equal(0x42903f,'c1 e8 1f 34 01') # signed index >=0 ->bool
equal(0x429049,'32 c0')
equal(0x429050,'b0 01')

# The precise item acceptance check is the pure interaction lookup, not the
# broader route predicate.712e40 checks it twice before copying metadata; it
# returnsfalse forunsupported items without a canonical inventory debit.
equal(0x712e7f,'48 8b 8b d0 00 00 00') # Prop option0
equal(0x712e8b,'48 8d 93 25 01 00 00') # prop variation string
equal(0x712e92,'44 8b 8f 88 00 00 00 44 8b 47 44')
assert rel(0x712e9d,0xe8)==0x4b12e0
equal(0x712ea2,'c1 e8 1f 84 c0') # negative index rejected
assert rel(0x712ed0,0xe8)==0x4b12e0
equal(0x4b12ea,'4c 8b 91 08 02 00 00') # match entries begin
equal(0x4b12f4,'48 8b 81 10 02 00 00') # end
equal(0x4b1320,'49 8b 0c da 3b b1 90 00 00 00') # entry type
equal(0x4b132c,'48 8b 01 44 8b c5 49 8b d6 ff 50 08') # pure Match(variation,id)
equal(0x4b135f,'b8 ff ff ff ff')
equal(0x4b136e,'8b c3') # matched entry index
equal(0x712f31,'89 8e 6c 07 00 00') # acceptedpath copies kind/id/index into overlay
equal(0x712f51,'89 8e 70 07 00 00 89 ae 74 07 00 00')
equal(0x712fde,'c7 86 88 0d 00 00 03 00 00 00 b0 01') # fade3, true
equal(0x7130ae,'32 c0') # rejected item returnsfalse
print('PASS prop-choice ABI: optional NULL actor, native OPEN/hand/tool/curio/cancel; pre-consumption typed drop/source restore; exact HeroUseItem curio route, signed acceptance lookup and metadata-only Item acceptance; exact build, read-only')
