"""Read-only proof separating automatic weekly assignment from UI acceptance.
No native execution, process launch, save access, installed writes or network.
"""
from pathlib import Path
import argparse
import hashlib
import struct

p = argparse.ArgumentParser()
p.add_argument('exe', type=Path)
args = p.parse_args()
data = args.exe.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe + 4] == b'PE\0\0'
optional = pe + 24
assert struct.unpack_from('<H', data, optional)[0] == 0x20b
table = optional + struct.unpack_from('<H', data, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', data, table + i * 40)
            for i in range(struct.unpack_from('<H', data, pe + 6)[0])]


def read(rva, n):
    for _, _, start, size, offset in sections:
        if start <= rva and rva + n <= start + size:
            return data[offset + rva - start:offset + rva - start + n]
    raise AssertionError(hex(rva))


def exact(rva, expected):
    expected = bytes.fromhex(expected)
    assert read(rva, len(expected)) == expected, hex(rva)


def call(rva, target):
    code = read(rva, 5)
    assert code[0] == 0xe8, hex(rva)
    assert rva + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(rva)
    return rva + 5


# The automatic caller selects Activity.slot and reads the stable Hero ID,
# then uses the same AssignHero entrypoint as the user accept callback.
# Skipping this call omits a native assignment while its caller continues
# to mark the slot record; suppressing the notice alone cannot fix that.
exact(0x53cbd4, '45 89 a6 90 00 00 00')       # Activity+90 = selected slot
exact(0x53cbdb, '45 8b 85 7c 13 00 00')       # R8D = Hero+137c
exact(0x53cbe2, '48 8d 54 24 40')             # result POD
exact(0x53cbe7, '49 8b ce')                   # RCX = Activity
assert call(0x53cbea, 0x5360c0) == 0x53cbef   # exact allowed native return
exact(0x53cbef, '90')
exact(0x53cbf0, '48 8d 4c 24 48')
call(0x53cbf5, 0x233de0)                     # result cleanup, no accept-bool test
exact(0x53cbfa, '49 8b 56 78')                # canonical slots begin
exact(0x53cbfe, '49 8b 86 80 00 00 00')       # canonical slots end
exact(0x53cc05, '48 2b c2 48 c1 f8 03')       # span / 8
exact(0x53cc0c, '49 b8 ab aa aa aa aa aa aa aa')
exact(0x53cc16, '49 0f af c0')                # span / 24 slot records
exact(0x53cc1a, '4c 3b e0 73 13')             # selected slot bounds check
exact(0x53cc1f, '4b 8d 04 64 48 8d 0c c2')   # begin + selected slot * 24
exact(0x53cc27, '48 85 c9 74 15')             # NULL check
exact(0x53cc2c, 'c6 41 14 01')                # slot record flag+14 = 1

# The UI callback has a different exact return address. It must retain the
# shared reservation/ordered-accept path, including stale-draft rejection.
exact(0x68b0b1, '48 8b 51 20 8b 41 18 89 82 90 00 00 00')
exact(0x68b0cf, '4c 8b 41 08 45 8b 80 7c 13 00 00')
exact(0x68b0da, '48 8d 55 f0 48 8b 49 20')
assert call(0x68b0e2, 0x5360c0) == 0x68b0e7
exact(0x68b0e7, '90 80 7d f0 00')             # UI checks accepted bool

# Inventory direct E8 relative encodings only; this intentionally makes no
# claim about indirect calls. Both proved sites are the only such encodings
# targeting AssignHero anywhere in the pinned .text section.
text = next(s for s in sections if s[0].rstrip(b'\0') == b'.text')
code = data[text[4]:text[4] + text[3]]
direct = []
for i in range(len(code) - 4):
    if code[i] == 0xe8 and text[2] + i + 5 + struct.unpack_from('<i', code, i + 1)[0] == 0x5360c0:
        direct.append(text[2] + i)
assert direct == [0x53cbea, 0x68b0e2], direct

# The exact 48-byte callsite neighborhood is suitable for an additional
# version/signature pin; it is not a new hook or trampoline entrypoint.
assert code.count(read(0x53cbd4, 48)) == 1

print('PASS weekly activity ABI: exact native/UI assignment callers, stable Hero ID, '
      'slot selection and post-call flag; two direct calls, unique native pin; pinned EXE, read-only')
