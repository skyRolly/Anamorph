#!/usr/bin/env python3
"""Replay N2-02 controls using a fresh corrected CMake build (JUCE 9.0.5).

Build both test targets first. Recompile ONLY the engine for negative controls;
reuse the exact corrected test objects, headers, flags and unchanged libraries.
"""
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--checkout', type=Path, required=True)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
for key in ('checkout', 'build', 'baseline', 'out'):
    setattr(a, key, getattr(a, key).resolve())
a.out.mkdir(parents=True, exist_ok=True)
baseline_sha = subprocess.check_output(['git', '-C', str(a.baseline), 'rev-parse', 'HEAD'], text=True).strip()
if baseline_sha != '113b9189f0ff4d3b6348c553c559c22ee8ade782':
    raise SystemExit('Negative control requires the recorded original source SHA')
source = (a.checkout/'src/dsp/AnamorphEngine.cpp').read_text()
needle = '        if (! pendingForced)\n        {'
assert source.count(needle) == 1
mutation = a.out/'premature-forced.cpp'
mutation.write_text(source.replace(needle, '        if (true) // negative control: merge before forced bottom\n        {'))
record = {}
for target in ('AnamorphTests', 'AnamorphStateTests'):
    commands = subprocess.check_output(['ninja', '-C', str(a.build), '-t', 'commands', target], text=True).splitlines()
    cc = next(shlex.split(x) for x in commands if x.endswith('/src/dsp/AnamorphEngine.cpp') and ' -c ' in x)
    link = next(shlex.split(x) for x in commands if ' -o '+target+'_artefacts/Release/'+target+' ' in x)
    if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
        link = link[2:-2]
    original_obj = cc[cc.index('-o')+1]
    for variant, src in [('fixed', a.checkout/'src/dsp/AnamorphEngine.cpp'), ('original', a.baseline/'src/dsp/AnamorphEngine.cpp'), ('premature', mutation)]:
        if target == 'AnamorphStateTests' and variant == 'premature':
            continue  # forced ownership is asserted in the engine test
        stem = target+'-'+variant
        obj = a.out/(stem+'.o')
        compile_cmd = cc.copy()
        for flag, value in [('-o', str(obj)), ('-c', str(src)), ('-MF', str(obj)+'.d'), ('-MT', str(obj))]:
            if flag in compile_cmd:
                compile_cmd[compile_cmd.index(flag)+1] = value
        subprocess.run(compile_cmd, cwd=a.build, check=True)
        binary = a.out/stem
        link_cmd = [str(obj) if x == original_obj else x for x in link]
        link_cmd[link_cmd.index('-o')+1] = str(binary)
        subprocess.run(link_cmd, cwd=a.build, check=True)
        result = subprocess.run([str(binary), '--n2-controls'], capture_output=True, text=True)
        (a.out/(stem+'.stdout')).write_text(result.stdout)
        (a.out/(stem+'.stderr')).write_text(result.stderr)
        record[stem] = {'exit': result.returncode, 'source_sha256': hashlib.sha256(src.read_bytes()).hexdigest(), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'compile': compile_cmd, 'link': link_cmd}
        print(stem, result.returncode, result.stdout.splitlines()[-1] if result.stdout else '')
(a.out/'identity.json').write_text(json.dumps(record, indent=2)+'\n')
