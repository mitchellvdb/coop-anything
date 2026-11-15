"""Read-only confirmed Shift-discard capture and native effect ABI."""
from pathlib import Path
import runpy,struct
abi=runpy.run_path(str(Path(__file__).with_name('test_wave_layout.py')))
read,equal,rel=abi['read'],abi['equal'],abi['rel']
# Shift entry rejects quest items, builds captured inventory/index/Item pointer,
# and either calls737070 directly or the normal Yes functor. It is not a drag.
equal(0x736d05,'8b 0d 89 27 35 02')
equal(0x736d0d,'39 48 44 75 36')
equal(0x736d50,'48 89 74 24 28 44 89 74 24 30')
assert rel(0x736d73)==0x737070
assert struct.unpack('<Q',read(0xecaa88+0x10,8))[0]==0x140738510
equal(0x738510,'48 83 c1 08 e9 57 eb ff ff')
equal(0x737070,'40 53 48 83 ec 60 48 8b d1') #9 whole existing hook bytes
equal(0x737079,'48 8b 19 48 8b 4b 08 44 8b 42 08')
equal(0x7370ac,'48 8b 42 10 8b 10 29 11 83 39 00')
assert rel(0x7370b9)==0x5de910 # owning native Item clear
equal(0x7370c0,'89 53 24') # native occupied-slot recount
print('PASS confirmed Shift-discard quest guard, capture, native decrement/clear/recount, existing9-byte hook')
