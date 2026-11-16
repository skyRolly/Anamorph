# N2-06 finite-endpoint candidate — scoped controls

The nonshipping engine-only diagnostic uses source head `230da0e2`, selected
patch SHA-256 `33a949720a3be46c17b9d6d41e258677096b30a243f3ae6c7f5ee658a979eb61`,
and expected diagnostic tree `e7ff97adc41e48d6e012660832e7fcc62842d232`.
The fresh clean build receipt is committed at `31bf22e2`; the reviewed
probe and control specifications are at `e2ea2fd8`. The serial Linux/Ninja
relinks record source, dependency, compiler inputs, probe, object, executable,
commands and result identities within the documented trust premises. Both
outer runners exited 0.

The candidate selected the finite Wet value only for exact Mix=1 in both
settled and smoothing Mix loops, retaining the historical expression when
either selected operand was non-finite or the coefficient was not one. The
actual-engine probe passed **11/11** assertions across 20,480 selected sample
outputs, including unchanged interior arithmetic, a copied smoother clock,
and selected dry/wet invalid-state and recovery controls. The same probe on
unmodified main failed **2/11** expected assertions: 1,748 settled endpoint
samples and 466 smoothing-path samples differed from the intended finite Wet;
its interior and clock checks had no mismatches.

This is a scoped candidate, not a production correction or proof of every
Match/Multiband Enable, oversampling, invalid-state, signed-zero or automation
interaction. The existing N2-03 output oracle models historical Mix=1 rounding
and must be carefully updated, with its own negative sensitivity retained,
before any independent production PR. Full suites, real-time checks and
exact-head hosted validation remain outstanding. No H4 candidate or product
recovery policy is approved by this experiment.
