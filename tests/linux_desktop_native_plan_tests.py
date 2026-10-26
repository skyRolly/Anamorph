#!/usr/bin/env python3
"""Ordinary-only recording tests of the production native test-executor plan.

No unshare/mount/setpriv/sudo, interpreter or installer is executed. Planned UID 0
is data only; fixture metadata belongs to the ordinary test user. These checks do
not establish real root ownership, shell interpretation or native gate success.
"""
import ast
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('native_plan',ROOT/'tests/linux_desktop_native_tests.py')
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)


class PlanCase(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='native-plan-');self.addCleanup(self.temp.cleanup)
        self.stage=Path(self.temp.name)/'stage';self.stage.mkdir()
        for name in ('install.sh','uninstall.sh','desktop-integration.sh','INSTALL.txt'):
            shutil.copy2(ROOT/'packaging/linux'/name,self.stage/name)
        shutil.copytree(ROOT/'resources/icons/linux',self.stage/'desktop-assets')
        (self.stage/'Anamorph').write_bytes(b'inert standalone fixture; never executed')
        module=self.stage/'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so';module.parent.mkdir(parents=True)
        module.write_bytes(b'inert VST3 fixture; never executed')
        native.STAGED=self.stage;native.UID=os.getuid();native.GID=os.getgid()
        native.STARTED=time.monotonic();native.PROGRESS_PATH=None
        self.case=native.NativeCase();self.case.setUp();self.addCleanup(self.case.doCleanups)
        self.recorded=[]

    def recorder(self,argv,**kwargs):
        self.assertEqual(argv[:6],['unshare','--mount','--pid','--net','--fork','--kill-child'])
        self.assertNotIn('--unshare-user',argv);self.assertNotIn('--uid',argv)
        self.assertEqual(argv[-2],'--namespace-config')
        config=json.loads(Path(argv[-1]).read_text());self.recorded.append(config)
        self.assertEqual(Path(argv[-1]).stat().st_mode & 0o777,0o600)
        for source,target,readonly in config['bindings']:
            self.assertTrue(target.startswith('/'));self.assertNotIn('..',Path(target).parts)
            if not readonly:self.assertTrue(Path(source).is_relative_to(self.case.root),'host write mount: '+source)
        self.assertEqual(config['environment']['PATH'],'/usr/bin:/bin')
        if Path('/usr/libexec').exists():
            self.assertIn(['/usr/libexec','/usr/libexec',True],config['bindings'])
        return subprocess.CompletedProcess(argv,0,'','')

    def test_entry_roles_use_real_uid_plan_without_user_namespace(self):
        with patch.object(native.subprocess,'run',self.recorder):
            for mode,root in [('user',False),('system',False),('system',True)]:
                self.case.invoke(mode=mode,root=root)
                config=self.recorded[-1]
                self.assertEqual(config['uid'],0 if root else os.getuid())
                self.assertEqual(config['command'],['/bin/sh','/package with spaces/install.sh','--'+mode])
                self.assertEqual(set(config['parent_namespaces']),{'mnt','pid','net','user'})

    def test_implicit_root_and_uninstall_selection(self):
        with patch.object(native.subprocess,'run',self.recorder):
            self.case.invoke(root=True,implicit=True)
            self.assertEqual(self.recorded[-1]['command'],['/bin/sh','/package with spaces/install.sh'])
            self.case.invoke(script='uninstall.sh',mode='system',root=False)
            self.assertEqual(self.recorded[-1]['uid'],os.getuid())
            self.assertEqual(self.recorded[-1]['command'],['/bin/sh','/package with spaces/uninstall.sh','--system'])

    def test_fault_utilities_keep_fixed_path_and_private_originals(self):
        self.case.make_fault()
        with patch.object(native.subprocess,'run',self.recorder):self.case.invoke(mode='system',root=True,fault='kill')
        config=self.recorded[-1]
        bindings={target:(source,ro) for source,target,ro in config['bindings']}
        self.assertEqual(bindings['/native-real/mv'],('/usr/bin/mv',True))
        self.assertTrue(bindings['/usr/bin/mv'][1])
        self.assertTrue(Path(bindings['/usr/bin/mv'][0]).is_relative_to(self.case.root))
        self.assertIn('/native-real/mv',Path(bindings['/usr/bin/mv'][0]).read_text())

    def test_production_bootstrap_is_selected_without_fixture_interpretation(self):
        self.assertIn('expected='+native.PIN,native.production_bootstrap())
        for entry in ('install.sh','uninstall.sh'):
            self.assertIn('expected='+native.PIN,(self.stage/entry).read_text())


