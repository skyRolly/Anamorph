#!/usr/bin/env python3
"""Real UID/ownership installer gate in private roots on disposable hosted VMs.

Run via actual sudo on a fresh GitHub-hosted Linux VM. No user namespace, metadata
recorder, substituted interpreter or host security-setting change is used. Trusted
VM-private utility overlays observe the real shell and inject bounded failures in
owned test processes; production helper bytes, pins and fixed PATH are unchanged.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('desktop_cases', ROOT/'tests/linux_desktop_install_tests.py')
legacy = importlib.util.module_from_spec(spec); spec.loader.exec_module(legacy)
PIN = '3ea641f74a7a04abd2cffeac4298e54022c078655593ad45d226d622ece3c45e'
UID = GID = None
OBSERVATIONS = []
OPERATIONS = []
STAGED = None
ARCHIVES = []
PROGRESS_PATH = None
STARTED = None
FAILURES_SO_FAR = []

# Called by a root-owned, read-only /usr/bin/dirname overlay only when the
# unchanged helper asks dirname($0). The parent is the actual /bin/sh interpreting
# the protected snapshot. All other dirname calls go straight to the real tool.
OBSERVER = r'''import hashlib,json,os,signal,sys
from pathlib import Path
path=Path(sys.argv[1]); parent=int(sys.argv[2])
if not str(path).startswith('/tmp/anamorph-desktop-helper.') or path.name!='desktop-integration.sh':
 raise SystemExit(0)
file=path.stat(); directory=path.parent.stat()
args=Path('/proc/'+str(parent)+'/cmdline').read_bytes().split(b'\0')
args=[a.decode() for a in args if a]
assert file.st_uid==0 and file.st_gid==0 and file.st_mode & 0o7777==0o600 and file.st_nlink==1
assert directory.st_uid==0 and directory.st_gid==0 and directory.st_mode & 0o7777==0o700
assert hashlib.sha256(path.read_bytes()).hexdigest()==''' + repr(PIN) + r'''
assert args[1]==str(path) and args[2] in ('preflight','install','uninstall'),args
assert args[5]=='/package with spaces/desktop-assets',args
record={'snapshot':str(path),'uid':file.st_uid,'gid':file.st_gid,'mode':oct(file.st_mode & 0o7777),
 'links':file.st_nlink,'directory_mode':oct(directory.st_mode & 0o7777),
 'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'actual_shell_argv':args,'action':args[2]}
with open('/native-audit/snapshots.jsonl','a') as stream: stream.write(json.dumps(record)+'\n')
control=Path('/native-control/signal')
if control.exists() and args[2]=='install':
 sig=control.read_text().strip(); control.unlink()
 # The real helper's parent is the literal bootstrap. Signal that test-owned
 # process only, then let the real helper return so the bootstrap can trap it.
 stat=Path('/proc/'+str(parent)+'/stat').read_text(); bootstrap=int(stat[stat.rfind(')')+2:].split()[1])
 cmd=Path('/proc/'+str(bootstrap)+'/cmdline').read_bytes()
 assert b'anamorph-desktop-helper' in cmd and b'-c\0' in cmd
 record['bootstrap_signal']=sig
 with open('/native-audit/signals.jsonl','a') as stream: stream.write(json.dumps(record)+'\n')
 os.kill(bootstrap,getattr(signal,'SIG'+sig))
'''


def write_progress(active=None):
    if PROGRESS_PATH is None: return
    data={'status':'in_progress','customer_archive_eligible':False,
          'elapsed_seconds':round(time.monotonic()-STARTED,3),
          'active_operation':active,'completed_operations':OPERATIONS,
          'snapshot_observations':OBSERVATIONS,'cleanup_fault_cases':legacy.CLEANUP_RESULTS,
          'failure_details':FAILURES_SO_FAR,'kernel_security_settings_changed':False,
          'ordinary_invoking_uid':UID,'root_metadata_mocked':False,'interpreter_replaced':False}
    # Atomic checkpoint within the task report directory. Cancellation cannot
    # turn partial evidence into a completed gate or customer eligibility.
    pending=PROGRESS_PATH.with_suffix('.pending')
    pending.write_text(json.dumps(data,indent=2)+'\n'); pending.replace(PROGRESS_PATH)


def require_disposable_runner():
    if not (sys.platform.startswith('linux') and os.getuid()==0
            and os.environ.get('GITHUB_ACTIONS')=='true'
            and os.environ.get('RUNNER_ENVIRONMENT')=='github-hosted'
            and os.environ.get('RUNNER_OS')=='Linux'
            and os.environ.get('GITHUB_REPOSITORY')=='skyRolly/Anamorph'):
        raise RuntimeError('native installer gate requires actual sudo on this repository\'s fresh disposable GitHub-hosted Linux VM')
    uid=int(os.environ.get('SUDO_UID','0')); gid=int(os.environ.get('SUDO_GID','0'))
    if uid==0 or gid==0: raise RuntimeError('an actual ordinary invoking user is required')
    if not shutil.which('bwrap') or not shutil.which('sudo'):
        raise RuntimeError('native private-root gate requires Bubblewrap and real sudo')
    return uid,gid


def own_user_fixture(path):
    # Fixture preparation uses real chown in task-owned paths, not a stat mock.
    # Never follow benign symlink fixtures or touch the system mounts.
    for directory, dirs, files in os.walk(path,followlinks=False):
        os.chown(directory,UID,GID,follow_symlinks=False)
        for name in dirs+files: os.chown(Path(directory)/name,UID,GID,follow_symlinks=False)


class NativeCase(legacy.InstallCase):
    def setUp(self):
        super().setUp()
        shutil.rmtree(self.package); shutil.copytree(STAGED,self.package)
        self.sandbox.chmod(0o1777)
        self.tools=self.root/'trusted-tools'; self.tools.mkdir()
        (self.tools/'observe.py').write_text(OBSERVER)
        wrapper=self.tools/'dirname'
        wrapper.write_text('#!/bin/sh\nfor last do :; done\ncase "$last" in /tmp/anamorph-desktop-helper.*/desktop-integration.sh)\n /usr/bin/python3 /native-tools/observe.py "$last" "$PPID" || exit $?;;\nesac\nexec /native-real/dirname "$@"\n')
        wrapper.chmod(0o755)
        self.audit=self.root/'audit'; self.audit.mkdir()
        self.control=self.root/'control'; self.control.mkdir()
        self.snapshot_tmp=self.root/'snapshot-tmp'; self.snapshot_tmp.mkdir(); self.snapshot_tmp.chmod(0o1777)
        self.sudoers=self.root/'sudoers'
        self.sudoers.write_text('Defaults !requiretty\nDefaults logfile="/native-audit/sudo.log"\n#'+str(UID)+' ALL=(root) NOPASSWD: ALL\n')
        self.sudoers.chmod(0o440)

    def tearDown(self):
        result=self._outcome.result
        FAILURES_SO_FAR[:]=[{'test':str(case),'traceback':trace} for case,trace in result.failures+result.errors]
        write_progress()

    def invoke(self, script='install.sh', mode='user', xdg=None, root=False, extra=(), command=None, fault=None, implicit=False):
        if time.monotonic()-STARTED > 1800:
            raise RuntimeError('bounded 30-minute native installer budget exhausted; incomplete gate')
        # Preserve real ordinary ownership for fixtures created by this root
        # test supervisor. Private system fixtures and protection metadata remain root.
        for path in self.sandbox.iterdir():
            if path.name not in ('system-local','system-vst'): own_user_fixture(path)
        uid=0 if root else UID; gid=0 if root else GID
        argv=['bwrap','--unshare-pid','--unshare-net','--new-session','--die-with-parent',
              '--cap-drop','ALL','--cap-add','CAP_SETUID','--cap-add','CAP_SETGID',
              '--cap-add','CAP_DAC_OVERRIDE','--cap-add','CAP_CHOWN','--cap-add','CAP_FOWNER',
              '--uid',str(uid),'--gid',str(gid),
              '--ro-bind','/usr/bin','/usr/bin','--ro-bind','/usr/share','/usr/share',
              '--ro-bind','/etc','/etc','--ro-bind',str(self.sudoers),'/etc/sudoers',
              '--symlink','usr/bin','/bin','--symlink','usr/lib','/lib',
              '--ro-bind','/lib64','/lib64','--proc','/proc','--dev','/dev',
              '--tmpfs','/run','--dir','/var','--dir','/root',
              '--bind',str(self.snapshot_tmp),'/tmp','--bind',str(self.sandbox),'/sandbox',
              '--ro-bind',str(self.package),'/package with spaces',
              '--ro-bind',str(self.tools),'/native-tools','--bind',str(self.audit),'/native-audit',
              '--bind',str(self.control),'/native-control','--dir','/native-real',
              '--ro-bind','/usr/bin/dirname','/native-real/dirname',
              '--ro-bind',str(self.tools/'dirname'),'/usr/bin/dirname',
              '--setenv','HOME','/sandbox/'+self.home_name,'--unsetenv','XDG_DATA_HOME',
              '--setenv','PATH','/usr/bin:/bin','--chdir','/tmp']
        for item in Path('/usr/lib').iterdir():
            if item.name!='vst3': argv += ['--ro-bind',str(item),str(item)]
        argv += ['--bind',str(self.system_vst),'/usr/lib/vst3','--bind',str(self.system_local),'/usr/local']
        # Fixed production PATH stays intact. Bind reviewed fault utilities over
        # their exact private namespace paths and keep originals at /native-real.
        if fault:
            for tool in self.fault.iterdir():
                text=tool.read_text().replace('/usr/bin/'+tool.name,'/native-real/'+tool.name)
                prepared=self.tools/('fault-'+tool.name); prepared.write_text(text); prepared.chmod(0o755)
                argv += ['--ro-bind','/usr/bin/'+tool.name,'/native-real/'+tool.name,
                         '--ro-bind',str(prepared),'/usr/bin/'+tool.name]
            argv += ['--setenv','ANAMORPH_FAULT',fault]
        if xdg is not None: argv += ['--setenv','XDG_DATA_HOME',xdg]
        if command:
            argv += [s.replace('/package/','/package with spaces/') for s in command]
        else:
            argv += ['/bin/sh','/package with spaces/'+script,*([] if implicit else ['--'+mode]),*extra]
        active={'test':self.id(),'entry':script,'mode':mode,'entry_uid':uid,'fault':fault}
        write_progress(active)
        try:
            result=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=45)
        except subprocess.TimeoutExpired:
            OPERATIONS.append({**active,'error':'test-owned installer subprocess exceeded 45 seconds'})
            write_progress(); raise
        recorded=self.audit/'snapshots.jsonl'
        rows=[json.loads(line) for line in recorded.read_text().splitlines()] if recorded.exists() else []
        OBSERVATIONS.extend(rows); recorded.unlink(missing_ok=True)
        self.assertEqual(list(self.snapshot_tmp.glob('anamorph-desktop-helper.*')),[],
                         'protected snapshot was not cleaned after the test-owned operation')
        OPERATIONS.append({'test':self.id(),'entry':script,'mode':('system' if implicit and root else mode),'entry_uid':uid,
                           'implicit':implicit,'fault':fault,'exit_code':result.returncode,
                           'protected_snapshots':len(rows),'actual_per_operation_sudo':uid!=0 and mode=='system'})
        write_progress()
        return result

    def test_real_sudo_and_root_selection(self):
        # All three actions pass real shell/metadata observation. Both install
        # entry forms perform preflight + publication, and uninstall is observed.
        for already_root in (False,True):
            with self.subTest(already_root=already_root):
                before=len(OBSERVATIONS)
                self.good(mode='system',root=already_root)
                data=self.system_local/'share'
                for relative in legacy.MANAGED_PATHS:
                    resource=data/relative
                    self.assertEqual(resource.stat().st_uid,0)
                    self.assertEqual(resource.stat().st_mode & 0o7777,0o644)
                receipt=data/'.tech.rolly.anamorph/receipt'
                self.assertEqual(receipt.stat().st_uid,0)
                self.assertEqual((self.system_local/'bin/Anamorph').stat().st_uid,0)
                self.assertEqual((self.system_local/'bin/Anamorph').read_bytes(),(self.package/'Anamorph').read_bytes())
                self.assertEqual((self.system_vst/'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').read_bytes(),
                                 (self.package/'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').read_bytes())
                self.good(mode='system',root=already_root)
                self.good(script='uninstall.sh',mode='system',root=already_root)
                seen=OBSERVATIONS[before:]
                self.assertEqual([x['action'] for x in seen],['preflight','install','preflight','install','uninstall'])
                self.assertEqual(list(self.snapshot_tmp.glob('anamorph-desktop-helper.*')),[])
                if not already_root:
                    log=(self.audit/'sudo.log').read_text()
                    self.assertIn('anamorph-desktop-helper',log)
                    self.assertIn('COMMAND=',log)
        result=self.invoke(root=True,implicit=True); self.assertEqual(result.returncode,0,result.stderr)
        self.assertTrue((self.system_local/'share'/legacy.DESKTOP).is_file())
        result=self.invoke(script='uninstall.sh',root=True,implicit=True); self.assertEqual(result.returncode,0,result.stderr)
        self.bad(root=True,message='--user cannot')
        self.bad(root=True,script='uninstall.sh',message='--user cannot')

    def test_real_snapshot_signal_cleanup_and_status(self):
        for sig,code in [('TERM',143),('HUP',129),('INT',130)]:
            with self.subTest(signal=sig):
                (self.control/'signal').write_text(sig)
                result=self.invoke(mode='system',root=True)
                self.assertEqual(result.returncode,code,result.stdout+result.stderr)
                self.assertFalse((self.control/'signal').exists(),'signal cut was not reached')
                self.assertEqual(list(self.snapshot_tmp.glob('anamorph-desktop-helper.*')),[])
                self.good(mode='system',root=True)
                self.good(script='uninstall.sh',mode='system',root=True)

    def test_real_system_receiptless_all_ten_paths(self):
        for relative in legacy.MANAGED_PATHS:
            for kind in ('regular','link','dangling-link'):
                with self.subTest(relative=relative,kind=kind):
                    app=self.system_local/'bin/Anamorph'; app.parent.mkdir(parents=True,exist_ok=True); app.write_bytes(b'existing app')
                    vst=self.system_vst/'Anamorph.vst3'; vst.mkdir(); (vst/'module').write_bytes(b'existing module')
                    target=self.system_local/'benign-target'; target.write_bytes(b'keep')
                    dest=self.system_local/'share'/relative; dest.parent.mkdir(parents=True,exist_ok=True)
                    if kind=='regular': dest.write_bytes(b'foreign resource')
                    else: dest.symlink_to('/usr/local/'+('benign-target' if kind=='link' else 'absent-target'))
                    self.bad(script='uninstall.sh',mode='system',root=True,message='without ownership receipt')
                    self.assertEqual(app.read_bytes(),b'existing app'); self.assertEqual((vst/'module').read_bytes(),b'existing module')
                    self.assertEqual(target.read_bytes(),b'keep')
                    dest.unlink(); self.good(script='uninstall.sh',mode='system',root=True); self.good(script='uninstall.sh',mode='system',root=True)
                    self.assertFalse(app.exists()); self.assertFalse(vst.exists())

    def test_clean_archive_and_required_assets(self):
        super().test_clean_archive_and_required_assets()
        archive=self.root/'customer.zip'
        with zipfile.ZipFile(archive) as zipped:
            entries={}
            for info in zipped.infolist():
                source=STAGED/info.filename
                self.assertEqual(zipped.read(info),source.read_bytes())
                self.assertEqual((info.external_attr >> 16) & 0o7777,source.stat().st_mode & 0o7777)
                entries[info.filename]={'sha256':hashlib.sha256(zipped.read(info)).hexdigest(),
                                        'mode':oct((info.external_attr >> 16) & 0o7777)}
        ARCHIVES.append({'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
                         'flat_layout':True,'entries':entries,'source_and_mode_equality':True})

    def test_gio_exec_escaping_and_hicolor_lookup(self):
        # The existing ordinary-only GIO launch fixture is separate from native
        # system installation of real project artifacts; its marker is not a DAW.
        (self.package/'Anamorph').write_text('#!/bin/sh\nprintf "%s\\n" "$0" > "$HOME/launcher-ran"\n')
        super().test_gio_exec_escaping_and_hicolor_lookup()


def main():
    global UID,GID,STAGED,PROGRESS_PATH,STARTED
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged-dir',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    args.report.parent.mkdir(parents=True,exist_ok=True)
    try: UID,GID=require_disposable_runner()
    except RuntimeError as error:
        args.report.write_text(json.dumps({'status':'incomplete','customer_archive_eligible':False,
                                          'missing_environment_capability':str(error)},indent=2)+'\n')
        raise
    STAGED=args.staged_dir.resolve(); legacy.STAGED=STAGED
    PROGRESS_PATH=args.report.resolve(); STARTED=time.monotonic(); write_progress()
    # The source/staged gate authenticates every script and approved asset before
    # any native installation. The archive round trip is a separate actual test.
    subprocess.run([sys.executable,str(ROOT/'scripts/check-linux-desktop.py'),'--staged-dir',str(STAGED)],check=True,stdout=subprocess.DEVNULL)
    assert (STAGED/'Anamorph').read_bytes()[:4]==b'\x7fELF'
    assert (STAGED/'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').read_bytes()[:4]==b'\x7fELF'
    assert hashlib.sha256((STAGED/'desktop-integration.sh').read_bytes()).hexdigest()==PIN
    for entry in ('install.sh','uninstall.sh'):
        assert 'expected='+PIN in (STAGED/entry).read_text()
    # Inherited methods create additional cases through this module binding.
    legacy.InstallCase=NativeCase
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(NativeCase)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    report={'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
            'failure_details':[{'test':str(case),'traceback':trace} for case,trace in result.failures+result.errors],
            'status':'verified' if result.wasSuccessful() else 'failed','customer_archive_eligible':result.wasSuccessful(),
            'ordinary_invoking_uid':UID,'root_metadata_mocked':False,'interpreter_replaced':False,
            'isolation':'real UIDs in root-created Bubblewrap mount/PID/network namespaces; no user namespace',
            'kernel_security_settings_changed':False,'cleanup_fault_cases':legacy.CLEANUP_RESULTS,
            'snapshot_observations':OBSERVATIONS,'operations':OPERATIONS,'archive_round_trips':ARCHIVES,
            'source_head':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
            'staged_files':{str(p.relative_to(STAGED)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'mode':oct(p.stat().st_mode & 0o7777)} for p in STAGED.rglob('*') if p.is_file()},
            'GUI_desktop_certification':False,'distinct_principal_exploit_proven':False}
    args.report.parent.mkdir(parents=True,exist_ok=True); args.report.write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)

if __name__=='__main__': main()
