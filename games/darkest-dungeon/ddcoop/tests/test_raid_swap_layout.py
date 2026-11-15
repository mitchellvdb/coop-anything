"""Read-only proof of normal inventory drop/swap and secondary-interface ABI."""
from pathlib import Path
import runpy
abi=runpy.run_path(str(Path(__file__).with_name('test_wave_layout.py')))
read,equal,rel=abi['read'],abi['equal'],abi['rel']
# Existing drop hook receives a secondary slot interface; native accesses its
# primary vtable at-10, inventory+18, slot+a8. Drag owns Item at+8, not a pointer.
equal(0x7eec38,'49 8d 57 08')
equal(0x7eec47,'48 8b 47 f0 48 8d 54 24 30 48 8d 4f f0')
equal(0x7eec88,'4c 8d 44 24 30 8b 97 a8 00 00 00 48 8b 4f 18')
assert rel(0x7eec97)==0x5df8f0 # native whole Item swap, modified notification
assert rel(0x7eecc7)==0x5df6a0 # distinct native merge path, not this whole-stack exchange
assert rel(0x7ed703)==0x5df8f0
assert rel(0x7ed74e)==0x5df6a0
# Copying back the displaced Item is part of native drag machinery. The shared
# path instead swaps both canonical models and suppresses stale source cleanup.
assert rel(0x7eecac)==0x3f5fc0
equal(0x7eecf1,'41 89 9f f0 05 00 00')
print('PASS normal raid stack exchange: slot primary/secondary roots, drag-owned Item, native swap versus merge and displaced-item copy')
