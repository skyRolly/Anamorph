from pathlib import Path
import subprocess,json
root=Path('/workspace/n2-execution');records=json.loads((root/'h4-counts/results.json').read_text());out=root/'h4-counts-guard-final';out.mkdir(exist_ok=True)
result=[]
for row in records:
 if row['variant']!='baseline':continue
 config=row['config'];name='guard-'+'-'.join(map(str,config));cg=out/(name+'.callgrind')
 cmd=['valgrind','--tool=callgrind','--collect-atstart=no','--callgrind-out-file='+str(cg),str(root/'h4-count-guard-final/probe'),*map(str,config)]
 r=subprocess.run(cmd,text=True,capture_output=True);(out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
 summary=next(l for l in cg.read_text().splitlines() if l.startswith('summary:'))
 result.append({'variant':'guard','config':config,'exit':r.returncode,'instructions':int(summary.split()[1]),'command':cmd});(out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(name,r.returncode,summary,flush=True)
