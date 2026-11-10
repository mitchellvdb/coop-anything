"""Isolated synthetic DLC data tests: no game launch, saves, Steam or activation."""
from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

path = Path(__file__).resolve().parents[1] / 'testkit/dlc-readiness/readiness.py'
spec = importlib.util.spec_from_file_location('readiness', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ddcoop-dlc-fixture-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'game'
        self.root.mkdir()

    def put(self, relative, data):
        f = self.root / relative
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_bytes(data if isinstance(data, bytes) else data.encode('utf-8'))
        return f

    def directory(self, name):
        package, subdir, _ = module.FEATURES[name]
        return self.root / 'dlc' / package / subdir

    def populate(self):
        self.put('_windows/win64/Darkest.exe', b'FAKE-EXE-FOR-SYNTHETIC-REPORT')
        for name in module.FEATURES:
            directory = self.directory(name)
            for witness in module.WITNESSES[name]:
                data = '{}'
                if witness.endswith('.info.darkest'):
                    data = '\n'.join(f'combat_skill: .id "skill{i}" .level {level}' for i in range(7) for level in range(5))
                elif witness.endswith('.camping_skills.json'):
                    data = json.dumps({'skills': [{'id': 'camp' + str(i), 'hero_classes': [name]} for i in range(7)]})
                self.put((directory / witness).relative_to(self.root), data)
        self.put('raid/camping/default.camping_skills.json', json.dumps({'skills': [{'id': 'shared', 'hero_classes': ['musketeer']}]}))
        for package in ['580100_crimson_court', '4964110_fires_edge']:
            self.put('dlc/' + package + '/shared/common.json', '{"shared":1}')
        self.put('dlc/1117860_arena_mp/data.json', 'EXCLUDED-ARENA-DATA')

    def by_name(self, report):
        return {f['feature']: f for f in report['features']}

    def known_report(self):
        report = module.inspect(self.root)
        # Model a matching supported EXE without storing/copying a genuine game executable.
        report['game_sha256'] = module.KNOWN_EXE
        report['known_game_build'] = True
        return report

    def snapshot(self):
        return {p.relative_to(self.root).as_posix(): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}

    def test_absent(self):
        result = module.inspect(self.root)
        self.assertEqual(result['schema'], 2)
        self.assertEqual(len(result['features']), 9)
        self.assertFalse(result['known_game_build'])
        self.assertTrue(all(not f['data_present'] and f['missing_witnesses'] for f in result['features']))
        self.assertFalse(result['live_dlc_support_verified'])

    def test_nine_features_stable_read_only(self):
        self.populate(); before = self.snapshot()
        first = module.inspect(self.root)
        self.assertEqual(first, module.inspect(self.root))
        self.assertEqual(before, self.snapshot())
        self.assertEqual(set(self.by_name(first)), set(module.FEATURES))
        self.assertTrue(all(f['data_present'] and len(f['data_sha256']) == 64 for f in first['features']))
        self.assertTrue(all(f['hero_data']['fits_existing_skill_codec'] for f in first['features'] if f['feature'] in module.HEROES))
        self.assertTrue(all(not f['hero_data']['live_hero_support_verified'] for f in first['features'] if f['feature'] in module.HEROES))

    def test_each_required_witness_missing_or_empty(self):
        self.populate()
        for name, witnesses in module.WITNESSES.items():
            for witness in witnesses:
                path = self.directory(name) / witness; saved = path.read_bytes()
                path.write_bytes(b'')
                result = module.inspect(self.root, [name])['features'][0]
                self.assertFalse(result['data_present'], (name, witness))
                self.assertIn(witness, result['missing_witnesses'])
                path.unlink()
                self.assertFalse(module.inspect(self.root, [name])['features'][0]['data_present'])
                path.write_bytes(saved)

    def test_package_or_unrelated_file_is_not_feature(self):
        self.put('dlc/580100_crimson_court/features/flagellant/unrelated.json', '{}')
        for name in ['flagellant', 'districts', 'crimson_court']:
            self.assertFalse(module.inspect(self.root, [name])['features'][0]['data_present'])

    def test_musketeer_reuses_required_base_camping(self):
        self.populate(); path = self.root / 'raid/camping/default.camping_skills.json'
        first = module.inspect(self.root, ['musketeer'])['features'][0]
        self.assertTrue(first['data_present']); self.assertEqual(first['hero_data']['camping_skill_count'], 1)
        path.write_text('{"skills":[]}', encoding='utf-8')
        second = module.inspect(self.root, ['musketeer'])['features'][0]
        self.assertNotEqual(first['data_sha256'], second['data_sha256'])
        self.assertFalse(second['hero_data']['fits_existing_skill_codec'])
        path.unlink()
        self.assertFalse(module.inspect(self.root, ['musketeer'])['features'][0]['data_present'])

    def test_heroes_codec_bounds_and_malformed_camping(self):
        self.populate()
        for hero in sorted(module.HEROES):
            directory = self.directory(hero)
            info = directory / ('heroes/' + hero + '/' + hero + '.info.darkest')
            with info.open('a', encoding='utf-8') as f: f.write('\ncombat_skill: .id "extra" .level 5\n')
            self.assertFalse(module.inspect(self.root, [hero])['features'][0]['hero_data']['fits_existing_skill_codec'])
        camp = self.directory('runaway') / 'raid/camping/runaway.camping_skills.json'
        for data in ['null', '{"skills":null}', '{"skills":[{"id":"bad","hero_classes":null}]}', 'not JSON']:
            camp.write_text(data, encoding='utf-8')
            self.assertEqual(module.inspect(self.root, ['runaway'])['features'][0]['hero_data']['camping_skill_count'], 0)

    def test_subset_duplicates_and_arena_excluded(self):
        self.populate(); before = module.inspect(self.root)
        subset = module.inspect(self.root, ['duelist', 'duelist', 'flagellant'])
        self.assertEqual([f['feature'] for f in subset['features']], ['duelist', 'flagellant'])
        self.put('dlc/1117860_arena_mp/data.json', 'CHANGED-IGNORED-ARENA')
        self.assertEqual(before, module.inspect(self.root))
        for unknown in ['arena_mp', 'butchers_circus', 'unknown_dlc']:
            with self.assertRaises(ValueError): module.inspect(self.root, [unknown])

    def test_shared_package_data_affects_child_features(self):
        self.populate(); before = self.by_name(module.inspect(self.root))
        self.put('dlc/4964110_fires_edge/shared/common.json', '{"shared":2}')
        after = self.by_name(module.inspect(self.root))
        self.assertEqual({n for n in before if before[n]['data_sha256'] != after[n]['data_sha256']}, {'runaway', 'duelist', 'fires_edge'})
        info = self.directory('flagellant') / 'heroes/flagellant/flagellant.info.darkest'
        info.write_bytes(info.read_bytes() + b'\n# altered data')
        third = self.by_name(module.inspect(self.root))
        self.assertEqual({n for n in after if after[n]['data_sha256'] != third[n]['data_sha256']}, {'flagellant'})

    def test_link_guards_without_os_link_privileges(self):
        self.populate(); target = self.directory('flagellant')
        real = module._linked
        with patch.object(module, '_linked', side_effect=lambda p: Path(p) == target or real(p)):
            with self.assertRaises(ValueError): module.inspect(self.root, ['flagellant'])
        target = self.directory('runaway') / 'upgrades'
        with patch.object(module, '_linked', side_effect=lambda p: Path(p) == target or real(p)):
            with self.assertRaises(ValueError): module.inspect(self.root, ['runaway'])

    def test_real_feature_link_escape_when_supported(self):
        outside = Path(self.temp.name) / 'outside'
        outside.mkdir(); (outside / 'data.json').write_text('{}', encoding='utf-8')
        feature = self.directory('flagellant'); feature.parent.mkdir(parents=True)
        try:
            feature.symlink_to(outside, target_is_directory=True)
        except OSError as error:
            self.skipTest('OS link creation unavailable: ' + str(error.winerror if hasattr(error, 'winerror') else error.errno))
        try:
            with self.assertRaises(ValueError): module.inspect(self.root, ['flagellant'])
        finally:
            feature.unlink()

    def test_compare_matching_and_pure(self):
        self.populate(); host = self.known_report(); guest = copy.deepcopy(host); saved = copy.deepcopy((host, guest))
        result = module.compare(host, guest)
        self.assertTrue(result['data_match']); self.assertTrue(result['build_match'])
        self.assertFalse(result['live_dlc_support_verified']); self.assertEqual((host, guest), saved)

    def test_compare_changes_missing_unknown_duplicate_and_schema(self):
        self.populate(); host = self.known_report(); guest = copy.deepcopy(host)
        self.by_name(guest)['runaway']['data_sha256'] = '0' * 64
        self.by_name(guest)['flagellant']['data_present'] = False
        guest['features'].append({'feature': 'unknown_feature'})
        result = module.compare(host, guest)
        self.assertFalse(result['data_match']); self.assertEqual(result['changed_features'], ['runaway'])
        self.assertEqual(result['missing_features'], ['flagellant']); self.assertEqual(result['unknown_features'], ['unknown_feature'])
        guest = copy.deepcopy(host); guest['features'].append(copy.deepcopy(guest['features'][0]))
        self.assertTrue(module.compare(host, guest)['errors'])
        guest['schema'] = 1; self.assertTrue(module.compare(host, guest)['errors'])
        guest = copy.deepcopy(host); self.by_name(guest)['runaway']['data_sha256'] = 7
        self.assertTrue(module.compare(host, guest)['errors'])

    def test_compare_required_subset_and_missing_reference(self):
        self.populate(); host = self.known_report(); guest = copy.deepcopy(host)
        self.by_name(guest)['runaway']['data_sha256'] = '0' * 64
        self.assertTrue(module.compare(host, guest, ['musketeer'])['data_match'])
        host['features'] = [f for f in host['features'] if f['feature'] != 'duelist']
        result = module.compare(host, guest, ['duelist', 'unknown'])
        self.assertEqual(result['missing_reference_features'], ['duelist']); self.assertEqual(result['unknown_features'], ['unknown'])
        self.assertFalse(result['data_match'])

    def test_compare_build_hash_cannot_be_overridden_by_flag(self):
        self.populate(); host = self.known_report(); guest = copy.deepcopy(host)
        guest['game_sha256'] = 'f' * 64; guest['known_game_build'] = True
        self.assertFalse(module.compare(host, guest)['build_match'])
        host['game_sha256'] = 'f' * 64
        result = module.compare(host, guest)
        self.assertTrue(result['build_match']); self.assertFalse(result['known_game_build']); self.assertFalse(result['data_match'])

    def test_cli_inspect_compare_and_arena_refusal(self):
        self.populate(); before = self.snapshot()
        command = [sys.executable, '-I', str(path)]
        p = subprocess.run(command + [str(self.root), '--enabled', 'runaway', 'duelist'], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr); self.assertEqual(len(json.loads(p.stdout)['features']), 2)
        report = self.known_report()
        a, b = Path(self.temp.name) / 'host.json', Path(self.temp.name) / 'guest.json'
        for f in [a, b]: f.write_text(json.dumps(report), encoding='utf-8')
        p = subprocess.run(command + ['--compare', str(a), str(b)], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr); self.assertTrue(json.loads(p.stdout)['data_match'])
        p = subprocess.run(command + [str(self.root), '--enabled', 'arena_mp'], capture_output=True, text=True)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(before, self.snapshot())


if __name__ == '__main__':
    unittest.main()
