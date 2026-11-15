import csv,math
from pathlib import Path
p=Path('/workspace/anamorph-audit-113b9189')
def get(name,x,h):
 with (p/name).open() as f:return [(int(r['t']),float.fromhex(r['out']),float.fromhex(r['fullwet'])) for r in csv.DictReader(f) if int(r['xover'])==x and int(r['hz'])==h]
def rms(xs):return math.sqrt(sum(v*v for v in xs)/len(xs))
for x,h in [(20,20),(20,50),(50,50),(200,200)]:
 a=get('gate-twin-baseline.csv',x,h);b=get('gate-twin-warm.csv',x,h)
 for lo,hi in [(0,960),(960,1920),(1920,2880),(2880,4480)]:
  aa=[v for t,v,w in a if lo<=t<hi];ref=[w for t,v,w in a if lo<=t<hi];warm=[v for t,v,w in b if lo<=t<hi];
  print('cross',x,'tone',h,'range',lo,hi,'raw_vs_fullwet_peak',max(abs(i-j) for i,j in zip(aa,ref)),'rms_err',rms([i-j for i,j in zip(aa,ref)]),'level_db',20*math.log10(rms(aa)/rms(ref)),'warm_vs_fullwet_peak',max(abs(i-j) for i,j in zip(warm,ref)))
