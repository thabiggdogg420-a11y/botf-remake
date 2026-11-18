"""Build wrapper regression tests. Fake CMake only; no downloads or compilation."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

class BuildScriptTests(unittest.TestCase):
    def run_build(self, mode='ok', stale=False, args=('linux', 'debug')):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'build.sh').write_bytes(Path(__file__).with_name('build.sh').read_bytes())
            (root/'fetch-godot-cpp.sh').write_text('#!/bin/sh\nexit 0\n')
            (root/'fetch-godot-cpp.sh').chmod(0o755)
            (root/'fake').mkdir()
            cmake = root/'fake/cmake'
            cmake.write_text('''#!/usr/bin/env bash
if [[ "$1" != --build ]]; then
  [[ "$MODE" == configure-fail ]] && exit 13
  exit 0
fi
printf '%s\\n' "$2" >> attempts
if [[ "$MODE" == build-fail ]]; then echo 'compiler error' >&2; exit 17; fi
if [[ "$MODE" == filtered ]]; then echo '[ 20%] Building CXX object godot-cpp/example.cpp'; fi
if [[ "$MODE" == missing-output ]]; then exit 0; fi
platform=${2#build-}; platform=${platform%-*}; target=${2##*-}
mkdir -p bin
if [[ "$platform" == linux ]]; then ext=so; else ext=dll; fi
touch "bin/libbotf_core.$platform.template_$target.x86_64.$ext"
''')
            cmake.chmod(0o755)
            if stale:
                (root/'bin').mkdir()
                (root/'bin/libbotf_core.linux.template_debug.x86_64.so').write_bytes(b'old')
            env = dict(os.environ, PATH=str(root/'fake')+os.pathsep+os.environ['PATH'], MODE=mode, JOBS='1')
            r = subprocess.run(['bash', str(root/'build.sh'), *args], env=env, text=True, capture_output=True)
            attempts = (root/'attempts').read_text().splitlines() if (root/'attempts').exists() else []
            return r, attempts
    def test_failed_build_with_stale_binary(self):
        r, _ = self.run_build('build-fail', True)
        self.assertEqual(r.returncode, 17)
        self.assertIn('compiler error', r.stderr)
        self.assertNotIn('[core] built', r.stdout)
    def test_failed_build_without_binary(self):
        r, _ = self.run_build('build-fail')
        self.assertEqual(r.returncode, 17)
    def test_filtered_success(self):
        r, _ = self.run_build('filtered')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('Building CXX', r.stdout)
        self.assertIn('[core] built linux template_debug', r.stdout)
    def test_quiet_success(self):
        r, _ = self.run_build()
        self.assertEqual(r.returncode, 0, r.stderr)
    def test_configure_failure(self):
        r, attempts = self.run_build('configure-fail', True)
        self.assertEqual(r.returncode, 13)
        self.assertEqual(attempts, [])
    def test_missing_output(self):
        r, _ = self.run_build('missing-output')
        self.assertNotEqual(r.returncode, 0)
        self.assertNotIn('[core] built', r.stdout)
    def test_all_platforms_targets(self):
        r, attempts = self.run_build(args=('all','both'))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(attempts, ['build-linux-debug','build-linux-release','build-windows-debug','build-windows-release'])
    def test_failure_stops_later_targets(self):
        r, attempts = self.run_build('build-fail', True, ('all','both'))
        self.assertEqual(r.returncode, 17)
        self.assertEqual(attempts, ['build-linux-debug'])
if __name__ == '__main__': unittest.main()
