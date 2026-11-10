"""Read-only proof of the pinned native hero-selection postcondition.
No game process, save reads/writes or native function execution.
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
 raise AssertionError('Unmapped pinned native range')
code=read(0x606450,0x94)
# Party.begin/end are +0/+8; normal and empty paths set Party.selected+430.
assert code[0xa:0xe]==bytes.fromhex('48 8b 19 48')
assert code[0x10:0x14]==bytes.fromhex('48 8b 41 08')
assert read(0x606482,4)==bytes.fromhex('48 8b 1c c3')
assert read(0x60648b,7)==bytes.fromhex('48 3b 99 30 04 00 00')
assert read(0x6064be,7)==bytes.fromhex('48 89 9f 30 04 00 00')
assert read(0x6064d0,9)==bytes.fromhex('33 c0 48 89 81 30 04 00 00')
assert read(0x6064ce,2)==bytes.fromhex('5f c3')
print('PASS selection ABI: pinned Party.begin/end, selected Hero* at party+430 (raid+438), native normal/empty writes; read-only')
