#!/usr/bin/env python3
"""Generate source-bound combined-engine N2-05 boundary variants; diagnostic only."""
import hashlib,json,subprocess
from pathlib import Path
combined=Path('/workspace/n205-combined-diagnostic')
main=Path('/workspace/n205-combined-on-main230')
base=(combined/'src/dsp/AnamorphEngine.cpp').read_text()
root='worklogs/staged-integration-maintenance-0102/review-20261117/n205-combined-boundary/'
record=Path(__file__).resolve().parent
anchor='''    const auto mixSample = [] (float dry, float wet, float m) noexcept
    {
        if (juce::exactlyEqual (m, 1.0f)'''
assert base.count(anchor)==1
naive=base.replace(anchor,'''    const auto mixSample = [] (float dry, float wet, float m) noexcept
    {
        if (juce::exactlyEqual (m, 0.0f) && std::isfinite (dry)) return dry;
        if (juce::exactlyEqual (m, 1.0f)''')
flagged=base.replace(anchor,'''    bool invalidWetAtDryEndpoint = false;
    const auto mixSample = [&invalidWetAtDryEndpoint] (float dry, float wet, float m) noexcept
    {
        if (juce::exactlyEqual (m, 0.0f) && std::isfinite (dry) && ! std::isfinite (wet - dry))
        {
            invalidWetAtDryEndpoint = true;
            return dry;
        }
        if (juce::exactlyEqual (m, 1.0f)''')
assert flagged.count('    bool nonFinite = false;')==1
flagged=flagged.replace('    bool nonFinite = false;','    bool nonFinite = invalidWetAtDryEndpoint;')
try:
 for variant,text in [('baseline',base),('naive',naive),('flagged',flagged)]:
  source=main/'src/dsp/AnamorphEngine.cpp';source.write_text(text)
  patch=subprocess.check_output(['git','-C',str(main),'diff','--binary','-U0','--','src/dsp/AnamorphEngine.cpp'])
  subprocess.run(['git','-C',str(main),'add','src/dsp/AnamorphEngine.cpp'],check=True)
  tree=subprocess.check_output(['git','-C',str(main),'write-tree'],text=True).strip()
  subprocess.run(['git','-C',str(main),'reset','-q'],check=True)
  p=record/(variant+'.patch');p.write_bytes(patch)
  expected=0 if variant=='flagged' else 1
  runs=[]
  for os in ('off','x4'):
   runs.append({'program':'n205-boundary','args':[os,'observe'],'expected_exit':0,'stdout_contains':['injected=1','mix_advancing=1','verdict=pass']})
   runs.append({'program':'n205-boundary','args':[os,'contract'],'expected_exit':expected,'stdout_contains':['injected=1','verdict=pass' if expected==0 else 'FAIL N2-05 selected recovery contract']})
  spec={'purpose':f'N2-05 {variant} on the combined production engine, with deliberately injected Wet state and independent N2-04 correction; main230 objects are used under the accepted receipt.',
   'source_head':'230da0e2b0b81422f7e3964eea2833c48266a963','source_tree':tree,
   'patch':{'commit':'spec','path':root+variant+'.patch','sha256':hashlib.sha256(patch).hexdigest(),
            'purpose':f'Apply combined production engine and isolate the {variant} N2-05 comparison.'},
   'probes':[{'name':'n205-boundary','target':'AnamorphTests','commit':'spec','path':root+'probe.cpp'}], 'runs':runs}
  (record/(variant+'-spec.json')).write_text(json.dumps(spec,indent=2)+'\n');print(variant,tree)
finally:
 (main/'src/dsp/AnamorphEngine.cpp').write_text(base)
 subprocess.run(['git','-C',str(main),'reset','-q'],check=True)
