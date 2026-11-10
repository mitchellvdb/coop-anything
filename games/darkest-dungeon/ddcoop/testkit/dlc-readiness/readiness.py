"""Read-only campaign-DLC inventory and report comparison, never activation.

readiness.py GAME_ROOT [--enabled FEATURE ...]
readiness.py --compare HOST.json GUEST.json [--enabled FEATURE ...]
Fingerprints are data evidence, not ownership or live co-op compatibility.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import stat
import struct

KNOWN_EXE = 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
FEATURES = {
    'musketeer': ('445700_musketeer', '', ['hero skills/upgrades/ownership', 'camping']),
    'flagellant': ('580100_crimson_court', 'features/flagellant', ['hero skills/upgrades/ownership', 'special hero effects']),
    'districts': ('580100_crimson_court', 'features/districts', ['district purchases', 'shared costs', 'weekly district effects']),
    'crimson_court': ('580100_crimson_court', 'features/crimson_court', ['blood/curse/invitations', 'Courtyard departure/return', 'persistent quest progress']),
    'shieldbreaker': ('702540_shieldbreaker', '', ['hero skills/upgrades/ownership', 'camping/nightmare transitions']),
    'color_of_madness': ('735730_color_of_madness', '', ['shard currencies/shop', 'Farmstead departure', 'wave/reward/retreat transitions']),
    'fires_edge': ('4964110_fires_edge', 'features/fires_edge', ['new districts/shared costs', 'weekly district effects']),
    'runaway': ('4964110_fires_edge', 'features/runaway', ['hero skills/upgrades/ownership', 'camping', 'burn/stealth effects']),
    'duelist': ('4964110_fires_edge', 'features/duelist', ['hero skills/upgrades/ownership', 'camping', 'stance/counter effects']),
}
HEROES = {'musketeer', 'flagellant', 'shieldbreaker', 'runaway', 'duelist'}
DATA_SUFFIXES = {'.json', '.darkest', '.xml', '.csv', '.txt', '.loc'}
WITNESSES = {
    'districts': ('campaign/town/districts/districts_districts.json',),
    'crimson_court': ('campaign/quest/crimson_court.quest.plot_quests.json', 'campaign/roster/roster.contagion.json'),
    'color_of_madness': ('campaign/quest/com.quest.plot_quests.json', 'campaign/provision/com.provision.json'),
    'fires_edge': ('campaign/town/districts/runaway_duelist.districts.json',),
}
for _hero in HEROES:
    _camp = 'crimson_court' if _hero == 'flagellant' else _hero
    WITNESSES[_hero] = ('heroes/' + _hero + '/' + _hero + '.info.darkest',
                        'upgrades/heroes/' + _hero + '.upgrades.json') + (() if _hero == 'musketeer' else
                        ('raid/camping/' + _camp + '.camping_skills.json',))
BASE_WITNESSES = {'musketeer': ('raid/camping/default.camping_skills.json',)}


def _linked(path):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, 'st_file_attributes', 0) & 0x400)


def _safe_path(root, relative):
    root = root.resolve()
    path = root
    for part in Path(relative).parts:
        if part in {'..', '.', ''}:
            raise ValueError('Invalid relative data path')
        path /= part
        if _linked(path) or not path.resolve().is_relative_to(root):
            raise ValueError('DLC data link leaves or redirects its data directory')
    return path


def _data_files(directory, skip_features=False):
    directory = Path(directory)
    if _linked(directory):
        raise ValueError('DLC data root is a link')
    root = directory.resolve()
    def visit(here):
        for path in sorted(here.iterdir()):
            if skip_features and path.parent == root and path.name == 'features':
                continue
            if _linked(path) or not path.resolve().is_relative_to(root):
                raise ValueError('DLC data link leaves or redirects its feature directory')
            if path.is_dir():
                yield from visit(path)
            elif path.is_file() and path.suffix.lower() in DATA_SUFFIXES:
                yield path.relative_to(root).as_posix(), path.read_bytes()
    yield from visit(root)


def _digest(files):
    digest = hashlib.sha256()
    for name, data in sorted(files):
        name = name.encode('utf-8')
        digest.update(struct.pack('<I', len(name)) + name + struct.pack('<Q', len(data)) + data)
    return digest.hexdigest() if files else None


def fingerprint(directory):
    files = list(_data_files(Path(directory)))
    return _digest(files), len(files)


def _hero_data(info, camp_files, hero):
    skills, levels, camping = set(), set(), set()
    for line in info.decode('utf-8-sig', errors='replace').splitlines():
        if not line.lstrip().startswith('combat_skill:'):
            continue
        identity = re.search(r'\.id\s+"([^"]+)"', line)
        level = re.search(r'\.level\s+(\d+)', line)
        if identity and level:
            skills.add(identity.group(1)); levels.add(int(level.group(1)))
    for data in camp_files:
        try:
            parsed = json.loads(data.decode('utf-8-sig'))
        except (UnicodeError, ValueError):
            continue
        entries = parsed.get('skills', []) if isinstance(parsed, dict) else []
        for skill in entries if isinstance(entries, list) else []:
            classes = skill.get('hero_classes') if isinstance(skill, dict) else None
            if isinstance(classes, list) and hero in classes and isinstance(skill.get('id'), str):
                camping.add(skill['id'])
    return {'combat_skill_count': len(skills), 'levels': sorted(levels),
            'camping_skill_count': len(camping),
            'fits_existing_skill_codec': 0 < len(skills) <= 32 and bool(levels) and max(levels) <= 4 and 0 < len(camping) <= 32,
            'live_hero_support_verified': False}


def inspect(game_root, enabled=None):
    root = Path(game_root).resolve()
    chosen = sorted(FEATURES if enabled is None else set(enabled))
    if set(chosen) - FEATURES.keys():
        raise ValueError('Unknown DLC feature; arena_mp is excluded')
    exe = _safe_path(root, '_windows/win64/Darkest.exe')
    game_hash = hashlib.sha256(exe.read_bytes()).hexdigest() if exe.is_file() else None
    result = {'schema': 2, 'known_game_build': game_hash == KNOWN_EXE,
              'game_sha256': game_hash, 'live_dlc_support_verified': False, 'features': []}
    common_cache = {}
    for name in chosen:
        package, subdir, cases = FEATURES[name]
        relative = Path('dlc') / package / subdir
        directory = _safe_path(root, relative)
        own = dict(_data_files(directory)) if directory.is_dir() else {}
        common = {}
        if subdir:
            if package not in common_cache:
                parent = _safe_path(root, Path('dlc') / package)
                common_cache[package] = dict(_data_files(parent, skip_features=True)) if parent.is_dir() else {}
            common = common_cache[package]
        base = {}
        for witness in BASE_WITNESSES.get(name, ()):
            path = _safe_path(root, witness)
            if path.is_file():
                base[witness] = path.read_bytes()
        missing = [p for p in WITNESSES[name] if not own.get(p)]
        missing += ['base/' + p for p in BASE_WITNESSES.get(name, ()) if not base.get(p)]
        files = [('feature/' + p, d) for p, d in own.items()]
        files += [('common/' + p, d) for p, d in common.items()]
        files += [('base/' + p, d) for p, d in base.items()]
        present = bool(own) and not missing
        profile = {'feature': name, 'relative_root': relative.as_posix(),
                   'data_present': present, 'data_files': len(own), 'shared_data_files': len(common) + len(base),
                   'data_sha256': _digest(files), 'missing_witnesses': missing,
                   'status': 'needs_live_validation' if present else 'missing_feature_data', 'test_groups': cases}
        if name in HEROES:
            info = own.get('heroes/' + name + '/' + name + '.info.darkest', b'')
            camps = [d for p, d in list(own.items()) + list(base.items()) if p.endswith('.camping_skills.json')]
            profile['hero_data'] = _hero_data(info, camps, name)
        result['features'].append(profile)
    return result


def compare(reference, candidate, required=None):
    """Pure report comparison. Matching data never means live DLC support or entitlement."""
    errors, unknown = [], set()
    def profiles(report, label):
        if not isinstance(report, dict) or report.get('schema') != 2 or not isinstance(report.get('features'), list):
            errors.append(label + ': invalid report schema'); return {}
        found = {}
        for entry in report['features']:
            if not isinstance(entry, dict) or not isinstance(entry.get('feature'), str):
                errors.append(label + ': invalid feature entry'); continue
            name = entry['feature']
            if name not in FEATURES:
                unknown.add(name); continue
            if name in found:
                errors.append(label + ': duplicate feature ' + name); continue
            if (entry.get('data_sha256') is not None and
                not (isinstance(entry['data_sha256'], str) and re.fullmatch('[0-9a-f]{64}', entry['data_sha256']))):
                errors.append(label + ': invalid fingerprint ' + name); continue
            found[name] = entry
        return found
    first, second = profiles(reference, 'reference'), profiles(candidate, 'candidate')
    chosen = set(first) if required is None else set(required)
    if not all(isinstance(n, str) for n in chosen):
        raise ValueError('Required features must be strings')
    unknown.update(chosen - FEATURES.keys())
    chosen &= FEATURES.keys()
    def complete(entry):
        return entry and entry.get('data_present') is True and entry.get('data_sha256') and entry.get('missing_witnesses') == []
    missing_reference = sorted(n for n in chosen if not complete(first.get(n)))
    missing = sorted(n for n in chosen if not complete(second.get(n)))
    changed = sorted(n for n in chosen if complete(first.get(n)) and complete(second.get(n)) and
                     first[n]['data_sha256'] != second[n]['data_sha256'])
    a = reference.get('game_sha256') if isinstance(reference, dict) else None
    b = candidate.get('game_sha256') if isinstance(candidate, dict) else None
    build_match = isinstance(a, str) and bool(re.fullmatch('[0-9a-f]{64}', a)) and a == b
    known = build_match and a == KNOWN_EXE
    return {'schema': 1, 'required_features': sorted(chosen), 'build_match': build_match,
            'known_game_build': known, 'missing_reference_features': missing_reference,
            'missing_features': missing, 'unknown_features': sorted(unknown), 'changed_features': changed,
            'errors': errors, 'data_match': bool(known and not (missing_reference or missing or changed or unknown or errors)),
            'live_dlc_support_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_root', nargs='?')
    parser.add_argument('--enabled', nargs='+', choices=sorted(FEATURES))
    parser.add_argument('--compare', nargs=2, metavar=('HOST_JSON', 'GUEST_JSON'))
    args = parser.parse_args()
    if bool(args.game_root) == bool(args.compare):
        parser.error('Use GAME_ROOT or --compare HOST.json GUEST.json')
    try:
        if args.compare:
            reports = []
            for name in args.compare:
                path = Path(name)
                if path.stat().st_size > 2 * 1024 * 1024:
                    raise ValueError('Report exceeds 2 MiB')
                reports.append(json.loads(path.read_text(encoding='utf-8-sig')))
            result = compare(*reports, required=args.enabled)
        else:
            result = inspect(args.game_root, args.enabled)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
