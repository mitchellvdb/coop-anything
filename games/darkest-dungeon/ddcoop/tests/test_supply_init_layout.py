"""Read-only proof of the complete normal supply opener on the pinned EXE."""
from pathlib import Path
import runpy
import struct

abi = runpy.run_path(str(Path(__file__).with_name('test_quest_return_layout.py')))
read, relative = abi['read'], abi['relative']
# Normal TownDisplay callback obtains this from its local capture, always joins
# the ProvisionSelect state machine, then switches the application to supplies.
assert read(0x1406e7a7d,4).hex() == '488b5908'
assert read(0x1406e7b09,10).hex() == 'c787cc37000001000000'
assert read(0x1406e7b4a,7).hex() == '488d8f70370000'
assert relative(0x1406e7b51,'e8',5) == 0x1407f6a50
assert read(0x1406e7b67,10).hex() == 'c780800000000a000000'
# Opening invokes the normal ProvisionSelect initialization before advancing
# through its own states. Its native constructor vtable already has this method.
assert read(0x1407f6b4a,3).hex() == 'ff5070'
assert struct.unpack('<Q',read(0x140ebdde8+0x70,8))[0] == 0x140694df0
assert read(0x1407f6b7c,3).hex() == '894358'
# Free items then stock, before the first view allocation. No direct engine
# state patch or raw C++ Item memcpy is introduced by the bridge.
assert relative(0x140694ec6,'e8',5) == 0x14057b100
assert relative(0x140694eda,'e8',5) == 0x14057b7c0
assert relative(0x140694ee4,'e8',5) == 0x140b9fb80
print('PASS normal supply opener: local capture, native state transition/initializer, free items then stock before views; exact executable verified')
