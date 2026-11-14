import hashlib, json, os, subprocess, time
from pathlib import Path
root=Path('/workspace/anamorph-closure-integration')
out=Path('/workspace/review-closure-integration-3669-checks')
out.mkdir(exist_ok=True)
base='51e6250d34b44d838a51f9a5086105d39d6b0f29'
commands=[
('docs-self',['python3','-B','scripts/check-docs.py','--self-test']),
('docs',['python3','-B','scripts/check-docs.py']),
('citations-self',['python3','-B','scripts/check-citations.py','--self-test']),
('citations',['python3','-B','scripts/check-citations.py','--check','--base',base]),
('portability-self',['python3','-B','scripts/check-portability.py','--self-test']),
('portability',['python3','-B','scripts/check-portability.py']),
('realtime-self',['python3','-B','scripts/check-realtime.py','--self-test']),
('realtime',['python3','-B','scripts/check-realtime.py']),
('dispatch-self',['python3','-B','scripts/check-dispatch.py','--self-test']),
('dispatch',['python3','-B','scripts/check-dispatch.py']),
('state-coverage-self',['python3','-B','scripts/check-state-coverage.py','--self-test']),
('state-coverage',['python3','-B','scripts/check-state-coverage.py']),
('workflow-routing',['python3','-B','scripts/test-workflow-routing.py','--report',str(out/'workflow-routing.json')]),
('release-notes-self',['python3','-B','scripts/check-release-notes.py','--self-test']),
('release-notes',['python3','-B','scripts/check-release-notes.py']),
('rosetta-inert',['python3','-B','scripts/diagnose-rosetta-scope.py','--self-test','--self-test-report',str(out/'rosetta-inert.json')]),
('cleanup-handshake',['python3','-B','tests/rosetta_cleanup_handshake_tests.py']),
('cleanup-fault',['python3','-B','tests/rosetta_cleanup_fault_tests.py']),
('cleanup-process',['python3','-B','tests/rosetta_cleanup_process_tests.py','--report',str(out/'rosetta-process.json')]),
('rosetta-launch',['python3','-B','scripts/diagnose-rosetta-launch.py','--self-test','--report',str(out/'rosetta-launch.json')]),
('rosetta-trace',['python3','-B','scripts/diagnose-rosetta-handshake.py','--self-test','--report',str(out/'rosetta-trace.json')]),
('mac-review-fixture',['python3','-B','scripts/test-macos-integration-review.py','--self-test']),
('native-parser',['python3','-B','tests/native_validation_tests.py']),
('installer-plan',['python3','-B','tests/linux_install_native_plan_tests.py']),
('installer-shell',['/bin/sh','-n','packaging/linux/install.sh']),
('uninstaller-shell',['/bin/sh','-n','packaging/linux/uninstall.sh']),
('icons-self',['python3','-B','scripts/check-icons.py','--self-test']),
('icons',['python3','-B','scripts/check-icons.py']),
('icon-preflight',['python3','-B','scripts/test-icon-preflight.py']),
('installer-icon-self',['python3','-B','scripts/check-installer-icon.py','--self-test']),
('installer-icon',['python3','-B','scripts/check-installer-icon.py']),
('linux-package-self',['python3','-B','scripts/check-linux-package.py','--self-test']),
('linux-package',['python3','-B','scripts/check-linux-package.py']),
('identity-self',['python3','-B','scripts/check-product-identity.py','--self-test']),
('identity',['python3','-B','scripts/check-product-identity.py']),
]
results=[]
for name,cmd in commands:
 print('start',name,flush=True)
 start=time.monotonic()
 try:
  r=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
  data=r.stdout; status=r.returncode
 except subprocess.TimeoutExpired as e:
  data=(e.stdout or b'')+b'\nTIMEOUT\n';status=124
 log=out/(name+'.log');log.write_bytes(data)
 results.append({'name':name,'argv':cmd,'exit':status,'seconds':round(time.monotonic()-start,3),'log_sha256':hashlib.sha256(data).hexdigest()})
 (out/'results.json').write_text(json.dumps({'source_head':'3669cf716b27de56dd125f29d0b2672598aebdc2','results':results},indent=2)+'\n')
 print('done',name,status,flush=True)
print('failures',[(x['name'],x['exit']) for x in results if x['exit']],flush=True)
raise SystemExit(any(x['exit'] for x in results))
