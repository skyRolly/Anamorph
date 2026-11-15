#!/usr/bin/env python3
"""Discriminate corrupted recorded data from unexercised workload premises."""
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

import compare_h4_instruction_results as compare


class ComparatorControls(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        self.data = root/'h4-instruction-baseline-results'
        shutil.copytree(compare.ROOT/'h4-instruction-baseline-results', self.data)
        patcher = mock.patch.object(compare, 'ROOT', root)
        patcher.start()
        self.addCleanup(patcher.stop)

    def alter_stdout_with_matching_receipt_hash(self, run, old, new):
        path = self.data/f'run-{run}.stdout'
        data = path.read_bytes()
        self.assertIn(old, data)
        data = data.replace(old, new)
        path.write_bytes(data)
        identity = self.data/'identity.json'
        record = json.loads(identity.read_text())
        record['runs'][run]['stdout_sha256'] = hashlib.sha256(data).hexdigest()
        identity.write_text(json.dumps(record))

    def test_unrelated_container_exit_is_rejected(self):
        path = self.data/'docker-events.jsonl'
        events = [json.loads(line) for line in path.read_text().splitlines()]
        for event in events:
            event['Actor']['ID'] = '0'*64
        path.write_text(''.join(json.dumps(event)+'\n' for event in events))
        with self.assertRaisesRegex(ValueError, 'container exit not verified'):
            compare.inspect('baseline')

    def test_altered_callgrind_bytes_are_rejected(self):
        path = self.data/'callgrind-band1.gz'
        data = gzip.decompress(path.read_bytes()) + b'changed\n'
        path.write_bytes(gzip.compress(data))
        with self.assertRaisesRegex(ValueError, 'callgrind digest mismatch'):
            compare.inspect('baseline')

    def test_unwrapped_execution_is_rejected_even_with_matching_hash(self):
        self.alter_stdout_with_matching_receipt_hash(4, b'wrapped_blocks=256', b'wrapped_blocks=0')
        with self.assertRaisesRegex(ValueError, 'wrapped path did not run'):
            compare.inspect('baseline')

    def test_missing_fade_and_glide_are_rejected_even_with_matching_hash(self):
        self.alter_stdout_with_matching_receipt_hash(6, b'fade_blocks=160', b'fade_blocks=0')
        with self.assertRaisesRegex(ValueError, 'fade did not run'):
            compare.inspect('baseline')
        original_root = Path(__file__).resolve().parent/'h4-instruction-baseline-results'
        shutil.copyfile(original_root/'run-6.stdout', self.data/'run-6.stdout')
        original = json.loads((original_root/'identity.json').read_text())
        (self.data/'identity.json').write_text(json.dumps(original))
        self.alter_stdout_with_matching_receipt_hash(7, b'proved sustained-glide samples 32768', b'proved sustained-glide samples 0')
        with self.assertRaisesRegex(ValueError, 'sustained glide not established'):
            compare.inspect('baseline')


if __name__ == '__main__':
    unittest.main(verbosity=2)
