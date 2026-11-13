import csv,math
p='/workspace/anamorph-audit-113b9189/gate-twin-baseline.csv'
for cross,hz in [(20,20),(20,50),(50,50),(200,200)]:
 with open(p) as z:
  rows=[r for r in csv.DictReader(z) if int(r['xover'])==cross and int(r['hz'])==hz and int(r['t'])<2560]
 ins=[.5*math.sin(2*math.pi*hz*(144000+int(r['t']))/48000) for r in rows]
 out=[float.fromhex(r['out']) for r in rows]
 ratio=math.sqrt(sum(x*x for x in out)/sum(x*x for x in ins))
 print(cross,hz,'2560-sample RMS ratio',ratio,'passes Test25 > 0.90',ratio>.90)
