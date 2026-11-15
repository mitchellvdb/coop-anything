"""Read-only ABI proof for the shared town event recruit (DD_ACT_EVENT_RECRUIT), exact Steam EXE. Never executes game code.

The heroes a town event offers live in TownEvent::System at campaign+1808 (556520() + 0x1808): Hero entries at +38..+40,
0x15b0 each (+137c ID, +15a8 available), and an (ID, flag) list at +68, 8 bytes each. TownEventDisplay::OnEnterStart
(6d0130, vtable ec2020 slot 14) builds the screen's hero slots and the drop callback lambda_2 (vtable ec2138) with
captures +8 display (rsi) and +10 System (r13). Its call slot passes &captures (rcx+8) and the hero (rdx) to 6d0690,
which walks display+120..128 for an active slot (+58) whose item (+158) holds the hero at +28, then marks the entry
taken (+15a8 = 0, list flag +4 = 0) and sends HeroRecruited (6c54d0, event e79ae8). The roster drop (6b3ee0) adds a
hero that is not yet in the roster with Roster::AddEntry (589ab0) before that callback.
Usage: python tests/test_event_recruit_layout.py <path to supported Darkest.exe>
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


def rel(va, size, target):
    code = read(va, size)
    assert va + size + struct.unpack_from('<i', code, size - 4)[0] == target, hex(va)


# OnEnterStart: virtual slot 14 of TownEventDisplay; hooked prologue (5 bytes: push rbp/rbx/rsi/rdi), System = campaign+1808.
assert q64(0x140ec2020 + 14 * 8) == 0x1406d0130
assert read(0x1406d0130, 5).hex() == '4055535657'
assert read(0x1406d015d, 3).hex() == '488bf1'  # rsi = display
assert read(0x1406d0160, 1).hex() == 'e8'
rel(0x1406d0160, 5, 0x140556520)
assert read(0x1406d0168, 7).hex() == '4c8da808180000'  # r13 = campaign + 0x1808
# lambda_2: vtable ec2138, captures display (+8) and System (+10); its call slot hands &captures and the hero to 6d0690.
assert read(0x1406d02c2, 3).hex() == '488d05'
rel(0x1406d02c2, 7, 0x140ec2138)
assert read(0x1406d02c9, 12).hex() == '488945f0' '488975f8' '4c896d00'
assert q64(0x140ec2138 + 2 * 8) == 0x1406d2bb0
assert read(0x1406d2bb0, 7).hex() == '488b12' '4883c108'
assert read(0x1406d2bb7, 1).hex() == 'e9'
rel(0x1406d2bb7, 5, 0x1406d0690)
# 6d0690: r12 = hero, r14 = captures; slots display+120..128; active +58; item +158; item's hero +28 compared with r12.
assert read(0x1406d06b6, 6).hex() == '4c8be2' '4c8bf1'
assert read(0x1406d06be, 17).hex() == '498b06' '488bb820010000' '4c8bb828010000'
assert read(0x1406d06e3, 4).hex() == '837d5800'
assert read(0x1406d06ed, 7).hex() == '488b8558010000'
assert read(0x1406d06fd, 4).hex() == '488b7028'
assert read(0x1406d070a, 3).hex() == '4c3be6'
# System = captures+8: Hero entries +38..+40 (0x15b0, ID +137c, taken: +15a8 = 0) and the (ID, flag) list +68..+70.
assert read(0x1406d0720, 4).hex() == '498b5e08'
assert read(0x1406d072f, 8).hex() == '488b4340' '488b4b38'
assert read(0x1406d0740, 6).hex() == '39917c130000'
assert read(0x1406d0748, 7).hex() == 'c681a815000000'
assert read(0x1406d074f, 7).hex() == '4881c1b0150000'
assert read(0x1406d075b, 8).hex() == '488b4b70' '488b4368'
assert read(0x1406d076c, 8).hex() == 'c6400400' '4883c008'
# HeroRecruited (event e79ae8) through 6c54d0, as the Stagecoach's callback does.
rel(0x1406d0799, 7, 0x140e79ae8)
assert read(0x1406d07be, 1).hex() == 'e8'
rel(0x1406d07be, 5, 0x1406c54d0)
# The roster drop adds the hero with Roster::AddEntry(roster, hero) before the callback.
assert read(0x1406b4269, 6).hex() == '498bd7' '488bce'
rel(0x1406b426f, 5, 0x140589ab0)
print('PASS event recruit ABI: System campaign+1808 (+38 Hero entries 0x15b0, +137c ID, +15a8 taken, +68 list), OnEnterStart 6d0130 '
      'slot 14 builds lambda_2 (display, System) -> 6d0690 (active slot +58, item +158, hero +28), HeroRecruited 6c54d0, '
      'roster drop AddEntry 589ab0; no game execution')
