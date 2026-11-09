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
assert read(0x1407ee393,6).hex()=='8bb7b8000000' # display index
assert read(0x1407ee399,4).hex()=='488b5730' # exact selected Item
assert read(0x1407ee3a2,4).hex()=='488d4808' # drag Item destination
call=read(0x1407ee3a6,5)
assert call[0]==0xe8 and 0x1407ee3ab+struct.unpack_from('<i',call,1)[0]==0x1403f5fc0
print('PASS prop ABI: native zero-event guard, distinct obstacle confirm/mode2/close3, canonical selected Item copy; exact build, no game/save changes')
