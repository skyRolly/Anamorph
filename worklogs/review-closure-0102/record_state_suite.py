#!/usr/bin/env python3
"""One-off State suite execution against the recorded final fresh-build receipt.

The shared source/JUCE guard is used before and after. All generated rules,
State link objects and the State executable are checked against the committed
receipt. Trusted local tools/system dependencies and no concurrent writers are
premises. This record is scoped to this experiment, not a general attestation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

AUDIT = Path('/workspace/anamorph-evidence/worklogs/dsp-numerical-audit-0102')
sys.path.insert(0, str(AUDIT))
from replay_provenance import JUCE, cache_values, verify_source_tree
from rebuild_probe import sha, version
from replay_experiment import blob
from relink_verified import verify_build_environment

SOURCE = '3669cf716b27de56dd125f29d0b2672598aebdc2'
TREE = '61c0fa72b2dd0ef59ff96a34b8068f3f7c06c0b3'
SCRIPT = 'worklogs/review-closure-0102/record_state_suite.py'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True)
    parser.add_argument('--juce', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--receipt-commit', required=True)
    parser.add_argument('--receipt-path', required=True)
    parser.add_argument('--script-commit', required=True)
    args = parser.parse_args()
    checkout, juce, build, out = (p.resolve() for p in (args.checkout,args.juce,args.build,args.out))
    if out.exists() or any(out.is_relative_to(p) for p in (checkout,juce,build)):
        raise ValueError('output must be new and outside source, JUCE and build')
    script = Path(__file__).resolve()
    script_bytes = blob(args.script_commit,SCRIPT)
    if script.read_bytes() != script_bytes:
        raise ValueError('command record differs from its committed bytes')
    receipt_bytes = blob(args.receipt_commit,args.receipt_path)
    receipt = json.loads(receipt_bytes)
    if (receipt['source_head'] != SOURCE or receipt['source_tree'] != TREE
            or receipt['patch_sha256'] is not None or receipt['juce_head'] != JUCE):
        raise ValueError('receipt is not for the selected clean source and dependency')
    if (Path(receipt['checkout']).resolve()!=checkout or Path(receipt['juce']).resolve()!=juce
            or Path(receipt['build_directory']).resolve()!=build):
        raise ValueError('receipt identifies another source, dependency or build')
    target='AnamorphStateTests'; recorded=receipt['targets_detail'][target]
    binary=build/(target+'_artefacts/Release/'+target)
    tool_paths=[script,AUDIT/'replay_provenance.py',AUDIT/'rebuild_probe.py',
                AUDIT/'replay_experiment.py',AUDIT/'relink_verified.py']
    tool_hashes={str(path):sha(path) for path in tool_paths}

    def verify():
        verify_build_environment(receipt['environment'])
        if verify_source_tree(checkout,SOURCE)!=TREE or verify_source_tree(juce,JUCE)!=receipt['juce_tree']:
            raise ValueError('source or JUCE differs from the recorded build')
        cache=cache_values(build)
        if (Path(cache.get('CMAKE_HOME_DIRECTORY','/nonexistent')).resolve()!=checkout
                or Path(cache.get('ANAMORPH_JUCE_PATH','/nonexistent')).resolve()!=juce):
            raise ValueError('CMake cache points to another source or dependency')
        for filename,key in [('CMakeCache.txt','cache_sha256'),('build.ninja','rules_sha256'),
                             ('compile_commands.json','compile_commands_sha256')]:
            if sha(build/filename)!=receipt[key]: raise ValueError('generated input changed: '+filename)
        if version(cache['CMAKE_CXX_COMPILER'])!=receipt['compiler_version']:
            raise ValueError('compiler version differs from receipt')
        commands=subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
        if commands!=recorded['ninja_commands']: raise ValueError('State build commands changed')
        for filename,digest in recorded['link_objects_sha256'].items():
            obj=Path(filename).resolve()
            if not obj.is_relative_to(build) or not obj.is_file() or sha(obj)!=digest:
                raise ValueError('State link object changed: '+filename)
        if sha(binary)!=recorded['binary_sha256']: raise ValueError('State executable changed')
        if any(sha(path)!=digest for path,digest in tool_hashes.items()):
            raise ValueError('execution tool changed')

    verify()
    out.mkdir(parents=True)
    command=['xvfb-run','-a','python3',str(checkout/'tests/state_isolation_tests.py'),
             '--full-suite','--',str(binary)]
    result=subprocess.run(command,cwd=out,capture_output=True)
    (out/'stdout').write_bytes(result.stdout);(out/'stderr').write_bytes(result.stderr)
    verify()
    summary=re.findall(rb'(?m)^\s*(\d+) checks, (\d+) failure\(s\)\s*$',result.stdout)
    isolated=b'State filesystem isolation: ' in result.stdout and b'controls passed' in result.stdout
    valid=result.returncode==0 and summary and tuple(map(int,summary[-1]))==(16555,0) and isolated
    identity={'source_head':SOURCE,'source_tree':TREE,'juce_head':JUCE,'juce_tree':receipt['juce_tree'],
        'receipt_commit':args.receipt_commit,'receipt_path':args.receipt_path,
        'receipt_sha256':hashlib.sha256(receipt_bytes).hexdigest(),'script_commit':args.script_commit,
        'tool_sha256':tool_hashes,'build':str(build),'command':command,'cwd':str(out),
        'binary_sha256':sha(binary),'exit':result.returncode,'summary':list(map(int,summary[-1])) if summary else None,
        'isolation_pass_line':bool(isolated),'stdout_sha256':sha(out/'stdout'),'stderr_sha256':sha(out/'stderr')}
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    print('State suite:',identity['summary'],'isolation:',bool(isolated),'exit:',result.returncode)
    return 0 if valid else 1


if __name__=='__main__':
    raise SystemExit(main())
