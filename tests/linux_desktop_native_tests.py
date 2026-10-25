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
SIGNALS = []
ENVIRONMENT_PREFLIGHT = {}
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
 'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'actual_shell_argv':args,'action':args[2],
 'user_namespace':os.readlink('/proc/self/ns/user')}
with open('/native-audit/snapshots.jsonl','a') as stream: stream.write(json.dumps(record)+'\n')
control=Path('/native-control/signal')
if control.exists() and args[2]=='install':
 sig=control.read_text().strip();target=parent;ancestry=[];bootstrap=None
 for _ in range(8):
  stat=Path('/proc/'+str(target)+'/stat').read_text()
  target=int(stat[stat.rfind(')')+2:].split()[1])
  if target<=1:break
  cmd=Path('/proc/'+str(target)+'/cmdline').read_bytes().split(b'\0')
  ancestry.append([value.decode(errors='replace') for value in cmd if value])
  if len(cmd)>2 and cmd[1]==b'-c' and b'expected=3ea641f74a7a04abd2cffeac4298e54022c078655593ad45d226d622ece3c45e' in cmd[2] and b'snapshot=$(mktemp' in cmd[2]:
   assert Path('/proc/'+str(target)).stat().st_uid==0
   bootstrap=target;break
 assert bootstrap is not None,ancestry
 record['bootstrap_signal']=sig;record['bootstrap_ancestry']=ancestry
 os.kill(bootstrap,getattr(signal,'SIG'+sig));control.unlink()
 with open('/native-audit/signals.jsonl','a') as stream:stream.write(json.dumps(record)+'\n')
