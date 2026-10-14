#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise authoritative CMake discovery and portable cache preservation offline."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tooling'))
import workflow


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='daemon dependencies with spaces ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix = self.root / 'external prefix'

    def package(self, name, version, targets):
        directory = self.prefix / 'lib/cmake' / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / f'{name}Config.cmake').write_text(''.join(
            f'add_library({target} INTERFACE IMPORTED)\n' for target in targets))
        (directory / f'{name}ConfigVersion.cmake').write_text(
            f'set(PACKAGE_VERSION "{version}")\n'
            'if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)\n'
            'set(PACKAGE_VERSION_COMPATIBLE FALSE)\nelse()\n'
            'set(PACKAGE_VERSION_COMPATIBLE TRUE)\nendif()\n')

    def configure(self, success=True, tests=False, systemd=False, extra=()):
        build = self.root / f'build-{len(list(self.root.glob("build-*")))}'
        command = ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
                   f'-DBUILD_TESTS={"ON" if tests else "OFF"}',
                   f'-DBUILD_SYSTEMD={"ON" if systemd else "OFF"}',
                   f'-DHOLONIGHT_EXTERNAL_PREFIX={self.prefix}', *extra]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode == 0, success, result.stdout)
        self.assertFalse((build / '_deps').exists())
        return result.stdout, build

    def test_missing_json_is_required(self):
        output, _ = self.configure(False, extra=['-DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=TRUE'])
        self.assertIn('nlohmann_json', output)

    def test_missing_toml_is_required(self):
        output, _ = self.configure(False, extra=['-DCMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=TRUE'])
        self.assertIn('tomlplusplus', output)

    def test_old_toml_rejected(self):
        self.package('tomlplusplus', '3.3.0', ['tomlplusplus::tomlplusplus'])
        output, _ = self.configure(False, extra=[f'-Dtomlplusplus_DIR={self.prefix}/lib/cmake/tomlplusplus',
            '-DCMAKE_IGNORE_PATH=/usr/lib/cmake/tomlplusplus;/usr/share/cmake/tomlplusplus;/lib/cmake/tomlplusplus;/usr/lib64/cmake/tomlplusplus;/usr/share/tomlplusplus;/usr/lib/cmake/tomlplusplus/;/lib/cmake/tomlplusplus/'])
        self.assertIn('3.4', output)

    def test_missing_gtest_rejected(self):
        self.configure(False, tests=True, extra=['-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE'])

    def test_old_gtest_rejected(self):
        self.package('GTest', '1.13.0', ['GTest::gtest', 'GTest::gtest_main', 'GTest::gmock'])
        output, _ = self.configure(False, tests=True, extra=[f'-DGTest_DIR={self.prefix}/lib/cmake/GTest',
            '-DCMAKE_IGNORE_PATH=/usr/lib/cmake/GTest;/usr/share/cmake/GTest;/lib/cmake/GTest;/usr/lib64/cmake/GTest;/usr/lib/cmake/GTest/;/lib/cmake/GTest/'])
        self.assertIn('1.14', output)

    def test_missing_gmock_rejected(self):
        self.package('GTest', '1.14.0', ['GTest::gtest', 'GTest::gtest_main'])
        output, _ = self.configure(False, tests=True)
        self.assertIn('GTest::gmock', output)

    def test_tests_off_never_discovers_gtest(self):
        self.configure(extra=['-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE'])

    def test_systemd_on_requires_package(self):
        env = dict(os.environ, PKG_CONFIG_LIBDIR=str(self.root / 'empty pkgconfig'))
        with patch.dict(os.environ, env, clear=True):
            output, _ = self.configure(False, systemd=True)
        self.assertIn('libsystemd', output)

    def test_systemd_off_has_no_discovery_definition_or_link(self):
        _, build = self.configure(extra=['-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE'])
        ninja = (build / 'build.ninja').read_text()
        self.assertNotIn('HOLONIGHTD_HAS_SYSTEMD', ninja)
        self.assertNotIn('libsystemd', ninja)

    def test_explicit_external_prefix_with_spaces(self):
        self.package('nlohmann_json', '3.12.0', ['nlohmann_json::nlohmann_json'])
        _, build = self.configure()
        self.assertIn(str(self.prefix), (build / 'CMakeCache.txt').read_text())

    def test_feature_change_preserves_incompatible_build(self):
        config = json.loads((ROOT / 'tooling/module.json').read_text())
        root = self.root / 'workflow'
        root.mkdir()
        (root / 'CMakePresets.json').write_text((ROOT / 'CMakePresets.json').read_text())
        build = root / 'build/test'
        build.mkdir(parents=True)
        (build / 'CMakeCache.txt').write_text('old cache')
        (build / 'sentinel').write_text('preserved')
        with patch.object(workflow, 'ROOT', root), patch.object(workflow, 'check_external', side_effect=lambda *args, **kwargs: kwargs['features']), \
             patch.object(workflow, 'toolchain_signature', return_value={}), \
             patch.object(workflow, 'run'), patch.object(workflow, 'refresh'):
            with patch.dict(os.environ, {'HOLONIGHT_BUILD_FEATURES': 'BUILD_SYSTEMD=ON'}):
                workflow.configure(config, 'test')
            self.assertEqual((root / 'build/test.previous-1/sentinel').read_text(), 'preserved')
            (build / 'CMakeCache.txt').write_text('new cache')
            with patch.dict(os.environ, {'HOLONIGHT_BUILD_FEATURES': 'BUILD_SYSTEMD=OFF'}):
                workflow.configure(config, 'test')
            self.assertTrue((root / 'build/test.previous-2/CMakeCache.txt').is_file())


if __name__ == '__main__':
    unittest.main()
