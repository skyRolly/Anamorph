# Remaining numerical maintenance — staged assessment

This record distinguishes source review and historical observations from new
final-main execution. The reviewed combined production source is `70bbdc22`;
the final staged main identity and any new maintenance PR have not yet been
established at this checkpoint. Original measurements remain in the preserved
[numerical audit](../dsp-numerical-audit-0102/original/REPORT.md).

| Finding | Evidence and contract | Current action and next discriminating step |
| --- | --- | --- |
| N2-04, P3 | Existing final output self-heal clears valid conditioned dry delay with poisoned and unaffected channels together. ADR-0009's valid-audio rule and the separate D-02 raw history apply. | A bounded exceptional-only scan preserving finite conditioned slots and zeroing invalid slots is proposed in draft [PR #243](https://github.com/skyRolly/Anamorph/pull/243), head `6a65b1d1` on actual main `230da0e2`. Local v6 development controls pass 125/0, fail 28/125 on the original clear and 6/125 on unsafe retention; full DSP passes 5739/0 on the same source and test bytes. The independently receipted v5 run and sequential mutations retain their older 109-check identity. Exact-head fresh replay, final isolated v6 mutations and hosted checks are in progress and must not be inferred from those development results. Owner review and separate authorization are required before merging the new PR. |
| N2-05, P3 | At Mix=0, `0*(wet-dry)` can make valid dry non-finite when Wet is invalid. An exact dry selection alone can hide the invalid wet output from the existing guard and leave wet state unhealed. | Preserve the finding; examine a way to propagate invalid-wet detection to the existing reset while keeping the exact dry endpoint. Test actual wet contamination and later valid blocks, independent of N2-04 and N2-08. Moving/adding a guard or changing recovery ownership can require architecture review; do not assume the diagnostic Mix selection is a complete fix. |
| N2-06, P3 | At exact Mix=1 the actual non-H4 path can cancel finite Wet: the archived Width witness turns expected `(4,4)` into `(0,8)`. H4 separately omits its settled reblend in a narrower configuration. | Evaluate exact endpoint selection and an exceptional-only treatment against actual Match/enable paths, smoother ticks, invalid Wet/Dry and ordinary bit differences. Interior arithmetic must remain unchanged. A global endpoint selection can change ordinary finite rounding; quantify its effect before proposing a separate correction. |
| N2-07, P3 | The settled Drive blend can cancel a finite saturation result; the archived one-band H4 witness separates it from N2-06. | Compare endpoint and exceptional-only strategies across settled, smoothing and held-envelope oversampling paths, with independent actual-path and invalid-input controls. Preserve interior arithmetic, Drive timing and the separate Mix behavior. |
| N2-08, P3 review assessment | Extreme finite Wet LR4 numerator overflow precedes the existing self-heal. A copied-state widened first step yielded representable values, without proving a full filter trajectory. `LR4Xover` documents expression-for-expression JUCE bit identity over input and cutoff sequences. | Keep independent of H4. Compare exceptional-only handling, selective widening and deferral against complete state/tail behavior, glides and JUCE full-engine twins. An approach changing the accepted equivalence contract requires the applicable owner/ADR decision; no production change is authorized by the existing numerical evidence alone. |
| N2-09, P3 review assessment | A later actual production trace at `70bbdc22` shows finite extreme operands overflowing the historical interior dry Enable interpolation, writing/reading invalid aligned history and reaching self-heal. N2-03 preserved and did not introduce that interior expression. | Track separately; no production N2-09 correction is in the current authorization. A diagnostic Enable fallback exposed a later dry-source interpolation overflow, but a 216-case unpatched search did not establish independent reachability. This bounded negative result is not a proof of impossibility. See [H4 follow-up](https://github.com/skyRolly/Anamorph/tree/6982e81bd2c4e6f08bb56a40bc8b86bbb0dc9db6/worklogs/h4-decision-checkpoint-0102/followup). |

Suggested sequence is N2-04 as an independently reviewable change; N2-05's
recovery ownership in parallel with H4 history policy; N2-08's numerical
contract independently; then separate N2-06 and N2-07 endpoint proposals when
actual-path and ordinary-bit controls justify them. N2-09 needs a distinct
future scope. These are extreme-input P3 findings; their release disposition is
an owner risk decision informed by evidence, not an automatic universal release
blocker. N2-01/V2-01 and the unaccepted H4 recovery/performance policy remain
separate P2 production/verification matters.
