from pathlib import Path
import json, subprocess, hashlib, argparse
p=argparse.ArgumentParser();p.add_argument('--identity',type=Path,required=True);p.add_argument('--checkout',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(exist_ok=True)
d=json.loads(a.identity.read_text());cc=next(c for c in d['commands'] if '-c' in c and c[c.index('-c')+1].endswith('/AnamorphEngine.cpp'));old=cc[cc.index('-o')+1];link=d['commands'][-1]
s=(a.checkout/'src/dsp/AnamorphEngine.cpp').read_text();original=subprocess.check_output(['git','-C',str(a.checkout),'show','113b9189f0ff4d3b6348c553c559c22ee8ade782:src/dsp/AnamorphEngine.cpp'],text=True)
wet=s.replace('L[i] = blendMultibandEnableSample (pmL[i], L[i], b);','L[i] = pmL[i] + b * (L[i] - pmL[i]);').replace('R[i] = blendMultibandEnableSample (pmR[i], R[i], b);','R[i] = pmR[i] + b * (R[i] - pmR[i]);')
dry=s.replace('aLw[i] = blendMultibandEnableSample (cdL[i], aLw[i], b);','aLw[i] = cdL[i] + b * (aLw[i] - cdL[i]);').replace('aRw[i] = blendMultibandEnableSample (cdR[i], aRw[i], b);','aRw[i] = cdR[i] + b * (aRw[i] - cdR[i]);')
record={'source_sha':subprocess.check_output(['git','-C',str(a.checkout),'rev-parse','HEAD'],text=True).strip(),'unmodified_objects_identity':d,'variants':{}}
for name,source in [('original',original),('wet-only',wet),('dry-only',dry)]:
 src=a.out/(name+'.cpp');src.write_text(source);obj=a.out/(name+'.o');cmd=cc.copy()
 for flag,value in [('-c',str(src)),('-o',str(obj)),('-MF',str(obj)+'.d'),('-MT',str(obj))]:
  if flag in cmd:cmd[cmd.index(flag)+1]=value
 subprocess.run(cmd,cwd=a.build,check=True);lc=[str(obj) if x==old else x for x in link];binary=a.out/name;lc[lc.index('-o')+1]=str(binary);subprocess.run(lc,cwd=a.build,check=True)
 r=subprocess.run([str(binary),'--multiband-endpoints'],text=True,capture_output=True);(a.out/(name+'.stdout')).write_text(r.stdout);(a.out/(name+'.stderr')).write_text(r.stderr)
 record['variants'][name]={'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'compile':cmd,'link':lc,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'exit':r.returncode,'last':r.stdout.splitlines()[-1]};print(name,r.returncode,r.stdout.splitlines()[-1],flush=True)
 (a.out/'identity.json').write_text(json.dumps(record,indent=2)+'\n')
