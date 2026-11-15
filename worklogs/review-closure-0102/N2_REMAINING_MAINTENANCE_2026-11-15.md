# Remaining numerical maintenance — 2026-11-15

These findings remain independent. A passing N2-02 or N2-03 regression does
not close them, and no production correction for them is included in this
assignment.

| Finding | Current disposition | Dependency and suggested next work |
| --- | --- | --- |
| N2-01, P2 | Confirmed H4 recursive dry-state discontinuity at Mix re-entry. | Owner decides whether to retain/defer the limitation or authorize an evidence-supported recovery/performance proposal. The consumption-aware diagnostic has incomplete exceptional-history and cost evidence. |
| V2-01, P2 verification | Test 25's existing aggregate RMS checks remain valid but do not establish worst-frequency/sample-level reconstruction. | Retain the independent failing-on-main oracle as nonshipping; make it mandatory with an authorized N2-01 production correction. |
| N2-04, P3 | Conditioned-dry-history loss during recovery remains confirmed. | Investigate with N2-05 and H4 history ownership, without assuming a common cause or fix. Preserve the separate raw and aligned histories. |
| N2-05, P3 | Invalid wet arithmetic can lose otherwise valid dry at Mix=0. | Investigate recovery/output ownership and exact endpoint behavior, separately from N2-04 and N2-08. |
| N2-06, P3 | Mix=1 endpoint cancellation remains confirmed. | A later focused numerical endpoint assignment should preserve interior arithmetic, Mix semantics and smoother clocks. |
| N2-07, P3 | Drive endpoint cancellation remains confirmed. | Assess independently of the Mix and Multiband Enable endpoint fixes. |
| N2-08, P3 review assessment | Extreme finite input causes wet LR4 numerator overflow and the existing final self-heal can zero the affected block. | A separate assignment should compare bounded exceptional handling, selective widening, explicit contract change or deferral. A copied-state first-step widening produced a representable result; it is not a production fix or complete-tail proof. |

Suggested sequence: first settle the P2 H4/recovery contract and regression
strategy; then investigate N2-04/N2-05 recovery ownership together while
retaining distinct controls, N2-08's wet arithmetic independently, and the
N2-06/N2-07 endpoint issues as focused changes. This ordering is a maintenance
recommendation, not a finding that every P3 is an automatic release blocker.
Owners may explicitly document and defer P3 items according to release policy.

`LR4Xover.h` documents an expression-for-expression, bit-identical JUCE filter
contract over input and cutoff sequences. A future N2-08 correction must test
that contract and ordinary-input identity: exceptional-only handling may leave
ordinary results unchanged but still changes exceptional behavior; wholesale
widening or a different filter representation may break JUCE equivalence and
requires the applicable architecture/numerical review. Test actual intermediate
and output behavior, state evolution, channel ownership, tails, glides and the
full-engine twin; a representable first sample alone is insufficient. Keep the
JUCE pin and original witnesses intact.

The 0.10.2 release is not authorized or established ready. Staged integration,
exact-source hosted gates, an explicit H4/ADR decision or accepted documented
deferral, remaining finding dispositions, and applicable owner-attested native,
DAW/listening/manual and Level-5 acceptance remain separate tasks. Historical
human or hardware evidence cannot be relabeled as new acceptance.
