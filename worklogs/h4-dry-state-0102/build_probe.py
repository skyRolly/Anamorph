#!/usr/bin/env python3
"""Relink a nonshipping probe with freshly compiled selected production TUs.

First build AnamorphTests in --build from --checkout, with the repository JUCE pin.
For diagnostic variants, pass an isolated checkout based on that same source SHA.
Unchanged CMake objects are reused; every replaced source and binary is hashed.
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
p.add_argument('--source', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--base-source', type=Path, required=True)
p.add_argument('--engine', type=Path)
a = p.parse_args()
for key in ('checkout', 'build', 'source', 'out', 'base_source'):
    setattr(a, key, getattr(a, key).resolve())
a.out.mkdir(parents=True, exist_ok=True)
commands = subprocess.check_output(['ninja', '-C', str(a.build), '-t', 'commands', 'AnamorphTests'], text=True).splitlines()
link = next(shlex.split(x) for x in commands if ' -o AnamorphTests_artefacts/Release/AnamorphTests ' in x)
if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
    link = link[2:-2]
record = {'source_sha': subprocess.check_output(['git', '-C', str(a.checkout), 'rev-parse', 'HEAD'], text=True).strip(), 'files': {}, 'commands': []}
for original, src in [('tests/dsp_tests.cpp', a.source), ('src/dsp/AnamorphEngine.cpp', a.engine.resolve() if a.engine else a.checkout/'src/dsp/AnamorphEngine.cpp'), ('src/dsp/MultibandWidth.cpp', a.checkout/'src/dsp/MultibandWidth.cpp'), ('src/dsp/MonoMaker.cpp', a.checkout/'src/dsp/MonoMaker.cpp'), ('src/dsp/SoloMonitor.cpp', a.checkout/'src/dsp/SoloMonitor.cpp')]:
    command = next(shlex.split(x) for x in commands if x.endswith('/'+original) and ' -c ' in x)
    old_obj = command[command.index('-o')+1]
    command = [x.replace(str(a.base_source), str(a.checkout)) for x in command]
    obj = a.out/(Path(original).stem+'.o')
    for flag, value in [('-o', str(obj)), ('-c', str(src)), ('-MF', str(obj)+'.d'), ('-MT', str(obj))]:
        if flag in command:
            command[command.index(flag)+1] = value
    subprocess.run(command, cwd=a.build, check=True)
    record['files'][str(src)] = hashlib.sha256(src.read_bytes()).hexdigest()
    record['commands'].append(command)
    link = [str(obj) if x == old_obj else x for x in link]
binary = a.out/'probe'
link[link.index('-o')+1] = str(binary)
subprocess.run(link, cwd=a.build, check=True)
record['commands'].append(link)
record['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
(a.out/'identity.json').write_text(json.dumps(record, indent=2)+'\n')
