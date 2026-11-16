from pathlib import Path
import subprocess,json
root=Path('/workspace/n2-execution');out=root/'h4-glide-counts';out.mkdir(exist_ok=True)
rows=[]
for variant in ['base','guard']:
 for config in [(48000,128,2,1,0,1),(48000,128,4,1,0,1),(192000,32,4,1,0,1),(192000,256,4,1,0,1)]:
  name=variant+'-'+'-'.join(map(str,config));cg=out/(name+'.callgrind')
  cmd=['valgrind','--tool=callgrind','--collect-atstart=no','--callgrind-out-file='+str(cg),str(root/('h4-glide-count-'+variant)/'probe'),*map(str,config)]
  r=subprocess.run(cmd,text=True,capture_output=True);(out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
  summary=next(l for l in cg.read_text().splitlines() if l.startswith('summary:'))
  rows.append({'variant':variant,'config':config,'exit':r.returncode,'instructions':int(summary.split()[1]),'stdout':r.stdout,'command':cmd});(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,summary,r.stdout.strip(),flush=True)
