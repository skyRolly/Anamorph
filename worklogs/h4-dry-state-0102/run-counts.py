from pathlib import Path
import subprocess,json
root=Path('/workspace/n2-execution');out=root/'h4-counts';out.mkdir(exist_ok=True)
configs=[]
for bands in [2,3,4]:
 for moving in [0,1]:configs.append([48000,128,bands,moving,0,1])
configs.extend([[192000,32,4,0,0,1],[192000,32,4,1,0,1],[48000,32,4,0,0,1],[192000,256,4,0,0,1],[48000,128,4,0,1,1],[48000,128,4,0,0,.5]])
records=[]
for config in configs:
 for variant in ['baseline','warm','state']:
  name=variant+'-'+'-'.join(map(str,config));cg=out/(name+'.callgrind');cmd=['valgrind','--tool=callgrind','--collect-atstart=no','--callgrind-out-file='+str(cg),str(root/('h4-count-'+variant)/'probe'),*map(str,config)]
  r=subprocess.run(cmd,text=True,capture_output=True);(out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
  events=cg.read_text() if cg.exists() else '';summary=next((l for l in events.splitlines() if l.startswith('summary:')),'summary: 0')
  records.append({'variant':variant,'config':config,'exit':r.returncode,'instructions':int(summary.split()[1]),'command':cmd})
  print(name,r.returncode,summary,flush=True)
  (out/'results.json').write_text(json.dumps(records,indent=2)+'\n')
