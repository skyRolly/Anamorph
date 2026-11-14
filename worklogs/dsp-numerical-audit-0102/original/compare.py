import csv,math,struct
from collections import defaultdict
base='/workspace/anamorph-audit-113b9189/'
def load(path):
 with open(base+path) as f:
  return {(int(r['bands']),float(r['mix']),int(r['leg']),int(r['t']),int(r['ch'])):float.fromhex(r['value']) for r in csv.DictReader(f)}
a=load('baseline.csv');b=load('endpoint.csv'); tr=load('trace.csv'); g=defaultdict(list)
for k,v in a.items():
 if v!=b[k]:g[(k[0],k[1],k[2])].append((k[3],k[4],v,b[k],abs(v-b[k])))
print('trace equal baseline',a==tr)
for k,rows in sorted(g.items()):
 print(k,'count',len(rows),'max_diff',max(r[4] for r in rows),'times',sorted(set(r[0] for r in rows))[:20],'first',rows[0])
print('total',sum(map(len,g.values())))
