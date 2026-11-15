from pathlib import Path
import subprocess,json
root=Path('/workspace/n2-execution');out=root/'h4-guard-bench';out.mkdir(exist_ok=True)
records=[]
for number,variant in enumerate(['baseline','guard','guard','baseline']):
 binary=root/('h4-baseline' if variant=='baseline' else 'h4-bench-guard')/'probe';dst=out/(str(number)+'-'+variant+'.csv')
 with dst.open('w') as f:r=subprocess.run([str(binary),'--bench'],stdout=f,check=True)
 records.append({'sequence':number,'variant':variant,'binary':str(binary),'output':str(dst)})
 (out/'order.json').write_text(json.dumps(records,indent=2)+'\n')
