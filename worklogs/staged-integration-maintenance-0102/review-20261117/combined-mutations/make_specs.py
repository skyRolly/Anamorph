#!/usr/bin/env python3
"""Generate reviewable engine-only negative controls for the identified combined tree.

Run only in the named disposable clean checkout, with no simultaneous build.
Each replacement has an exact expected count; the resulting patch and Git tree
are committed separately and later verified by the receipt-controlled runner.
"""
import hashlib,json,subprocess
from pathlib import Path
checkout=Path('/workspace/numerical-combined-mutant')
record=Path(__file__).resolve().parent
root='worklogs/staged-integration-maintenance-0102/review-20261117/combined-mutations/'
source=checkout/'src/dsp/AnamorphEngine.cpp'
base=source.read_text()
def replace(text, old, new, count=1, which=0):
    if text.count(old)!=count: raise ValueError(f'expected {count} occurrences, found {text.count(old)}: {old[:90]!r}')
    at=-1
    for _ in range(which+1): at=text.index(old,at+1)
    return text[:at]+new+text[at+len(old):]
loop='''        for (int i = 0; i < ddSize; ++i)
        {
            if (! std::isfinite (ddL[i])) ddL[i] = 0.0f;
            if (! std::isfinite (ddR[i])) ddR[i] = 0.0f;
        }'''
mix='''L[i] = mixSample (dryL, L[i], m);
            R[i] = mixSample (dryR, R[i], m);'''
mix_second='''L[i] = mixSample (dryL, L[i], m);
        R[i] = mixSample (dryR, R[i], m);'''
drive='''L[i] = driveSample (L[i], sl, blend);
                R[i] = driveSample (R[i], sr2, blend);'''
