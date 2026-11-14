from pathlib import Path
import difflib
root=Path('/workspace/anamorph-audit-113b9189')
source=(root/'source/src/dsp/AnamorphEngine.cpp').read_text()
variants={}
# A trace at the actual terminal smoother tick; no math or state is changed.
s=source.replace('#include <cstring>','#include <cstring>\n#include <cstdio>')
s=s.replace('const float b = mbEnableBlend.getNextValue();\n                L[i]', '''const bool auditWasSmoothing = mbEnableBlend.isSmoothing();
                const float auditWetL = L[i], auditWetR = R[i], auditAlignL = aLw[i], auditAlignR = aRw[i];
                const float b = mbEnableBlend.getNextValue();
                L[i]''')
s=s.replace('aRw[i] = cdR[i] + b * (aRw[i] - cdR[i]);', '''aRw[i] = cdR[i] + b * (aRw[i] - cdR[i]);
                if (auditWasSmoothing && ! mbEnableBlend.isSmoothing())
                    std::fprintf(stderr,"MB_END b=%g i=%d wetL=%a outL=%a wetR=%a outR=%a preL=%a preR=%a alignL=%a alignOutL=%a alignR=%a alignOutR=%a cleanL=%a cleanR=%a\\n", b,i,auditWetL,L[i],auditWetR,R[i],pmL[i],pmR[i],auditAlignL,aLw[i],auditAlignR,aRw[i],cdL[i],cdR[i]);''')
s=s.replace('if (nonFinite)\n    {\n        multiband.reset();','if (nonFinite)\n    {\n        std::fprintf(stderr,"HEAL n=%d\\n",n);\n        multiband.reset();')
variants['trace']=s
# Diagnostic endpoint selection: deliberately limited to the existing MB blend.
s=source
s=s.replace('L[i] = pmL[i] + b * (L[i] - pmL[i]);','L[i] = b == 1.0f ? L[i] : b == 0.0f ? pmL[i] : pmL[i] + b * (L[i] - pmL[i]);')
s=s.replace('R[i] = pmR[i] + b * (R[i] - pmR[i]);','R[i] = b == 1.0f ? R[i] : b == 0.0f ? pmR[i] : pmR[i] + b * (R[i] - pmR[i]);')
s=s.replace('aLw[i] = cdL[i] + b * (aLw[i] - cdL[i]);','aLw[i] = b == 1.0f ? aLw[i] : b == 0.0f ? cdL[i] : cdL[i] + b * (aLw[i] - cdL[i]);')
s=s.replace('aRw[i] = cdR[i] + b * (aRw[i] - cdR[i]);','aRw[i] = b == 1.0f ? aRw[i] : b == 0.0f ? cdR[i] : cdR[i] + b * (aRw[i] - cdR[i]);')
variants['endpoint']=s
# Diagnostic counterfactual only: let these two independently crossfaded values through an ordinary duck.
s=source.replace('dst.algorithm = al;   dst.haasSide = hs; dst.dimMode = dm; dst.mbEnable = mb;', 'dst.algorithm = al;   dst.haasSide = hs; dst.dimMode = dm;')
s=s.replace('dst.monoMakerEnable = mm; dst.oversample = ov; dst.bypass = by; dst.autoGainMatch = ag;', 'dst.monoMakerEnable = mm; dst.oversample = ov; dst.autoGainMatch = ag;')
variants['live_toggle']=s
s=source.replace('ch[i] += blend * (s - ch[i]);', 'ch[i] = blend == 1.0f ? s : ch[i] + blend * (s - ch[i]);')
variants['drive_endpoint']=s
s=source
s=s.replace('L[i] = dryL + m * (L[i] - dryL);', 'L[i] = m == 0.0f ? dryL : dryL + m * (L[i] - dryL);')
s=s.replace('R[i] = dryR + m * (R[i] - dryR);', 'R[i] = m == 0.0f ? dryR : dryR + m * (R[i] - dryR);')
variants['mix_zero']=s
s=source.replace('L[i] = dryL + m * (L[i] - dryL);', 'L[i] = m == 1.0f ? L[i] : dryL + m * (L[i] - dryL);').replace('R[i] = dryR + m * (R[i] - dryR);', 'R[i] = m == 1.0f ? R[i] : dryR + m * (R[i] - dryR);')
variants['mix_one']=s
s=variants['drive_endpoint']
s=s.replace('L[i] = dryL + m * (L[i] - dryL);', 'L[i] = m == 1.0f ? L[i] : dryL + m * (L[i] - dryL);')
s=s.replace('R[i] = dryR + m * (R[i] - dryR);', 'R[i] = m == 1.0f ? R[i] : dryR + m * (R[i] - dryR);')
variants['drive_mix']=s
cmds=(root/'commands.txt').read_text().splitlines()
cc=next(c for c in cmds if ' -c ' in c and c.endswith('/src/dsp/AnamorphEngine.cpp'))
link=next(c for c in cmds if ' -o AnamorphTests_artefacts/Release/AnamorphTests ' in c)
for name,s in variants.items():
 assert s!=source
 (root/f'engine-{name}.cpp').write_text(s)
 patch=''.join(difflib.unified_diff(source.splitlines(True),s.splitlines(True),fromfile='baseline/AnamorphEngine.cpp',tofile=f'diagnostic-{name}/AnamorphEngine.cpp'))
 (root/f'{name}.patch').write_text(patch)
 obj=f'{root}/engine-{name}.o'
 c=cc.replace('CMakeFiles/AnamorphTests.dir/src/dsp/AnamorphEngine.cpp.o',obj).replace(str(root/'source/src/dsp/AnamorphEngine.cpp'),str(root/f'engine-{name}.cpp'))
 l=link.replace('CMakeFiles/AnamorphTests.dir/tests/dsp_tests.cpp.o',str(root/'probe.o')).replace('CMakeFiles/AnamorphTests.dir/src/dsp/AnamorphEngine.cpp.o',obj).replace(' -o AnamorphTests_artefacts/Release/AnamorphTests ',f' -o {root}/probe-{name} ')
 (root/f'build-{name}.sh').write_text('#!/bin/bash\nset -e\n'+c+'\n'+l+'\n')
