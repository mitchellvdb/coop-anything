"""Read-only proof for test76's two native assumptions in the pinned Darkest.exe (never launches or writes it).

1. Camp Leave (70b7c0) returns at once while camp overlay+0x24 is 0; the overlay's frame update (70a1e0) sets
   +0x24 to 1 only while no panel covers the camp and clears it otherwise; the fade's end lambda (70b850) sets
   camp part 7 (raid+0x5118). runtime/dd_items_bridge.inc waits for +0x24 before applying a shared Leave.
2. Campaign+0x58 is nextGuid: the persist.roster writer stores "nextGuid" from +0x58, new heroes take
   guid = [+0x58]++ (586373), and both listener-handle allocators (589e40, 589ef0) take [rcx+0x58]++ with rcx =
   Campaign::Current() (called from the roster panel 6b48d0). runtime/dd_weekly_bridge.inc shares that counter.

Usage: python -I tests/test_camp_guid_layout.py <Darkest.exe>
"""
import hashlib
import struct
import sys

GAME_SHA256 = 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'

CHECKS = [
    # camp
    (0x14070B7C7, '488b5108807a2400744a', 'Leave: rdx=[functor+8]; cmp byte [rdx+24h],0; je return'),
    (0x14070A27E, '41c644242401', 'overlay frame update sets +24 = 1 (camp opened)'),
    (0x14070A29E, '45887c2424', 'overlay frame update clears +24 while a panel covers the camp'),
    (0x14070A2C7, '41c644242401', 'overlay frame update sets +24 = 1 when uncovered'),
    (0x14070B864, 'c7801851000007000000c6420800', 'fade end lambda: raid+5118 = 7 (leaving), overlay+8 = 0'),
    # guid counter
    (0x1405854B2, '4d8d4658488d1573c59100', 'roster save writes "nextGuid" from +58h'),
    (0x140586373, '458b4c2458418d41014189442458', 'new hero: guid = [campaign+58h]++'),
    (0x140589E5C, '448b4158418d4001894158', 'listener allocator A: [rcx+58h]++'),
    (0x140589F0C, '448b4158418d4001894158', 'listener allocator B: [rcx+58h]++'),
    (0x1406B49B0, 'e86b1beaff', 'roster panel calls Campaign::Current()'),
    (0x1406B49D9, '488bc8e85f54edff', 'roster panel passes that campaign to allocator A'),
]
STRINGS = [(0x140EA1A30, b'nextGuid\0')]
CALLS = [(0x1406B49B0, 0x140556520), (0x1406B49DC, 0x140589E40)]


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
    for address, target in CALLS:
        code = at(address, 5)
        assert code[0] == 0xE8 and address + 5 + struct.unpack_from('<i', code, 1)[0] == target, hex(address)
    print('PASS camp leave/active flag and campaign nextGuid layout: %d byte checks, %d calls, pinned EXE, read-only'
          % (len(CHECKS), len(CALLS)))


if __name__ == '__main__':
    main()