'''


def write_progress(active=None):
    if PROGRESS_PATH is None: return
    data={'status':'in_progress','customer_archive_eligible':False,
          'elapsed_seconds':round(time.monotonic()-STARTED,3),
          'active_operation':active,'completed_operations':OPERATIONS,
          'snapshot_observations':OBSERVATIONS,'signal_observations':SIGNALS,
          'native_environment_preflight':ENVIRONMENT_PREFLIGHT,'cleanup_fault_cases':legacy.CLEANUP_RESULTS,
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
    if not all(shutil.which(tool) for tool in ('unshare','mount','setpriv','sudo')):
        raise RuntimeError('native private-root gate requires unshare, mount, setpriv and real sudo')
    return uid,gid


def own_user_fixture(path):
    # Fixture preparation uses real chown in task-owned paths, not a stat mock.
    # Never follow benign symlink fixtures or touch the system mounts.
    for directory, dirs, files in os.walk(path,followlinks=False):
        os.chown(directory,UID,GID,follow_symlinks=False)
        for name in dirs+files: os.chown(Path(directory)/name,UID,GID,follow_symlinks=False)


def enter_private_namespace(config_path):
    """Prepare only task-owned mounts in this new namespace, then exec real UID."""
    require_disposable_runner()
    config_path=Path(config_path)
    meta=config_path.stat()
    if config_path.is_symlink() or meta.st_uid!=0 or meta.st_mode & 0o777!=0o600:
        raise RuntimeError('unprotected native namespace configuration')
    config=json.loads(config_path.read_text()); root=Path(config['root'])
    if os.getpid()!=1 or any(os.readlink('/proc/self/ns/'+name)==config['parent_namespaces'][name] for name in ('mnt','pid','net')):
        raise RuntimeError('native setup requires distinct task mount/PID/network namespaces')
    if os.readlink('/proc/self/ns/user')!=config['parent_namespaces']['user']:
        raise RuntimeError('native ownership must remain in the real user namespace')
    if root.is_symlink() or root.stat().st_uid!=0 or config_path.parent!=root.parent:
        raise RuntimeError('namespace root must belong to this task')
    def checked(*args): subprocess.run(args,check=True,stdout=subprocess.DEVNULL)
    # This acts only on the new mount namespace; host propagation/settings stay intact.
    checked('mount','--make-rprivate','/')
    for directory in ('usr','usr/lib','usr/local','usr/lib/vst3','run','var','root','tmp','proc','dev','native-real'):
        (root/directory).mkdir(parents=True,exist_ok=True)
    def bind(source,target,readonly):
        source=Path(source); relative=Path(target).relative_to('/')
        if '..' in relative.parts: raise RuntimeError('invalid namespace target')
        destination=root/relative; destination.parent.mkdir(parents=True,exist_ok=True)
        if source.is_dir(): destination.mkdir(exist_ok=True)
        elif not destination.exists(): destination.touch()
        checked('mount','--bind',str(source),str(destination))
        if readonly: checked('mount','-o','remount,bind,ro',str(destination))
    for source,target,readonly in config['bindings']: bind(source,target,readonly)
    for name,target in (('bin','usr/bin'),('sbin','usr/sbin'),('lib','usr/lib')):
        path=root/name
        if not path.is_symlink(): path.symlink_to(target)
    checked('mount','-t','proc','-o','nosuid,nodev,noexec','proc',str(root/'proc'))
    for name in ('null','zero','random','urandom'): bind('/dev/'+name,'/dev/'+name,False)
    (root/'dev/pts').mkdir(exist_ok=True)
    checked('mount','-t','devpts','-o','newinstance,ptmxmode=0666,mode=0620','devpts',str(root/'dev/pts'))
    for name,target in (('ptmx','pts/ptmx'),('fd','/proc/self/fd'),('stdin','/proc/self/fd/0'),('stdout','/proc/self/fd/1'),('stderr','/proc/self/fd/2')):
        path=root/'dev'/name
        if not path.is_symlink():path.symlink_to(target)
    # Keep a dedicated namespace init. The customer entry/bootstrap must not be
    # PID 1, whose signal rules differ; exiting init also reaps/kills its orphans.
    os.chroot(root); os.chdir('/tmp')
    child=os.fork()
    if child:
        # The live-PID refusal fixture must refer to an init owned by the same
        # real account as the entry process, as in an ordinary user namespace.
        os.setgroups([]);os.setgid(config['gid']);os.setuid(config['uid'])
        _,status=os.waitpid(child,0)
        code=os.waitstatus_to_exitcode(status)
        os._exit(code if code>=0 else 128-code)
    os.environ.pop('XDG_DATA_HOME',None);os.environ.pop('ANAMORPH_FAULT',None)
    os.environ.update(config['environment'])
    # No user namespace and no no-new-privs flag: genuine setuid sudo is required.
    # Drop mount/namespace capabilities before the project installer is started.
    argv=['/usr/bin/setpriv','--bounding-set=-all,+setuid,+setgid,+dac_override,+chown,+fowner,+audit_write',
          '--inh-caps=-all','--ambient-caps=-all','--reuid',str(config['uid']),
          '--regid',str(config['gid']),'--clear-groups','--',*config['command']]
    os.execv(argv[0],argv)


def production_bootstrap():
    text=(STAGED/'install.sh').read_text()
    match=re.search(r"    priv /bin/sh -c '([\s\S]*?)' anamorph-desktop-helper",text)
    if not match or 'expected='+PIN not in match[1]:raise RuntimeError('cannot select reviewed production bootstrap')
    return match[1]



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
        bindings=[('/usr/bin','/usr/bin',True),('/usr/sbin','/usr/sbin',True),
                  ('/usr/share','/usr/share',True),('/etc','/etc',True),
                  (str(self.sudoers),'/etc/sudoers',True),('/lib64','/lib64',True),
                  (str(self.snapshot_tmp),'/tmp',False),(str(self.sandbox),'/sandbox',False),
                  (str(self.package),'/package with spaces',True),
                  (str(self.tools),'/native-tools',True),(str(self.audit),'/native-audit',False),
                  (str(self.control),'/native-control',False),
                  ('/usr/bin/dirname','/native-real/dirname',True),
                  (str(self.tools/'dirname'),'/usr/bin/dirname',True)]
        # Preserve the complete read-only runtime (including sudo/GIO libexec)
        # while shadowing only the two product-owned system destinations.
        for item in Path('/usr').iterdir():
            if item.name not in ('bin','sbin','share','lib','local'):
                bindings.append((str(item),str(item),True))
        for item in Path('/usr/lib').iterdir():
            if item.name!='vst3': bindings.append((str(item),str(item),True))
        bindings += [(str(self.system_vst),'/usr/lib/vst3',False),(str(self.system_local),'/usr/local',False)]
        environment={'HOME':'/sandbox/'+self.home_name,'PATH':'/usr/bin:/bin'}
        if fault:
            for tool in self.fault.iterdir():
                text=tool.read_text().replace('/usr/bin/'+tool.name,'/native-real/'+tool.name)
                prepared=self.tools/('fault-'+tool.name);prepared.write_text(text);prepared.chmod(0o755)
                bindings += [('/usr/bin/'+tool.name,'/native-real/'+tool.name,True),
                             (str(prepared),'/usr/bin/'+tool.name,True)]
            environment['ANAMORPH_FAULT']=fault
        if xdg is not None:environment['XDG_DATA_HOME']=xdg
        payload=([s.replace('/package/','/package with spaces/') for s in command] if command else
                 ['/bin/sh','/package with spaces/'+script,*([] if implicit else ['--'+mode]),*extra])
        namespace_root=self.root/'namespace-root';namespace_root.mkdir(exist_ok=True)
        config=self.root/'namespace-config.json'
        config.write_text(json.dumps({'root':str(namespace_root),'bindings':bindings,'environment':environment,
                                      'uid':uid,'gid':gid,'command':payload,
                                      'parent_namespaces':{name:os.readlink('/proc/self/ns/'+name) for name in ('mnt','pid','net','user')}}));config.chmod(0o600)
        argv=['unshare','--mount','--pid','--net','--fork','--kill-child',
              sys.executable,str(Path(__file__).resolve()),'--namespace-config',str(config)]
        kind=('customer_entry' if command is None else 'embedded_bootstrap' if command[:2]==['/bin/sh','-c'] else 'ordinary_fixture_probe')
        active={'test':self.id(),'entry':script,'mode':mode,'entry_uid':uid,'fault':fault,'execution_kind':kind}
        write_progress(active)
        try:
            result=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=45)
        except subprocess.TimeoutExpired:
            OPERATIONS.append({**active,'error':'test-owned installer subprocess exceeded 45 seconds'})
            write_progress(); raise
        recorded=self.audit/'snapshots.jsonl'
        rows=[json.loads(line) for line in recorded.read_text().splitlines()] if recorded.exists() else []
        for row in rows:
            self.assertEqual(row['user_namespace'],os.readlink('/proc/self/ns/user'))
        OBSERVATIONS.extend(rows); recorded.unlink(missing_ok=True)
        signals=self.audit/'signals.jsonl'
        if signals.exists():
            SIGNALS.extend(json.loads(line) for line in signals.read_text().splitlines());signals.unlink()
        self.assertEqual(list(self.snapshot_tmp.glob('anamorph-desktop-helper.*')),[],
                         'protected snapshot was not cleaned after the test-owned operation')
        OPERATIONS.append({**active,'mode':('system' if implicit and root else mode),'entry_uid':uid,
                           'implicit':implicit,'fault':fault,'exit_code':result.returncode,
                           'protected_snapshots':len(rows),'actual_per_operation_sudo':command is None and uid!=0 and mode=='system'})
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
            for direct in (False,True):
                with self.subTest(signal=sig,direct_bootstrap=direct):
                    self.good(mode='system',root=True)
                    (self.control/'signal').write_text(sig)
                    command=(['/bin/sh','-c',production_bootstrap(),'anamorph-desktop-helper',
                              '/package with spaces/desktop-integration.sh','/package with spaces/desktop-assets',
                              'install','/usr/local/share','/usr/local/bin/Anamorph'] if direct else None)
                    result=self.invoke(mode='system',root=True,command=command)
                    # The customer's existing priv() maps errors to 1; the exact
                    # embedded bootstrap preserves its own signal status.
                    self.assertEqual(result.returncode,code if direct else 1,result.stdout+result.stderr)
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
    if sys.argv[1:2]==['--namespace-config']:
        if len(sys.argv)!=3: raise RuntimeError('one protected namespace configuration required')
        enter_private_namespace(sys.argv[2]);return
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
    preflight=NativeCase();preflight.setUp()
    try:
        for label,root_user,command,expected in [
            ('ordinary_uid',False,['/usr/bin/id','-u'],UID),
            ('root_uid',True,['/usr/bin/id','-u'],0),
            ('real_sudo_uid',False,['/usr/bin/sudo','-n','/usr/bin/id','-u'],0)]:
            result=preflight.invoke(root=root_user,command=command)
            if result.returncode or result.stdout.strip()!=str(expected):
                detail={'status':'incomplete','customer_archive_eligible':False,
                        'missing_environment_capability':label,
                        'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
                args.report.write_text(json.dumps(detail,indent=2)+'\n')
                raise RuntimeError('native private-root capability unavailable: '+label)
            ENVIRONMENT_PREFLIGHT[label]=int(result.stdout.strip())
    finally:preflight.doCleanups()
    # Inherited methods create additional cases through this module binding.
    legacy.InstallCase=NativeCase
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(NativeCase)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    report={'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
            'failure_details':[{'test':str(case),'traceback':trace} for case,trace in result.failures+result.errors],
            'status':'verified' if result.wasSuccessful() else 'failed','customer_archive_eligible':result.wasSuccessful(),
            'ordinary_invoking_uid':UID,'root_metadata_mocked':False,'interpreter_replaced':False,
            'isolation':'real UIDs in unshare mount/PID/network namespaces with task-private chroot; no user namespace',
            'kernel_security_settings_changed':False,'cleanup_fault_cases':legacy.CLEANUP_RESULTS,
            'snapshot_observations':OBSERVATIONS,'signal_observations':SIGNALS,
            'native_environment_preflight':ENVIRONMENT_PREFLIGHT,
            'operations':OPERATIONS,'archive_round_trips':ARCHIVES,
            'source_head':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
            'staged_files':{str(p.relative_to(STAGED)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'mode':oct(p.stat().st_mode & 0o7777)} for p in STAGED.rglob('*') if p.is_file()},
            'GUI_desktop_certification':False,'distinct_principal_exploit_proven':False}
    args.report.parent.mkdir(parents=True,exist_ok=True); args.report.write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)

if __name__=='__main__': main()
