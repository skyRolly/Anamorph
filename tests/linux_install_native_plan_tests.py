#!/usr/bin/env python3
"""Nonprivileged recording tests; these do not establish native sudo execution."""
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import re
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('native_install', ROOT / 'tests/linux_install_native_tests.py')
native = importlib.util.module_from_spec(spec); spec.loader.exec_module(native)


class PlanCase(unittest.TestCase):
    def setUp(self):
        native.UID = os.getuid(); native.GID = os.getgid(); native.STARTED = time.monotonic()
        native.REPORT = None; native.cases.STAGED = None
        self.case = native.NativeCase(); self.case.setUp(); self.addCleanup(self.case.doCleanups)
        self.records = []

    def recorder(self, argv, **kwargs):
        self.assertEqual(argv[:6], ['unshare', '--mount', '--pid', '--net', '--fork', '--kill-child'])
        self.assertNotIn('--unshare-user', argv)
        config = json.loads(Path(argv[-1]).read_text()); self.records.append(config)
        self.assertEqual(config['environment']['PATH'], '/usr/bin:/bin')
        self.assertEqual(Path(argv[-1]).stat().st_mode & 0o777, 0o600)
        for source, destination, readonly in config['bindings']:
            self.assertTrue(destination.startswith('/'))
            if not readonly: self.assertTrue(Path(source).is_relative_to(self.case.root))
        return subprocess.CompletedProcess(argv, 0, '', '')

    def test_user_system_and_root_plans_remain_isolated(self):
        with patch.object(native, 'own_user_fixture', lambda path: None), patch.object(native.subprocess, 'run', self.recorder):
            for mode, root in [('user', False), ('system', False), ('system', True)]:
                self.case.invoke(mode=mode, root=root)
        self.assertEqual([item['uid'] for item in self.records], [os.getuid(), os.getuid(), 0])
        self.assertEqual([item['command'][-1] for item in self.records], ['--user', '--system', '--system'])

    def test_fault_tools_are_readonly_and_keep_fixed_path(self):
        self.case.fault_tool('mv', 'exit 1')
        with patch.object(native, 'own_user_fixture', lambda path: None), patch.object(native.subprocess, 'run', self.recorder):
            self.case.invoke(fault='fail')
        self.assertIn(['/usr/bin/mv', '/native-real/mv', True], self.records[0]['bindings'])

    def test_every_case_is_required_and_omission_is_detected(self):
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(native.NativeCase)
        self.assertTrue(native.coverage_liveness(suite))
        for name in native.cases.REQUIRED_METHODS + native.NATIVE_REQUIRED_METHODS:
            partial = unittest.TestSuite(test for test in suite if test._testMethodName != name)
            with self.assertRaisesRegex(RuntimeError, 'omits mandatory installer cases'):
                native.coverage(partial)
        with self.assertRaisesRegex(RuntimeError, 'accepted an empty suite'):
            native.coverage_liveness(suite, checker=lambda ignored: [])

    def test_incomplete_report_is_ineligible_and_creates_parent(self):
        native.REPORT = self.case.root / 'report/new/result.json'
        self.addCleanup(setattr, native, 'REPORT', None)
        native.checkpoint('incomplete', error='unavailable isolation')
        data = json.loads(native.REPORT.read_text())
        self.assertEqual(data['status'], 'incomplete')
        self.assertFalse(data['customer_archive_eligible'])

    def test_cleanup_error_is_recorded_before_success_can_be_reported(self):
        native.REPORT = self.case.root / 'lifecycle/result.json'
        self.addCleanup(setattr, native, 'REPORT', None)
        class FailingCleanup(unittest.TestCase):
            def runTest(self):
                def fail(): raise RuntimeError('seeded cleanup failure')
                self.addCleanup(fail)
        result = unittest.TextTestRunner(stream=io.StringIO(), resultclass=native.ProgressResult).run(FailingCleanup())
        self.assertFalse(result.wasSuccessful())
        data = json.loads(native.REPORT.read_text())
        self.assertEqual(data['errors'], 1)
        self.assertFalse(data['customer_archive_eligible'])

    def test_customer_upload_requires_source_native_and_behavior_gates(self):
        text = (ROOT / '.github/workflows/build.yml').read_text()
        def checked(workflow):
            upload = re.search(r'- name: Upload Linux artifacts\n([\s\S]*?)(?=\n      - name:)', workflow)
            self.assertIsNotNone(upload)
            for name in ('tests', 'stage', 'plugin_install'):
                self.assertIn("steps." + name + ".outcome == 'success'", upload[1])
            native_step = re.search(r'id: plugin_install\n([\s\S]*?)(?=\n      - name:)', workflow)
            self.assertIsNotNone(native_step)
            for name in ('tests', 'stage', 'package_source'):
                self.assertIn("steps." + name + ".outcome == 'success'", native_step[1])
            self.assertIn('sudo -n', native_step[1])
            self.assertIn('tests/linux_install_native_tests.py', native_step[1])
        checked(text)
        for name in ('tests', 'stage', 'plugin_install', 'package_source'):
            mutant = text.replace("steps." + name + ".outcome == 'success'", 'true')
            with self.assertRaises(AssertionError): checked(mutant)


if __name__ == '__main__': unittest.main()