mods={
 'n204-clear':('n204',lambda s:replace(s,loop,'        dryDelayBuffer.clear();')),
 'n204-retain':('n204',lambda s:replace(s,loop,'        // Negative control: retain unvalidated conditioned history.')),
 'n206-settled':('n206',lambda s:replace(s,mix,'''L[i] = dryL + m * (L[i] - dryL);
            R[i] = dryR + m * (R[i] - dryR);''')),
 'n206-smoothing':('n206',lambda s:replace(s,mix_second,'''L[i] = dryL + m * (L[i] - dryL);
        R[i] = dryR + m * (R[i] - dryR);''')),
 'n206-unguarded':('n206',lambda s:replace(s,'juce::exactlyEqual (m, 1.0f) && std::isfinite (dry) && std::isfinite (wet)','juce::exactlyEqual (m, 1.0f)')),
 'n206-interior':('n206',lambda s:replace(s,'return dry + m * (wet - dry);','return (1.0f - m) * dry + m * wet;')),
 'n207-settled':('n207',lambda s:replace(s,'ch[i] = driveSample (ch[i], s, blend);','ch[i] += blend * (s - ch[i]);')),
 'n207-smoothing':('n207',lambda s:replace(s,drive,'''L[i] += blend * (sl - L[i]);
                R[i] += blend * (sr2 - R[i]);''',2,0)),
 'n207-held':('n207',lambda s:replace(s,drive,'''L[i] += blend * (sl - L[i]);
                R[i] += blend * (sr2 - R[i]);''',2,1)),
 'n207-right-settled':('n207',lambda s:replace(s,'ch[i] = driveSample (ch[i], s, blend);','ch[i] = ch == R ? ch[i] + blend * (s - ch[i]) : driveSample (ch[i], s, blend);')),
 'n207-right-smoothing':('n207',lambda s:replace(s,'R[i] = driveSample (R[i], sr2, blend);','R[i] += blend * (sr2 - R[i]);',2,0)),
 'n207-right-held':('n207',lambda s:replace(s,'R[i] = driveSample (R[i], sr2, blend);','R[i] += blend * (sr2 - R[i]);',2,1)),
 'n207-unguarded':('n207',lambda s:replace(s,'juce::exactlyEqual (blend, 1.0f) && std::isfinite (input) && std::isfinite (saturated)','juce::exactlyEqual (blend, 1.0f)')),
 'n207-interior':('n207',lambda s:replace(s,'return input + blend * (saturated - input);','return (1.0f - blend) * input + blend * saturated;')),
 'n203-wet':('n203',lambda s:replace(s,'''L[i] = blendMultibandEnableSample (pmL[i], L[i], b);
                R[i] = blendMultibandEnableSample (pmR[i], R[i], b);''','''L[i] = pmL[i] + b * (L[i] - pmL[i]);
                R[i] = pmR[i] + b * (R[i] - pmR[i]);''')),
 'n202-ordinary':('n202',lambda s:replace(replace(replace(replace(s,'const auto hs = dst.haasSide;    const auto dm = dst.dimMode;','const auto hs = dst.haasSide;    const auto dm = dst.dimMode; const auto mb = dst.mbEnable;'),'const auto mm = dst.monoMakerEnable; const auto ov = dst.oversample;','const auto mm = dst.monoMakerEnable; const auto ov = dst.oversample; const auto by = dst.bypass;'),'dst.algorithm = al;   dst.haasSide = hs; dst.dimMode = dm;','dst.algorithm = al;   dst.haasSide = hs; dst.dimMode = dm; dst.mbEnable = mb;'),'dst.monoMakerEnable = mm; dst.oversample = ov; dst.autoGainMatch = ag;','dst.monoMakerEnable = mm; dst.oversample = ov; dst.bypass = by; dst.autoGainMatch = ag;')),
 'n202-forced':('n202',lambda s:replace(s,'if (! pendingForced)\n        {\n            const bool heard','if (true) // negative control: premature forced adoption\n        {\n            const bool heard')),
}
try:
 for name,(target,mutate) in mods.items():
    source.write_text(mutate(base))
    patch=subprocess.check_output(['git','-C',str(checkout),'diff','--binary','-U0','--','src/dsp/AnamorphEngine.cpp'])
    if not patch: raise ValueError('empty mutation '+name)
    subprocess.run(['git','-C',str(checkout),'add','src/dsp/AnamorphEngine.cpp'],check=True)
    tree=subprocess.check_output(['git','-C',str(checkout),'write-tree'],text=True).strip()
    subprocess.run(['git','-C',str(checkout),'reset','-q'],check=True)
    (record/(name+'.patch')).write_bytes(patch)
    cross='n203' if target!='n203' else 'n202'
    if target=='n202':cross='n203'
    path=root+name+'.patch'
    spec={'purpose':f'Isolated {name} mutation on the combined N2-04/N2-06/N2-07 source; all other engine contents and test headers are retained.',
      'source_head':'0ea2299b4f9f0ea66fffefbe27e317aad89af7de','source_tree':tree,
      'patch':{'commit':'spec','path':path,'sha256':hashlib.sha256(patch).hexdigest(),'purpose':f'Restore or perturb only {name} as a negative control.'},
      'probes':[{'name':'combined-mutation','target':'AnamorphTests','commit':'spec','path':root+'harness.cpp',
        'support':[root+x for x in ['AllocationGuard.h','n2_conditioned_history_tests.h','n2_mix_endpoint_tests.h','n2_drive_endpoint_tests.h','n2_endpoint_tests.h','n2_control_tests.h','n2_test_support.h']]}],
      'runs':[{'program':'combined-mutation','args':[target],'expected_exit':1,'stdout_contains':['FAIL ','selector='+target]},
              {'program':'combined-mutation','args':[cross],'expected_exit':0,'stdout_contains':['selector='+cross,'0 failures']}]}
    (record/(name+'-spec.json')).write_text(json.dumps(spec,indent=2)+'\n')
    print(name,target,cross,tree)
finally:
 source.write_text(base)
 subprocess.run(['git','-C',str(checkout),'reset','-q'],check=True)
