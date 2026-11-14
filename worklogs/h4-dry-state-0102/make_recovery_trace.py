#!/usr/bin/env python3
"""Nonshipping counter variant; never use its binary for performance evidence."""
import argparse
import subprocess
import sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--checkout',type=Path,required=True);p.add_argument('--build',type=Path,required=True)
p.add_argument('--base-source',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--unrelated',action='store_true');p.add_argument('--reset-both-mutation',action='store_true')
a=p.parse_args();a.checkout=a.checkout.resolve();a.build=a.build.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
s=(a.checkout/'src/dsp/AnamorphEngine.cpp').read_text();needle='namespace anamorph\n{';assert s.count(needle)==1
s=s.replace(needle,needle+'\nextern int n2WetHeals; extern int n2DryHeals[2];',1)
needle='    if (nonFinite)\n    {';assert s.count(needle)==1
s=s.replace(needle,needle+'\n        ++n2WetHeals; // temporary execution witness',1)
s=s.replace('                    multiband.resetDryChannel (channel);','                    ++n2DryHeals[channel]; // temporary execution witness\n                    multiband.resetDryChannel (channel);')
if a.reset_both_mutation:
    assert 'multiband.resetDryChannel (channel);' in s
    s=s.replace('multiband.resetDryChannel (channel);','multiband.resetDryChannel (0); multiband.resetDryChannel (1);')
engine=a.out/'engine-trace.cpp';engine.write_text(s)
subprocess.run([sys.executable,str(Path(__file__).with_name('build_probe.py')),
    '--checkout',str(a.checkout),'--build',str(a.build),'--base-source',str(a.base_source),
    '--source',str(Path(__file__).with_name('unrelated_state_probe.cpp' if a.unrelated else 'recovery_probe.cpp')),
    '--engine',str(engine),'--out',str(a.out)],check=True)
r=subprocess.run([str(a.out/'probe')],text=True,capture_output=True)
(a.out/'stdout.csv').write_text(r.stdout);(a.out/'stderr.log').write_text(r.stderr)
print(r.stdout,end='');raise SystemExit(r.returncode)
