from pathlib import Path
import subprocess,json
root=Path('/workspace/n2-execution');out=root/'h4-thread-results';out.mkdir(exist_ok=True);record=[]
for seq,label in enumerate(['base','guard','guard','base']):
 binary=root/('h4-thread-'+label)/'probe';dest=out/(str(seq)+'-'+label+'.csv')
 with dest.open('w') as f:subprocess.run([str(binary)],stdout=f,check=True)
 record.append({'sequence':seq,'variant':label,'command':[str(binary)],'output':str(dest)});(out/'order.json').write_text(json.dumps(record,indent=2)+'\n')
