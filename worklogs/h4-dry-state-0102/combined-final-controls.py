from pathlib import Path
import shlex,subprocess,json,hashlib
root=Path('/workspace/anamorph-n2-combined');build=Path('/workspace/n2-execution/combined-build');out=Path('/workspace/n2-execution/combined-final-controls');out.mkdir(exist_ok=True)
commands=subprocess.check_output(['ninja','-C',str(build),'-t','commands','AnamorphTests'],text=True).splitlines();cc=next(shlex.split(x) for x in commands if x.endswith('/src/dsp/AnamorphEngine.cpp') and ' -c ' in x);old_obj=cc[cc.index('-o')+1];link=next(shlex.split(x) for x in commands if ' -o AnamorphTests_artefacts/Release/AnamorphTests ' in x)[2:-2]
s=(root/'src/dsp/AnamorphEngine.cpp').read_text();needle='    const auto nb = dst.mbBands;';assert s.count(needle)==1
lost=s.replace(needle,needle+'\n    const auto lostMb = dst.mbEnable; const auto lostBypass = dst.bypass;').replace('    dst.monoMakerEnable = mm; dst.oversample = ov; dst.autoGainMatch = ag;','    dst.monoMakerEnable = mm; dst.oversample = ov; dst.autoGainMatch = ag;\n    dst.mbEnable = lostMb; dst.bypass = lostBypass;')
endpoint=s.replace('L[i] = blendMultibandEnableSample (pmL[i], L[i], b);','L[i] = pmL[i] + b * (L[i] - pmL[i]);').replace('R[i] = blendMultibandEnableSample (pmR[i], R[i], b);','R[i] = pmR[i] + b * (R[i] - pmR[i]);').replace('aLw[i] = blendMultibandEnableSample (cdL[i], aLw[i], b);','aLw[i] = cdL[i] + b * (aLw[i] - cdL[i]);').replace('aRw[i] = blendMultibandEnableSample (cdR[i], aRw[i], b);','aRw[i] = cdR[i] + b * (aRw[i] - cdR[i]);')
record={}
for name,source in [('lost-controls',lost),('old-endpoints',endpoint)]:
 src=out/(name+'.cpp');src.write_text(source);obj=out/(name+'.o');compile_cmd=cc.copy()
 for flag,val in [('-c',str(src)),('-o',str(obj)),('-MF',str(obj)+'.d'),('-MT',str(obj))]:
  if flag in compile_cmd:compile_cmd[compile_cmd.index(flag)+1]=val
 subprocess.run(compile_cmd,cwd=build,check=True);cmd=[str(obj) if t==old_obj else t for t in link];binary=out/name;cmd[cmd.index('-o')+1]=str(binary);subprocess.run(cmd,cwd=build,check=True)
 record[name]={'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'compile':compile_cmd,'link':cmd,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'runs':{}}
 for selector in ['--n2-controls','--multiband-endpoints']:
  r=subprocess.run([str(binary),selector],text=True,capture_output=True);(out/(name+selector+'.stdout')).write_text(r.stdout);(out/(name+selector+'.stderr')).write_text(r.stderr);record[name]['runs'][selector]={'exit':r.returncode,'last':r.stdout.splitlines()[-1]};print(name,selector,r.returncode,r.stdout.splitlines()[-1],flush=True)
 (out/'identity.json').write_text(json.dumps(record,indent=2)+'\n')
