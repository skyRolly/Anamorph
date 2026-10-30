#!/usr/bin/env python3
"""Actual sudo/root installer validation in disposable hosted-VM private roots."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('install_cases', ROOT / 'tests/linux_install_tests.py')
cases = importlib.util.module_from_spec(spec); spec.loader.exec_module(cases)
UID = GID = None
STAGED = None
STARTED = None
REPORT = None
OPERATIONS = []
NATIVE_REQUIRED_METHODS = (
    'test_actual_sudo_system_install_upgrade_uninstall',
    'test_system_killed_swap_and_parked_copy_recovery',
    'test_foreign_owner_system_resource_is_preserved',
)


def checkpoint(status='in_progress', **extra):
    if REPORT is None: return
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    data = {'status': status, 'customer_archive_eligible': False,
            'operations': OPERATIONS, 'kernel_security_settings_changed': False, **extra}
    pending = REPORT.with_suffix('.pending')
    pending.write_text(json.dumps(data, indent=2) + '\n'); pending.replace(REPORT)


class ProgressResult(unittest.TextTestResult):
    def startTest(self, test):
        super().startTest(test); checkpoint(active_test=test.id())
    def stopTest(self, test):
        super().stopTest(test)
        checkpoint(completed_tests=self.testsRun, failures=len(self.failures), errors=len(self.errors))


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


class NativeCase(cases.InstallCase):
    def setUp(self):
        super().setUp()
        self.sandbox.chmod(0o1777)
        self.tools = self.root / 'trusted-tools'; self.tools.mkdir()
        self.sudoers = self.root / 'sudoers'
        self.sudoers.write_text('Defaults !requiretty\n#' + str(UID) + ' ALL=(root) NOPASSWD: ALL\n')
        self.sudoers.chmod(0o440)

    def invoke(self, script='install.sh', mode='user', root=False, xdg=None,
               fault=None, command=None, extra=()):
        if time.monotonic() - STARTED > 1200:
            raise RuntimeError('native installer test budget exhausted; incomplete gate')
        for path in self.sandbox.iterdir():
            if path.name not in ('system-local', 'system-vst'): own_user_fixture(path)
        uid, gid = (0, 0) if root else (UID, GID)
        bindings = [('/usr/bin', '/usr/bin', True), ('/usr/sbin', '/usr/sbin', True),
                    ('/usr/share', '/usr/share', True), ('/etc', '/etc', True),
                    (str(self.sudoers), '/etc/sudoers', True), ('/lib64', '/lib64', True),
                    (str(self.sandbox), '/sandbox', False),
                    (str(self.package), '/package with spaces', True)]
        for item in Path('/usr').iterdir():
            if item.name not in ('bin', 'sbin', 'share', 'lib', 'local'):
                bindings.append((str(item), str(item), True))
        for item in Path('/usr/lib').iterdir():
            if item.name != 'vst3': bindings.append((str(item), str(item), True))
        bindings += [(str(self.system_vst), '/usr/lib/vst3', False),
                     (str(self.system_local), '/usr/local', False)]
        environment = {'HOME': '/sandbox/home', 'PATH': '/usr/bin:/bin'}
        if xdg is not None: environment['XDG_DATA_HOME'] = xdg
        if fault:
            for tool in self.fault.iterdir():
                prepared = self.tools / tool.name
                prepared.write_text(tool.read_text().replace('/usr/bin/' + tool.name, '/native-real/' + tool.name))
                prepared.chmod(0o755)
                bindings += [('/usr/bin/' + tool.name, '/native-real/' + tool.name, True),
                             (str(prepared), '/usr/bin/' + tool.name, True)]
            environment['ANAMORPH_FAULT'] = fault
        payload = ([value.replace('/package/', '/package with spaces/') for value in command] if command else
                   ['/bin/sh', '/package with spaces/' + script, '--' + mode, *extra])
        namespace_root = self.root / 'namespace-root'; namespace_root.mkdir(exist_ok=True)
        config = self.root / 'namespace-config.json'
        config.write_text(json.dumps({'root': str(namespace_root), 'bindings': bindings,
            'environment': environment, 'uid': uid, 'gid': gid, 'command': payload,
            'parent_namespaces': {name: os.readlink('/proc/self/ns/' + name)
                                  for name in ('mnt', 'pid', 'net', 'user')}}))
        config.chmod(0o600)
        argv = ['unshare', '--mount', '--pid', '--net', '--fork', '--kill-child',
                sys.executable, str(Path(__file__).resolve()), '--namespace-config', str(config)]
        checkpoint(active_test=self.id())
        result = subprocess.run(argv, capture_output=True, text=True, timeout=45)
        OPERATIONS.append({'entry': script, 'mode': mode, 'root_entry': root,
                           'actual_per_operation_sudo': command is None and mode == 'system' and not root,
                           'fault': fault, 'exit_code': result.returncode})
        checkpoint(active_test=self.id())
        return result

    def test_actual_sudo_system_install_upgrade_uninstall(self):
        self.good(mode='system'); self.assert_installed(system=True)
        self.seed_legacy(self.system_local / 'share')
        self.good(mode='system')
        for relative in cases.MANAGED: self.assertFalse((self.system_local / 'share' / relative).exists())
        self.good(script='uninstall.sh', mode='system')
        self.good(script='uninstall.sh', mode='system')
        self.assertFalse((self.system_vst / 'Anamorph.vst3').exists())

    def test_system_killed_swap_and_parked_copy_recovery(self):
        previous = self.seed_plugin(system=True)
        self.fault_tool('mv', '''case "$1" in */.anamorph-install-stage/Anamorph.vst3)
 if [ ! -e /sandbox/control/cut ]; then
  : > /sandbox/control/cut
  kill -KILL "$PPID"
  exit 1
 fi;; esac''')
        self.bad(mode='system', root=True, fault='kill')
        self.assertTrue((self.control / 'cut').exists())
        self.good(mode='system'); self.assert_installed(system=True)
        self.good(script='uninstall.sh', mode='system')
        self.assertFalse(previous.exists())

    def test_foreign_owner_system_resource_is_preserved(self):
        self.seed_plugin(system=True)
        state = self.seed_legacy(self.system_local / 'share')
        resource = self.system_local / 'share' / cases.PNG
        os.chown(resource, UID, GID)
        before = resource.read_bytes()
        result = self.good(mode='system', script='uninstall.sh')
        self.assertIn('warning: legacy Anamorph resources preserved', result.stderr)
        self.assertEqual(resource.read_bytes(), before)
        self.assertEqual(resource.stat().st_uid, UID)
        self.assertTrue((state / 'receipt').is_file())


def coverage(suite):
    selected = {test._testMethodName for test in suite}
    missing = set(cases.REQUIRED_METHODS + NATIVE_REQUIRED_METHODS) - selected
    if missing: raise RuntimeError('native suite omits mandatory installer cases: ' + ', '.join(sorted(missing)))
    return sorted(selected)


def coverage_liveness(suite, checker=coverage):
    selected = checker(suite)
    try: checker(unittest.TestSuite())
    except RuntimeError: pass
    else: raise RuntimeError('coverage verifier accepted an empty suite')
    return selected


def main():
    global UID, GID, STAGED, STARTED, REPORT
    if sys.argv[1:2] == ['--namespace-config']:
        enter_private_namespace(sys.argv[2]); return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged-dir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    REPORT = args.report.resolve(); STARTED = time.monotonic(); checkpoint()
    try:
        UID, GID = require_disposable_runner()
        STAGED = args.staged_dir.resolve(); cases.STAGED = STAGED
        subprocess.run([sys.executable, str(ROOT / 'scripts/check-linux-package.py'),
                        '--staged-dir', str(STAGED)], check=True, stdout=subprocess.DEVNULL)
        preflight = NativeCase(); preflight.setUp()
        try:
            for root_user, command, expected in [
                (False, ['/usr/bin/id', '-u'], UID), (True, ['/usr/bin/id', '-u'], 0),
                (False, ['/usr/bin/sudo', '-n', '/usr/bin/id', '-u'], 0)]:
                result = preflight.invoke(root=root_user, command=command)
                if result.returncode or result.stdout.strip() != str(expected):
                    raise RuntimeError('real UID/sudo private-root preflight failed: ' + result.stderr)
        finally: preflight.doCleanups()
        cases.InstallCase = NativeCase
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(NativeCase)
        selected = coverage_liveness(suite)
        result = unittest.TextTestRunner(verbosity=2, resultclass=ProgressResult).run(suite)
        checkpoint('verified' if result.wasSuccessful() else 'failed',
            customer_archive_eligible=result.wasSuccessful(), tests_run=result.testsRun,
            failures=len(result.failures), errors=len(result.errors), mandatory_cases=selected,
            root_metadata_mocked=False, interpreter_replaced=False,
            isolation='real UIDs and sudo in private mount/PID/network namespaces; no user namespace',
            source_head=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
            staged_files={str(path.relative_to(STAGED)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in STAGED.rglob('*') if path.is_file()})
        raise SystemExit(0 if result.wasSuccessful() else 1)
    except Exception as error:
        checkpoint('incomplete', error=str(error)); raise


if __name__ == '__main__': main()
