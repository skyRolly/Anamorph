#!/usr/bin/env python3
"""Generate isolated N2-03 call-site controls and replay exactly the same oracle."""
import argparse
import json
import subprocess
import sys
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--checkout', type=Path, required=True)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
a.checkout=a.checkout.resolve();a.build=a.build.resolve();a.out=a.out.resolve()
a.out.mkdir(parents=True,exist_ok=True)
source=(a.checkout/'src/dsp/AnamorphEngine.cpp').read_text()
original=subprocess.check_output(['git','-C',str(a.checkout),'show','113b9189f0ff4d3b6348c553c559c22ee8ade782:src/dsp/AnamorphEngine.cpp'],text=True)
wet=source.replace('L[i] = blendMultibandEnableSample (pmL[i], L[i], b);','L[i] = pmL[i] + b * (L[i] - pmL[i]);').replace('R[i] = blendMultibandEnableSample (pmR[i], R[i], b);','R[i] = pmR[i] + b * (R[i] - pmR[i]);')
dry=source.replace('aLw[i] = blendMultibandEnableSample (cdL[i], aLw[i], b);','aLw[i] = cdL[i] + b * (aLw[i] - cdL[i]);').replace('aRw[i] = blendMultibandEnableSample (cdR[i], aRw[i], b);','aRw[i] = cdR[i] + b * (aRw[i] - cdR[i]);')
assert wet!=source and dry!=source
record={}
for variant,text in [('fixed',source),('original',original),('wet-only',wet),('dry-only',dry)]:
    engine=a.out/('engine-'+variant+'.cpp');engine.write_text(text)
    out=a.out/variant
    subprocess.run([sys.executable,str(Path(__file__).with_name('build_probe.py')),
        '--checkout',str(a.checkout),'--build',str(a.build),'--base-source',str(a.checkout),
        '--source',str(Path(__file__).with_name('probe.cpp')),'--out',str(out),'--engine',str(engine)],check=True)
    result=subprocess.run([str(out/'probe')],text=True,capture_output=True)
    (out/'stdout.log').write_text(result.stdout);(out/'stderr.log').write_text(result.stderr)
    record[variant]={'exit':result.returncode,'last_line':result.stdout.splitlines()[-1] if result.stdout else ''}
    print(variant,record[variant])
(a.out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
raise SystemExit(0 if record['fixed']['exit']==0 and all(record[x]['exit']==1 for x in ['original','wet-only','dry-only']) else 1)
