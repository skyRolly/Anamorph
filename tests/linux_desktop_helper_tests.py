#!/usr/bin/env python3
"""Ordinary-only helper capture tests: no elevation or fixture interpretation.

Extract the production entry-point function, substitute only its trusted digest
for inert fixture bytes, and supply a recording priv() executor. It runs the
literal bootstrap as the current user, with root ownership metadata modelled by
stat(), temporary creation redirected under the fixture root, and the final
sh() interpreter replaced by a recorder. Files, modes, links, hashing and cleanup
are real; root ownership/protection against a different account is not tested.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
START = '# BEGIN verified desktop helper\n'
END = '# END verified desktop helper\n'
FIXTURE = b'inert helper fixture bytes\x00\xff\n'


def entry_block(name):
    text = (ROOT / 'packaging/linux' / name).read_text()
    return START + text.split(START, 1)[1].split(END, 1)[0] + END


# Functions exist only in the test executor, never in the production entry code.
EXECUTOR = r'''
priv() {
    [ "$1" = /bin/sh ] && [ "$2" = -c ] || return 90
    bootstrap=$3
    shift 3
    /bin/sh -c '
        mktemp() {
            /usr/bin/mktemp -d "$TEST_ROOT/snapshots/helper.XXXXXXXXXX"
        }
        stat() {
            /usr/bin/python3 "$TEST_ROOT/metadata.py" "$@"
        }
        sh() {
            /usr/bin/python3 "$TEST_ROOT/record.py" "$@"
        }
    '"$bootstrap" "$@"
}
'''

METADATA = '''import os, pathlib, subprocess, sys
args = sys.argv[1:]
path = pathlib.Path(args[-1])
root = pathlib.Path(os.environ['TEST_ROOT'])
result = subprocess.check_output(['/usr/bin/stat', *args], text=True).strip()
if root / 'snapshots' in path.parents:
    # Translate only the actual fixture owner to root, never invent file modes.
    fields = result.split()
    if fields[0] == str(os.getuid()):
        fields[0] = '0'
    result = ' '.join(fields)
    fault = os.environ.get('METADATA_FAULT')
    if fault == 'directory' and path.is_dir(): result = '0 755'
    if path.is_file():
        if fault == 'owner': result = '99 600 1'
        if fault == 'mode': result = '0 644 1'
        if fault == 'links': result = '0 600 2'
print(result)
'''

RECORDER = '''import hashlib, json, os, pathlib, sys
root = pathlib.Path(os.environ['TEST_ROOT'])
snapshot = pathlib.Path(sys.argv[1])
assert root / 'snapshots' in snapshot.parents
assert snapshot != pathlib.Path(os.environ['SOURCE'])
assert snapshot.is_file() and not snapshot.is_symlink()
assert snapshot.stat().st_mode & 0o777 == 0o600
assert snapshot.parent.stat().st_mode & 0o777 == 0o700
assert snapshot.stat().st_nlink == 1
if os.environ.get('REPLACE_SOURCE'):
    source = pathlib.Path(os.environ['SOURCE'])
    source.unlink()
    source.write_bytes(b'changed source after verification')
result = {'argv': sys.argv[1:], 'digest': hashlib.sha256(snapshot.read_bytes()).hexdigest(),
          'uid': os.getuid(), 'source_digest': hashlib.sha256(pathlib.Path(os.environ['SOURCE']).read_bytes()).hexdigest()}
(root / 'record.json').write_text(json.dumps(result))
raise SystemExit(int(os.environ.get('RECORDER_STATUS', '0')))
'''


class HelperCaptureTests(unittest.TestCase):
    def setUp(self):
        self.assertNotEqual(os.getuid(), 0, 'these tests must run without root')
        self.temp = tempfile.TemporaryDirectory(prefix='anamorph-helper-unit-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.package = self.root / 'archive space % " $ ` \\'
        self.package.mkdir()
        (self.root / 'snapshots').mkdir()
        self.source = self.package / 'desktop-integration.sh'
        self.source.write_bytes(FIXTURE)
        (self.root / 'metadata.py').write_text(METADATA)
        (self.root / 'record.py').write_text(RECORDER)

    def invoke(self, entry='install.sh', action='preflight', **environment):
        snapshots_before = set((self.root / 'snapshots').iterdir())
        block = entry_block(entry)
        # Fail before running if the production interpreter boundary changes.
        self.assertEqual(block.count('sh "$snapshot/desktop-integration.sh" "$@" "$assets"'), 1)
        self.assertEqual(block.count('/bin/sh'), 1)  # literal bootstrap interpreter only
        digest = hashlib.sha256(FIXTURE).hexdigest()
        block, count = re.subn(r'expected=[0-9a-f]{64}', 'expected=' + digest, block)
        self.assertEqual(count, 1)
        script = (EXECUTOR + block + '\nset -eu\nmode=system\nHERE=' + shlex.quote(str(self.package))
                  + '\ndesktop_integration ' + shlex.quote(action)
                  + ' ' + shlex.quote(str(self.root / 'data space % " $ ` \\'))
                  + ' ' + shlex.quote(str(self.root / 'bin/Anamorph')) + '\n')
        result = subprocess.run(['/bin/sh', '-c', script], text=True, capture_output=True,
                                env={**os.environ, 'TEST_ROOT': str(self.root), 'SOURCE': str(self.source),
                                     **environment}, timeout=15)
        self.assertEqual(set((self.root / 'snapshots').iterdir()), snapshots_before, 'snapshot cleanup changed unrelated paths')
        return result

    def rejected(self, message, **environment):
        result = self.invoke(**environment)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(message, result.stderr)
        self.assertFalse((self.root / 'record.json').exists(), 'rejected bytes reached interpreter')

    def test_valid_capture_for_each_system_operation(self):
        for entry, action in (('install.sh', 'preflight'), ('install.sh', 'install'),
                              ('uninstall.sh', 'uninstall')):
            with self.subTest(entry=entry, action=action):
                result = self.invoke(entry=entry, action=action)
                self.assertEqual(result.returncode, 0, result.stderr)
                record = json.loads((self.root / 'record.json').read_text())
                self.assertEqual(record['digest'], hashlib.sha256(FIXTURE).hexdigest())
                self.assertEqual(record['uid'], os.getuid())
                self.assertEqual(record['argv'][1:], [action, str(self.root / 'data space % " $ ` \\'),
                                                     str(self.root / 'bin/Anamorph'), str(self.package / 'desktop-assets')])
                self.assertFalse(Path(record['argv'][0]).exists())

    def test_missing_input(self):
        self.source.unlink()
        self.rejected('missing/nonregular/linked helper')

    def test_nonregular_input(self):
        self.source.unlink()
        self.source.mkdir()
        self.rejected('missing/nonregular/linked helper')
        self.source.rmdir()
        os.mkfifo(self.source)
        self.rejected('missing/nonregular/linked helper')

    def test_symlink_input(self):
        target = self.package / 'target'; target.write_bytes(FIXTURE)
        self.source.unlink(); self.source.symlink_to(target)
        self.rejected('missing/nonregular/linked helper')
        target.unlink()
        self.rejected('missing/nonregular/linked helper')

    def test_linked_ancestor(self):
        link = self.root / 'linked-archive'; link.symlink_to(self.package, target_is_directory=True)
        self.package = link
        self.rejected('linked helper ancestry')

    def test_hardlinked_input(self):
        os.link(self.source, self.package / 'second-name')
        self.rejected('hard-linked helper')

    def test_real_writable_source_mode(self):
        self.source.chmod(0o666)  # disposable fixture only
        self.rejected('writable helper metadata')

    def test_modelled_snapshot_metadata_rejection(self):
        for fault in ('directory', 'owner', 'mode', 'links'):
            with self.subTest(fault=fault):
                self.rejected('unsafe snapshot', METADATA_FAULT=fault)

    def test_changed_source_bytes_and_mutable_sidecar(self):
        self.source.write_bytes(b'changed inert bytes')
        # A sidecar cannot authorize code: the production digest stays embedded.
        (self.package / 'SHA256SUMS').write_text(hashlib.sha256(self.source.read_bytes()).hexdigest())
        self.rejected('helper digest mismatch')

    def test_consumption_survives_sequential_source_replacement(self):
        result = self.invoke(REPLACE_SOURCE='1')
        self.assertEqual(result.returncode, 0, result.stderr)
        record = json.loads((self.root / 'record.json').read_text())
        self.assertEqual(record['digest'], hashlib.sha256(FIXTURE).hexdigest())
        self.assertNotEqual(record['digest'], record['source_digest'])

    def test_consumer_failure_propagates_and_cleans_only_snapshot(self):
        sentinel = self.root / 'snapshots/keep'; sentinel.mkdir()
        keep = sentinel / 'unrelated'; keep.write_bytes(b'keep')
        result = self.invoke(RECORDER_STATUS='47')
        self.assertEqual(result.returncode, 47, result.stderr)
        self.assertEqual(keep.read_bytes(), b'keep')

    def test_production_pins_and_all_entry_paths(self):
        block = entry_block('install.sh')
        self.assertEqual(block, entry_block('uninstall.sh'))
        helper = (ROOT / 'packaging/linux/desktop-integration.sh').read_bytes()
        self.assertEqual(re.search(r'expected=([0-9a-f]{64})', block)[1], hashlib.sha256(helper).hexdigest())
        self.assertIn('ASSETS=${4:-"$HERE/desktop-assets"}', helper.decode())
        for name, actions in (('install.sh', ('preflight', 'install')), ('uninstall.sh', ('uninstall',))):
            text = (ROOT / 'packaging/linux' / name).read_text()
            self.assertNotIn('priv sh "$HERE/desktop-integration.sh"', text)
            outside = text.replace(block, '')
            self.assertNotIn('sh "$HERE/desktop-integration.sh"', outside)
            for action in actions:
                self.assertEqual(outside.count('desktop_integration ' + action + ' '), 2)
            self.assertIn('mode=system', outside)
            self.assertIn('if [ "$requested" = user ]; then', outside)


if __name__ == '__main__':
    unittest.main()
