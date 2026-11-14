#!/usr/bin/env python3
"""Build an archived probe from fresh baseline CMake objects; no fixed workspace paths."""
import argparse,hashlib,shlex,subprocess
from pathlib import Path
from replay_provenance import fresh_build, verify_archive_file, verify_variant
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--checkout',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--probe',choices=['probe','gate','overlap','processor','os','heal','drive'],required=True)
p.add_argument('--variant',default='baseline',choices=['baseline','live_toggle','endpoint','always_dry','trace','os_trace','retain_dry','drive_endpoint','drive_mix','mix_zero','mix_one'])
a=p.parse_args();a.checkout=a.checkout.resolve();a.build=a.build.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
sha=subprocess.check_output(['git','-C',str(a.checkout),'rev-parse','HEAD'],text=True).strip()
if sha!='113b9189f0ff4d3b6348c553c559c22ee8ade782':raise SystemExit('Archival replay requires the exact baseline SHA. Use a separate worktree.')
root=Path(__file__).resolve().parent/'original';state=a.probe=='processor';target='AnamorphStateTests' if state else 'AnamorphTests';main='state_tests.cpp' if state else 'dsp_tests.cpp'
names={'probe':'probe.cpp','gate':'gate-probe.cpp','overlap':'overlap-matrix.cpp','processor':'processor_probe.cpp','os':'os-probe.cpp','heal':'heal-probe.cpp','drive':'drive-probe.cpp'}
verify_archive_file(root, root/names[a.probe], names[a.probe])
if a.variant!='baseline': verify_variant(a.checkout, root, a.variant)
a.build=fresh_build(a.checkout,a.build,a.out/'fresh-build',target)
commands=subprocess.check_output(['ninja','-C',str(a.build),'-t','commands',target],text=True).splitlines()
cc=next(shlex.split(x) for x in commands if x.endswith('/tests/'+main) and ' -c ' in x)
link=next(shlex.split(x) for x in commands if ' -o '+target+'_artefacts/Release/'+target+' ' in x)
link=link[2:-2] if link[:2]==[':','&&'] and link[-2:]==['&&',':'] else link
source=a.out/names[a.probe];source.write_text((root/source.name).read_text().replace('/workspace/anamorph-audit-113b9189/source/tests/',str(a.checkout/'tests')+'/'))
obj=a.out/'probe.o';old_obj=cc[cc.index('-o')+1]
def compile_as(cmd,src,obj):
 cmd=cmd.copy()
 for flag,value in [('-o',str(obj)),('-c',str(src)),('-MF',str(obj)+'.d'),('-MT',str(obj))]:
  if flag in cmd:cmd[cmd.index(flag)+1]=value
 subprocess.run(cmd,cwd=a.build,check=True)
compile_as(cc,source,obj);link=[str(obj) if x==old_obj else x for x in link]
if a.variant!='baseline':
 ec=next(shlex.split(x) for x in commands if x.endswith('/src/dsp/AnamorphEngine.cpp') and ' -c ' in x)
 eo=a.out/'engine.o';old=ec[ec.index('-o')+1];compile_as(ec,root/('engine-'+a.variant+'.cpp'),eo);link=[str(eo) if x==old else x for x in link]
binary=a.out/(a.probe+'-'+a.variant);link[link.index('-o')+1]=str(binary)
subprocess.run(link,cwd=a.build,check=True)
print(str(binary));print('sha256 '+hashlib.sha256(binary.read_bytes()).hexdigest())
