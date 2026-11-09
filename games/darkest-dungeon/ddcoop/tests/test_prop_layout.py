"""Read-only exact native prop-event/pickup evidence. Does not run the game."""
from pathlib import Path
import hashlib,struct,sys
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest()=='f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe=struct.unpack_from('<I',data,0x3c)[0];opt=pe+24
table=opt+struct.unpack_from('<H',data,pe+20)[0]
sections=[struct.unpack_from('<8sIIII',data,table+i*40) for i in range(struct.unpack_from('<H',data,pe+6)[0])]
def read(va,n):
    rva=va-0x140000000
    for name,virtual,base,size,offset in sections:
        if base<=rva and rva+n<=base+size:return data[offset+rva-base:offset+rva-base+n]
    raise AssertionError(hex(va))
assert 0x20b8+0xd88==0x2e40
assert read(0x140774801,7).hex()=='83bb402e000000' # native event assertion condition
assert read(0x140774808,2).hex()=='7442' # zero skips assert
assert read(0x14071477d,4).hex()=='4c8b4108' # functor+8 panel
assert read(0x140714781,8).hex()=='4183b8880d000002' # mode2
assert read(0x14071478f,7).hex()=='498b88700d0000' # obstacle prop
assert read(0x1407147a2,3).hex()=='ff5028' # native prop Interact
assert read(0x14071484d,10).hex()=='c781880d000003000000' # native closing state3
# Distinct X functors: curio and obstacle have different prop fields and the
# obstacle callback additionally clears its own completion flag.
assert struct.unpack('<Q',read(0x140ec8508+16,8))[0]==0x140714270
assert struct.unpack('<Q',read(0x140ec8230+16,8))[0]==0x140714540
for start,mode,closing in [(0x140714270,0x140714281,0x140714313),(0x140714540,0x140714551,0x1407145e3)]:
    assert read(start,10).hex()=='48895c2410574883ec70' # three complete copied instructions
    assert read(mode,7).hex()=='83bb880d000002' # only ready2
    assert read(closing,10).hex()=='c781880d000003000000' # native fade3, never event0
assert read(0x1407145f1,7).hex()=='c680550e000000'
# SELECT writes the selected skill; the normal UI update then advances its
# input state. Requiring target30 before SELECT creates a circular FIFO wait.
assert read(0x1405ce02c,7).hex()=='488986e0080000'
assert read(0x140765dae,3).hex()=='8d43ff' # state-1 jump table
assert struct.unpack('<I',read(0x14076c2cc+4*(29-1),4))[0]==0x768a95
assert struct.unpack('<I',read(0x14076c2cc+4*(30-1),4))[0]==0x768f10
assert read(0x140768c22,7).hex()=='488bbbe0080000'
assert read(0x140768eac,2).hex()=='ffc2' # next input state
assert read(0x1407ee393,6).hex()=='8bb7b8000000' # display index
assert read(0x1407ee399,4).hex()=='488b5730' # exact selected Item
assert read(0x1407ee3a2,4).hex()=='488d4808' # drag Item destination
call=read(0x1407ee3a6,5)
assert call[0]==0xe8 and 0x1407ee3ab+struct.unpack_from('<i',call,1)[0]==0x1403f5fc0
print('PASS prop/input ABI: native zero-event guard, distinct curio/obstacle X and confirm, whole copied instructions, native fade3; SELECT skill pointer and native29/30 dispatch; canonical Item copy; exact build, no game/save changes')