class UserCoverageCase(unittest.TestCase):
    def test_real_suite_keeps_every_user_reliability_case(self):
        suite=unittest.defaultTestLoader.loadTestsFromTestCase(native.NativeCase)
        self.assertEqual(len(native.legacy.USER_RELIABILITY_METHODS),8)
        self.assertEqual(native.user_coverage_liveness(suite),list(native.legacy.USER_RELIABILITY_METHODS))
        for removed in native.legacy.USER_RELIABILITY_METHODS:
            partial=unittest.TestSuite(case for case in suite if case._testMethodName!=removed)
            with self.assertRaisesRegex(RuntimeError,'native suite omits required ordinary-user cases:'):
                native.user_reliability_coverage(partial)

    def test_disabled_coverage_verifier_is_rejected(self):
        suite=unittest.defaultTestLoader.loadTestsFromTestCase(native.NativeCase)
        with self.assertRaisesRegex(RuntimeError,'coverage verifier accepted an empty native suite'):
            native.user_coverage_liveness(suite,checker=lambda ignored: [])


class ResultLifecycleCase(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='native-result-');self.addCleanup(self.temp.cleanup)
        self.report=Path(self.temp.name)/'progress.json'
        native.PROGRESS_PATH=self.report;native.STARTED=time.monotonic()
        native.FAILURES_SO_FAR[:]=[];native.RESULT_PROGRESS.clear()
        self.addCleanup(setattr,native,'PROGRESS_PATH',None)

    def run_probe(self, kind):
        class Probe(unittest.TestCase):
            def setUp(self):
                if kind=='setup':raise RuntimeError('inert setup error')
                if kind=='cleanup':self.addCleanup(self.broken_cleanup)
            def broken_cleanup(self):raise RuntimeError('inert cleanup error')
            def runTest(self):
                if kind=='body':self.fail('inert body failure')
        result=unittest.TextTestRunner(stream=io.StringIO(),resultclass=native.ProgressResult).run(
            unittest.TestSuite([Probe()]))
        report=json.loads(self.report.read_text())
        self.assertEqual(report['status'],'in_progress')
        self.assertFalse(report['customer_archive_eligible'])
        self.assertEqual(report['test_result']['phase'],'stopped_after_cleanups')
        self.assertEqual(len(report['failure_details']),len(result.failures)+len(result.errors))
        self.assertEqual(report['test_result']['failures'],len(result.failures))
        self.assertEqual(report['test_result']['errors'],len(result.errors))
        self.assertFalse(result.wasSuccessful())
        return report

    def test_body_failure_checkpoint(self):
        self.assertEqual(self.run_probe('body')['test_result']['failures'],1)

    def test_setup_error_checkpoint(self):
        self.assertEqual(self.run_probe('setup')['test_result']['errors'],1)

    def test_cleanup_error_checkpoint(self):
        self.assertEqual(self.run_probe('cleanup')['test_result']['errors'],1)

    def test_incomplete_execution_remains_ineligible(self):
        result=native.ProgressResult(unittest.runner._WritelnDecorator(io.StringIO()),True,0)
        result.startTest(unittest.FunctionTestCase(lambda:None))
        report=json.loads(self.report.read_text())
        self.assertEqual(report['test_result']['phase'],'started')
        self.assertEqual(report['status'],'in_progress')
        self.assertFalse(report['customer_archive_eligible'])

    def test_unchanged_source_report_writer_creates_absent_parent(self):
        # Execute only the actual final writer AST, never main's installer path.
        source=ast.parse((ROOT/'tests/linux_desktop_install_tests.py').read_text())
        main=next(node for node in source.body if isinstance(node,ast.FunctionDef) and node.name=='main')
        block=next(node for node in main.body if isinstance(node,ast.If) and ast.unparse(node.test)=='args.report')
        writer=ast.Module(body=block.body,type_ignores=[])
        path=Path(self.temp.name)/'absent-build-parent'/'report.json'
        self.assertFalse(path.parent.exists())
        scope={'args':SimpleNamespace(report=path,hosted_namespace_permission=False),
               'json':json,'result':SimpleNamespace(testsRun=8,failures=[],errors=[]),
               'STAGED':None,'original_permission':None,'CLEANUP_SMOKE':False,'CLEANUP_RESULTS':[],
               'ORDINARY_ONLY':False,'USER_RELIABILITY':True}
        exec(compile(ast.fix_missing_locations(writer),'<production-report-writer>','exec'),scope)
        self.assertEqual(json.loads(path.read_text())['tests_run'],8)


if __name__=='__main__':
    if os.getuid()==0:raise SystemExit('native plan recorder must run as an ordinary user')
    unittest.main()
