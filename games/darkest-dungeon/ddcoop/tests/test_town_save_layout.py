"""Read-only proof for test79's town saves in the pinned Darkest.exe (never launches or writes it).

After a stress-building placement (accept lambda 68b080: assign 5360c0 at 68b0e2) and after the red X
(68c320: SetHero through its vtable, returning at 68c373), the game marks the hero's roster entry
(+162c = 1), then calls 585a60 (writes persist.roster.json), 591540 (writes persist.town.json) and
3d3110 (the raid files). The character sheet's skill toggle lambdas end the same way (805208), with
585a60, the wallet notification 567990 and 3d3110, but without the town writer. The assign itself calls
567990 (5361df), which is why the gold was saved while the town file kept the slot empty.
runtime/dd_activity_bridge.inc and runtime/dd_hamlet_bridge.inc now call the same writers after a
shared placement, removal or skill toggle.

Usage: python -I tests/test_town_save_layout.py <Darkest.exe>
"""
import hashlib
import struct
import sys

GAME_SHA256 = 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'

CHECKS = [
    (0x14068B2F8, 'c6812c16000001', 'accept: roster entry +162c = 1'),
    (0x14068C3C8, 'c6812c16000001', 'red X: roster entry +162c = 1'),
    (0x140805208, 'c6812c16000001', 'skill toggle: roster entry +162c = 1'),
    (0x14068B328, '488d8838110000', 'accept: rcx = campaign+1138 for the town writer'),
    (0x14068C3F9, '488d8838110000', 'red X: rcx = campaign+1138 for the town writer'),
    (0x14068B334, '488d0d351c3e02', 'accept: rcx = image+2a6cf70 for the raid writer'),
]
STRINGS = [(0x140EA2238, b'persist.town.json\0'), (0x140EA1A68, b'persist.roster.json\0')]
# (call site, target)
CALLS = [
    (0x14068B0E2, 0x1405360C0),  # accept: assign
    (0x14068B31E, 0x140585A60),  # accept: roster writer
    (0x14068B32F, 0x140591540),  # accept: town writer
    (0x14068B33B, 0x1403D3110),  # accept: raid writer
    (0x14068C3EF, 0x140585A60),  # red X: roster writer
    (0x14068C400, 0x140591540),  # red X: town writer
    (0x1408045EC, 0x1405D4F70),  # sheet: combat toggle
    (0x1408040C3, 0x1405D3DD0),  # sheet: camping toggle
    (0x14080522F, 0x140585A60),  # sheet: roster writer
    (0x140805240, 0x140567990),  # sheet: wallet notification
    (0x1405361DF, 0x140567990),  # assign notifies the wallet itself
]
JUMPS = [(0x14068C411, 0x1403D3110), (0x140805255, 0x1403D3110)]  # red X and sheet: raid writer (tail jump)
# The writers name their files: lea rdx/rsi, [rip+...] to the strings above.
LEAS = [(0x1405915A0, 0x140EA2238), (0x140585AD6, 0x140EA1A68)]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    data = open(sys.argv[1], 'rb').read()
    assert hashlib.sha256(data).hexdigest() == GAME_SHA256, 'not the pinned Darkest.exe build'
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<Q', data, pe + 48)[0]
    sections = []
    for i in range(count):
        o = pe + 24 + optional + 40 * i
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
        sections.append((va, max(vsize, rsize), raw, rsize))

    def at(address, length):
        rva = address - base
        for va, size, raw, rsize in sections:
            if va <= rva and rva + length <= va + rsize:
                return data[raw + rva - va: raw + rva - va + length]
        raise AssertionError(hex(address))

    for address, expected, what in CHECKS:
        got = at(address, len(expected) // 2).hex()
        assert got == expected, f'{hex(address)} {what}: {got} != {expected}'
    for address, text in STRINGS:
        assert at(address, len(text)) == text, hex(address)
    for opcode, table in ((0xE8, CALLS), (0xE9, JUMPS)):
        for address, target in table:
            code = at(address, 5)
            assert code[0] == opcode and address + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(address)
    for address, target in LEAS:
        code = at(address, 7)
        assert code[0] == 0x48 and code[1] == 0x8D, hex(address)
        assert address + 7 + struct.unpack_from('<i', code, 3)[0] == target, hex(address)
    print('PASS town/roster writers after placement, red X and skill toggle: %d byte checks, %d calls, %d jumps, '
          '%d file names, pinned EXE, read-only' % (len(CHECKS), len(CALLS), len(JUMPS), len(LEAS)))


if __name__ == '__main__':
    main()
