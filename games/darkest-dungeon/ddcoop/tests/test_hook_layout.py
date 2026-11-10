"""Read-only proof for every live hook, using VS dumpbin (no decoder package).
Run in an x64 Native Tools prompt, or pass --dumpbin <absolute tool path>.
No game process, DLL loading, game writes, save access or network calls.
"""
from pathlib import Path
import argparse, hashlib, re, shutil, struct, subprocess
p = argparse.ArgumentParser()
p.add_argument('exe', type=Path)
p.add_argument('--dumpbin', default=shutil.which('dumpbin'))
args = p.parse_args()
assert args.dumpbin, 'Use an x64 Native Tools prompt or --dumpbin'
root = Path(__file__).resolve().parents[1]
data = args.exe.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
pe = struct.unpack_from('<I', data, 0x3c)[0]
table = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', data, table + i * 40) for i in range(struct.unpack_from('<H', data, pe + 6)[0])]
text = next(s for s in sections if s[0].rstrip(b'\0') == b'.text')
code = data[text[4]:text[4] + text[3]]
def read(va, n):
    rva = va - 0x140000000
    assert text[2] <= rva and rva + n <= text[2] + text[3]
    return code[rva - text[2]:rva - text[2] + n]
addresses = {}
for name, va, pattern in re.findall(r'ADDR\((\w+),\s*0x([0-9a-f]+)ULL,\s*\{([^}]+)\}', (root/'src/addresses.inc').read_text(encoding='utf-8')):
    va = int(va, 16)
    needle = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]+)', pattern))
    assert len(needle) == 48 and read(va, 48) == needle and code.count(needle) == 1, name
    assert name not in addresses, (name, "duplicate native name")
    addresses[name] = va
source = (root/'runtime/dd_runtime.c').read_text(encoding='utf-8')
primary = ['activity_assign_hero','activity_slot_widget_ctor','buy_upgrade','wagon_buy','hero_recruit',
           'recruit_panel_show','recruit_panel_hide','roster_ui_drop','wagon_sell']
lengths = [int(x) for x in re.search(r'size_t sizes\[\] = \{([^}]+)', source)[1].split(',')]
assert len(primary) == len(lengths)
hooks = list(zip(primary, lengths))
for file in (root/'runtime').glob('*.inc'):
    hooks += [(name, int(n)) for name, n in re.findall(r'\{"([a-z0-9_]+)",\s*\(uintptr_t\)\w+,\s*&[^,]+,\s*(\d+)\}', file.read_text(encoding='utf-8'))]
assert len(hooks) > 50 and len({addresses[name] for name, n in hooks}) == len(hooks), 'Missing or duplicate live hook'
for name, n in hooks:
    start = addresses[name]
    result = subprocess.run([args.dumpbin, '/DISASM', f'/RANGE:0x{start:x},0x{start+n+32:x}', str(args.exe)], check=True, capture_output=True)
    rows = []
    for line in result.stdout.decode(errors='replace').splitlines():
        m = re.match(r'\s*([0-9A-F]{16}):\s+(?:[0-9A-F]{2} ?)+\s{2,}(\S.*)', line)
        if m: rows.append((int(m[1],16), m[2]))
    assert any(va == start + n for va, body in rows), (name, n, 'split instruction')
    copied = [(va, body) for va, body in rows if start <= va < start + n]
    assert copied and copied[0][0] == start, name
    for va, body in copied:
        mnemonic = body.split()[0].lower()
        assert not (mnemonic == 'call' or mnemonic.startswith(('j', 'loop')) or re.search(r'\[0[0-9A-Fa-f]{8,}h\]', body)), (name, hex(va), 'PC-relative copied instruction', body)
print(f'PASS hook ABI: {len(addresses)} uniquely named signatures/{len(set(addresses.values()))} distinct native addresses; {len(hooks)} distinct live/early hook targets, whole instructions and no PC-relative trampoline copies; pinned EXE, read-only')
