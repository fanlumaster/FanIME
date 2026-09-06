"""Run local setup in a moved checkout without replacing the reviewed build project."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PrepareEnvironmentTests(unittest.TestCase):
    def exercise(self, component):
        with tempfile.TemporaryDirectory(prefix='msime moved checkout ') as directory:
            root = Path(directory)
            target = root / component
            shutil.copytree(ROOT / component / 'scripts', target / 'scripts')
            for name in ('CMakeLists.txt', 'CMakePresets.json'):
                shutil.copy2(ROOT / component / name, target / name)
            (target / 'tests').mkdir()
            cmake = target / 'CMakeLists.txt'
            cmake.write_text(cmake.read_text() + '\n# locally reviewed build setting\n')
            original = cmake.read_bytes()
            presets_file = target / 'CMakePresets.json'
            presets = json.loads(presets_file.read_text())
            presets['configurePresets'].append({'name': 'local-extra', 'inherits': 'vcpkg-release',
                                                'cacheVariables': {'LOCAL_SETTING': 'kept'}})
            presets_file.write_text(json.dumps(presets, indent=2))
            env = dict(os.environ, VCPKG_ROOT=str(root / 'custom vcpkg'), BOOST_ROOT=str(root / 'custom boost'))
            for _ in range(2):
                subprocess.run([sys.executable, str(target / 'scripts/prepare_env.py')],
                               cwd=directory, env=env, check=True, capture_output=True)
                self.assertEqual(original, cmake.read_bytes())
                actual = json.loads(presets_file.read_text())
                by_name = {p['name']: p for p in actual['configurePresets']}
                self.assertEqual(by_name['local-extra']['cacheVariables']['LOCAL_SETTING'], 'kept')
                self.assertIn('vcpkg-release', by_name)
                expected = (root / 'custom vcpkg/scripts/buildsystems/vcpkg.cmake').as_posix()
                self.assertEqual(by_name['vcpkg-release']['cacheVariables']['CMAKE_TOOLCHAIN_FILE'], expected)
            clangd = (target / '.clangd').read_text().replace('\\', '/')
            self.assertIn('vendor/MetasequoiaImeEngine/utfcpp/source', clangd)
            if component == 'server':
                self.assertIn('vendor/MetasequoiaImeEngine/voice/include', clangd)
                self.assertIn('vendor/MetasequoiaImeEngine/voice/third_party/miniaudio', clangd)
                self.assertIn('/ui/include', clangd)
                self.assertFalse((target / 'tests/CMakePresets.json').exists())

    def test_tsf_environment_preserves_the_monorepo_build(self):
        self.exercise('windows')

    def test_server_environment_resolves_shared_components(self):
        self.exercise('server')


if __name__ == '__main__':
    unittest.main()
