"""Read-only native raid-finish/week-order proof for the supported executable.
This proves code layout and call order, not complete deterministic gameplay.
"""
from pathlib import Path
import hashlib
import struct
import sys

D = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(D).hexdigest() == "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12"
BASE = 0x140000000
pe = struct.unpack_from("<I", D, 0x3c)[0]
table = pe + 24 + struct.unpack_from("<H", D, pe + 20)[0]
sections = [struct.unpack_from("<8sIIII", D, table + i * 40)
            for i in range(struct.unpack_from("<H", D, pe + 6)[0])]


def read(rva, size):
    for _, _, start, length, raw in sections:
        if start <= rva and rva + size <= start + length:
            return D[raw + rva - start:raw + rva - start + size]
    raise AssertionError(hex(rva))


def exact(rva, value):
    assert read(rva, len(bytes.fromhex(value))) == bytes.fromhex(value), hex(rva)


def call(rva, target):
    exact(rva, "e8")
    assert rva + 5 + struct.unpack("<i", read(rva + 1, 4))[0] == target


def relative(rva, prefix, target):
    code = bytes.fromhex(prefix)
    exact(rva, prefix)
    assert rva + len(code) + 4 + struct.unpack("<i", read(rva + len(code), 4))[0] == target


def rtti(vtable):
    col = struct.unpack("<Q", read(vtable - 8, 8))[0] - BASE
    desc = struct.unpack("<I", read(col + 12, 4))[0]
    return read(desc + 16, 128).split(b"\0", 1)[0]


def entry(vtable, offset, target):
    assert struct.unpack("<Q", read(vtable + offset, 8))[0] == BASE + target


# The native main update invokes RaidFinish and then TownVisitStart without
# returning to the frame loop in between. The strings identify the normal path.
assert read(0xe749f8, 11) == b"RaidFinish\0"
assert read(0xe74a08, 15) == b"TownVisitStart\0"
assert read(0xe73ce8, 20) == b"Darkest::RaidFinish\0"
relative(0x3e249e, "4c8d0d", 0xe749f8)
exact(0x3e24be, "488bce")
call(0x3e24c1, 0x3d8da0)
relative(0x3e24c6, "4c8d0d", 0xe74a08)
exact(0x3e24e6, "488b05")
exact(0x3e24ed, "0fb688f1020000")
call(0x3e24fb, 0x556fb0)
call(0x3e2523, 0x3d3110)  # save follows native finish and town-week processing
relative(0x3d8de6, "488d0d", 0xe73ce8)
# All pushes, no RIP-relative instructions: this 13-byte prologue can be copied
# by the existing rel32 trampoline strategy if a later change adds that hook.
exact(0x3d8da0, "40555356574154415541564157")

# TownVisitStart receives its one boolean in CL, then calls the campaign Town.
exact(0x556fd3, "0fb6e9")
call(0x557274, 0x556520)
exact(0x557279, "488d8838110000")
exact(0x557280, "400fb6d5")
call(0x557284, 0x591840)
# Town dispatches Building::VisitStart(bool) through virtual slot +0x28.
exact(0x591970, "488b0b488b01400fb6d6ff5028")
assert b"StageCoach@Building" in rtti(0xe9d088)
entry(0xe9d088, 0x28, 0x545630)
# Building dispatches its stores' weekly generation, including HeroRecruit.
exact(0x545653, "488b3b488bcf488b07ff5030488b07488bcfff5028")
assert b"HeroRecruit@Store@Building" in rtti(0xe9cfd0)
entry(0xe9cfd0, 0x28, 0x550b90)
# Stagecoach class selection uses the same named Rules instance bound by the
# runtime, not Battle or another independent stream at this callsite.
relative(0x551240, "488d0d", 0x2add250)
relative(0x551247, "488b05", 0x2add250)
exact(0x55124e, "ff5018")
# The native minstd step reads/writes state+8, multiplier16807, modulus2^31-1.
# Its valid nonzero states are 1..2147483646, the admitted seed range.
exact(0x28fcfd, "8b4908")
exact(0x28fd13, "4469c1a7410000")
exact(0x28fd1a, "69c2ffffff7f")
exact(0x28fd23, "44894608")
exact(0x28fd29, "4181c0ffffff7f44894608")
# Existing post-results synchronization occurs after this processing in live
# logs; the candidate generation is not first entered by results Next.
text = next(s for s in sections if s[0].rstrip(b"\0") == b".text")
code = D[text[4]:text[4] + text[3]]
assert code.count(read(0x3d8da0, 48)) == 1
assert code.count(read(0x3e24be, 48)) == 1
print("PASS raid-week ABI: exact native RaidFinish before TownVisitStart, "
      "canonical Stagecoach/HeroRecruit virtual dispatch, pre-results generation "
      "and safe entry/callsite neighborhoods; pinned EXE, read-only")
