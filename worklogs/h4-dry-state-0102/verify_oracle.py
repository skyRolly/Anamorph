#!/usr/bin/env python3
"""Optional N2-01 oracle. Expected to FAIL current production, not a CI gate.

The independent warm production module determines unblended dry samples; a
continuously full-wet production twin determines wet samples. The low-frequency
unit-width twin is also an independent healthy reference (preserved raw CSV).
"""
import argparse
import csv
import json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('matrix')
p.add_argument('--out',required=True)
a=p.parse_args()
rows={r['case']:r for r in csv.DictReader(open(a.matrix))}
# Normalized stimuli <= .5. Healthy independent controls differ by at most
# 1.1921e-7 in this matrix. Allow 16 float eps for the bounded chain of MS and
# interpolation roundings; do not require bit-exact phase reconstruction.
tolerance=16*2**-23
checks=[]
for name in ['tone20','tone50']:
    r=rows[name]
    checks += [
        {'case':name,'assertion':'H4 consumer gate actually ran with an executing wet bank',
         'passed':int(r['gate_clean_blocks'])>0 and int(r['wet_changed_blocks'])>0},
        {'case':name,'assertion':'re-entry and subsequent state consumption occurred',
         'passed':int(r['reentry_blocks'])>960 and int(r['dry_frozen_blocks'])+int(r['dry_changed_blocks'])>0},
        {'case':name,'assertion':'first 960 samples reconstruct within the healthy rounding envelope',
         'passed':float(r['peak960'])<=tolerance,'observed':float(r['peak960']),'limit':tolerance},
        {'case':name,'assertion':'state recovers after the stimulus transition',
         'passed':float(r['tail_peak'])<=tolerance,'observed':float(r['tail_peak']),'limit':tolerance},
    ]
result={'checks':checks,'failed':sum(not c['passed'] for c in checks),'assertions':len(checks)}
open(a.out,'w').write(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(1 if result['failed'] else 0)
