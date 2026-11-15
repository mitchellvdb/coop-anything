"""Read-only normal campaign Wave Continue ABI proof; never executes game code."""
from pathlib import Path
import hashlib,struct,sys
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest()=='f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe=struct.unpack_from('<I',data,0x3c)[0]
first=pe+24+struct.unpack_from('<H',data,pe+20)[0]
secs=[struct.unpack_from('<8sIIII',data,first+40*i) for i in range(struct.unpack_from('<H',data,pe+6)[0])]
def read(rva,n):
 for _,_,base,size,raw in secs:
  if base<=rva and rva+n<=base+size:return data[raw+rva-base:raw+rva-base+n]
 raise AssertionError(hex(rva))
def equal(rva,s):assert read(rva,len(bytes.fromhex(s)))==bytes.fromhex(s),hex(rva)
def rel(rva):
 b=read(rva,5);assert b[0]==0xe8
 return rva+5+struct.unpack_from('<i',b,1)[0]
# Normal RaidDisplay button+4258 captures its display at functor+8. This is
# the callback in its std::function vtable, not an automatic wave update.
equal(0x75860f,'48 8d 05 a2 b4 77 00 48 89 85 98 00 00 00')
equal(0x75861d,'4c 89 ad a0 00 00 00')
equal(0x758668,'49 8d 95 58 42 00 00')
assert struct.unpack('<Q',read(0xed3ab8+0x10,8))[0]==0x140780240
equal(0x780240,'48 83 ec 28 48 8b 49 08') #8 whole hook bytes, no relative instruction
assert rel(0x780248)==0x76fda0
equal(0x78024d,'33 c0 48 83 c4 28 c3')
# Native quest definition+40 is compared with the initialized wave type key;
# only the normal wave branch changes WaveLogic.waiting+3d0. Both wave and
# ordinary native paths start display fade+46ac.
equal(0x76fdcc,'48 8b 85 c0 52 00 00 48 8b 90 08 01 00 00')
equal(0x76fdda,'8b 05 b8 87 eb 01 39 42 40 75 21')
equal(0x76fde5,'48 8b 9d 38 5f 00 00')
equal(0x76fdff,'c6 83 d0 03 00 00 01')
equal(0x76ffb4,'41 c6 87 ac 46 00 00 01')
# FinishTeleportWait clears native waiting and advances WaveLogic. MoveToNewArea
# reads the parent raid directly; no call to the normal door770080 is involved.
equal(0x75afd5,'c6 83 d0 03 00 00 00')
equal(0x75afe3,'83 bb 64 02 00 00 02')
equal(0x75aff6,'48 8d 15 83 c2 73 00')
assert rel(0x75b04a)==0x50d310
equal(0x50d350,'83 bf 68 02 00 00 00 48 8b 17')
equal(0x50d371,'48 8b 47 20 bd 01 00 00 00 8b 4f 38')
print('PASS normal wave Continue functor, rooted quest/WaveLogic, native teleport/fade and automatic move ABI')
