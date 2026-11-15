"""Read-only ABI proof for the shared Sanitarium treatment choice (DD_ACT_TREATMENT), exact Steam EXE. Never executes game code.

QuirkTreatmentActivityDisplay's click (6a3910, slot 2 of the DrawQuirkTreatmentData lambda_1 functor vtable ebe9d0) reads
functor +8 activity, +10 group, +14 slot, +18 entry. It finds the record at activity+190 + slot*1e0 + group*a0 and the
entry at record+0 + entry*88. A new name (compare of record+18 with the entry, 0x80 bytes) is copied and gets state 1 when
entry+81 is set, otherwise 538580(entry, 1). The same name steps entry+84 (3 wraps to 0) and, when that step is not
allowed, goes through 538580; state 0 clears record+18. The state is written to entry+84 and record+98.
runtime/dd_treatment_bridge.inc (treatment_result, treatment_next) mirrors this. The Sanitarium builds both of its
activities with the QuirkTreatment ctor 538690 (vtable e9b468): the quirk one sets +23c/+23d, the disease one sets +23e.
Usage: python tests/test_treatment_layout.py <path to supported Darkest.exe>
"""
from pathlib import Path
import hashlib
import struct
import sys

b = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
base = 0x140000000
pe = struct.unpack_from('<I', b, 60)[0]
q = pe + 24 + struct.unpack_from('<H', b, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', b, q + 40 * i) for i in range(struct.unpack_from('<H', b, pe + 6)[0])]


def read(va, size):
    for _, _, rva, n, raw in sections:
        if rva <= va - base and va - base + size <= rva + n:
            return b[raw + va - base - rva:raw + va - base - rva + size]
    raise AssertionError(hex(va))


def q64(va):
    return struct.unpack('<Q', read(va, 8))[0]


def call(va, target):
    code = read(va, 5)
    assert code[0] == 0xe8, hex(va)
    assert va + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(va)


def lea(va, target):
    code = read(va, 7)
    assert code[:3] in (bytes.fromhex('488d05'), bytes.fromhex('4c8d05')), hex(va)
    assert va + 7 + struct.unpack_from('<i', code, 3)[0] == target, hex(va)


# The functor: its vtable's call slot is 6a3910, and the display builds it with that vtable.
assert q64(0x140ebe9d0 + 2 * 8) == 0x1406a3910
lea(0x1406a2ae8, 0x140ebe9d0)
# Hooked prologue (10 bytes, as src/addresses.inc): push rsi; sub rsp,20; mov rax,[rcx+8] (activity).
assert read(0x1406a3910, 10).hex() == '405648' '83ec20' '488b4108'
# Slot +14 against the record count (activity+190..198, 0x1e0 each: the 1/0x1e0 multiplier after sar 5).
assert read(0x1406a391d, 4).hex() == '448b4914'
assert read(0x1406a3921, 14).hex() == '488bb090010000' '488b9098010000'
assert read(0x1406a392f, 10).hex() == '48b8efeeeeeeeeeeeeee'
assert read(0x1406a393c, 4).hex() == '48c1fa05'
# Group +10, entry +18; record = (slot*3 + group) * 0xa0; entries at record+0, 0x88 each (1/0x88 after sar 3).
assert read(0x1406a394d, 4).hex() == '48634910' and read(0x1406a3951, 4).hex() == '4b8d0449'
assert read(0x1406a3955, 4).hex() == '418b5018'
assert read(0x1406a396b, 8).hex() == '4c8d3489' '49c1e605'
assert read(0x1406a3961, 10).hex() == '49b8f1f0f0f0f0f0f0f0'
assert read(0x1406a397f, 4).hex() == '48c1f803'
assert read(0x1406a399b, 7).hex() == '4869da88000000'
# Name compare: record+18 against the entry, 0x80 bytes; equal -> toggle, different -> copy.
assert read(0x1406a3995, 6).hex() == '41b880000000'
assert read(0x1406a39ae, 4).hex() == '488d4f18'
assert read(0x1406a39bc, 2).hex() == '7579'  # jne 6a3a37 (new name)
# Toggle: entry+84 (3 -> 0, else +1), flags +81/+82, else 538580; state 0 clears record+18 with "".
assert read(0x1406a39be, 6).hex() == '8b8384000000'
assert read(0x1406a39c6, 9).hex() == '83f803' '8d4801' '0f45d1'
assert read(0x1406a39df, 7).hex() == '0fb68382000000' and read(0x1406a39e8, 7).hex() == '0fb68381000000'
call(0x1406a39f6, 0x140538580)
assert read(0x1406a39fd, 6).hex() == '899384000000' and read(0x1406a3a0c, 6).hex() == '899384000000'
lea(0x1406a3a19, 0x140e39238)
assert read(0x140e39238, 1) == b'\0'
assert read(0x1406a3a20, 9).hex() == 'ba80000000' '488d4f18'
# New name: 0x80 bytes copied to record+18, state 1 if entry+81, else 538580(entry, 1).
assert read(0x1406a3a37, 8).hex() == '0f1003' 'ba01000000'
assert read(0x1406a3a7e, 9).hex() == '80bb8100000000' '750a'
call(0x1406a3a8a, 0x140538580)
assert read(0x1406a3a91, 6).hex() == '899384000000'
# The result goes to record+98.
assert read(0x1406a3aa1, 8).hex() == '4189943698000000'
# 538580(entry, state): next = state==3 ? 0 : state+1; 0 ends; 1 needs +81, 2 needs +82; otherwise loops from next.
assert read(0x140538580, 13).hex() == '4533c0' '8d4201' '83fa03' '440f45c0'
assert read(0x14053859f, 7).hex() == '0fb68182000000' and read(0x1405385a8, 7).hex() == '0fb68181000000'
assert read(0x1405385b3, 8).hex() == '418bd0' 'e9c5ffffff'  # jmp 538580 with edx = next
# Both Sanitarium activities are QuirkTreatment (ctor 538690 sets vtable e9b468); +23e marks the disease one.
lea(0x1405386d7, 0x140e9b468)
call(0x14054f8ba, 0x140538690)
assert read(0x14054f8c0, 9).hex() == '66c7803c0200000101'
call(0x14054f93d, 0x140538690)
assert read(0x14054f943, 7).hex() == 'c6803e02000001'
# The draft list builder receives (activity,slot,HeroID), independently of the
# still-empty paid slot. +238 stores only the last previewed hero globally.
assert q64(0x140e9b468+0x38)==0x140538830
assert read(0x140538830,12).hex()=='4053555741564883ec288bea'
assert read(0x14053883c,6).hex()=='418bd8488bf9' # r8d HeroID, rcx activity
assert read(0x140538847,6).hex()=='899938020000'
assert read(0x1405388a8,8).hex()=='3bd80f84b0000000' # equal committed -> no rebuild
assert read(0x140538948,5).hex()=='e9e3110000' # normal hero's lists539b30
print('PASS treatment ABI: 6a3910 functor +8/+10/+14/+18, record activity+190 + slot*1e0 + group*a0 (+18 name, +98 state), '
      'entries 0x88 (+81/+82 flags, +84 state), new name copied / same name stepped via 538580, state 0 clears; '
      'both Sanitarium activities are QuirkTreatment with +23e on the disease one; per-slot preview538830/last hero238 and12-byte hook; no game execution')
