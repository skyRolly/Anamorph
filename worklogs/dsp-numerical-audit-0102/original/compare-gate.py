import csv,math
from collections import defaultdict
from pathlib import Path
p=Path('/workspace/anamorph-audit-113b9189')
def read(n):
 with (p/n).open() as f:return {(int(r['xover']),int(r['hz']),int(r['t'])):float.fromhex(r['out']) for r in csv.DictReader(f)}
a=read('gate-baseline.csv');b=read('gate-warm.csv');g=defaultdict(list)
for (x,h,t),v in a.items():g[(x,h)].append((t,v,b[x,h,t]))
for k,v in sorted(g.items()):
 for n in (960,4480):
  z=[(x-y,y) for t,x,y in v if t<n];peak=max(abs(x) for x,y in z);rms=math.sqrt(sum(x*x for x,y in z)/len(z));ref=math.sqrt(sum(y*y for x,y in z)/len(z));print(k,'n',n,'max',f'{peak:.9g}','rms',f'{rms:.9g}','ref_rms',f'{ref:.9g}','ratio',f'{rms/ref:.8g}')
