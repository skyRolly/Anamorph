#!/usr/bin/env python3
"""Ordinary-only recording tests of the production native test-executor plan.

No unshare/mount/setpriv/sudo, interpreter or installer is executed. Planned UID 0
is data only; fixture metadata belongs to the ordinary test user. These checks do
not establish real root ownership, shell interpretation or native gate success.
"""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch

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


if __name__=='__main__':
    if os.getuid()==0:raise SystemExit('native plan recorder must run as an ordinary user')
    unittest.main()
