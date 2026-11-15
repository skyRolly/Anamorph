#!/usr/bin/env python3
"""Negative controls for the recorded recovery-cost data validator."""
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import validate_h4_recovery_cost as check


class RecoveryCostControls(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)/'results'
        shutil.copytree(Path(__file__).resolve().parent/'h4-recovery-instruction-results-v2', self.root)

    def test_altered_callgrind_is_rejected(self):
        path = self.root/'callgrind-recovery.gz'
        path.write_bytes(gzip.compress(gzip.decompress(path.read_bytes())+b'changed\n'))
        with self.assertRaisesRegex(ValueError, 'callgrind digest mismatch'):
            check.validate(self.root)

    def test_missing_reset_is_rejected_even_with_updated_stream_hash(self):
        path = self.root/'run-1.stdout'
        data = path.read_bytes().replace(b'reset_blocks=256', b'reset_blocks=0')
        path.write_bytes(data)
        p = self.root/'identity.json'; record = json.loads(p.read_text())
        record['runs'][1]['stdout_sha256'] = hashlib.sha256(data).hexdigest()
        p.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, 'observed recovery path missing'):
            check.validate(self.root)


if __name__ == '__main__':
    unittest.main(verbosity=2)
