# Owner authorization and continuity — 2026-11-15 UTC

This record preserves the owner's new instruction before integration or new
investigation. The prior consolidated independent review is commit
`bca1e9b18a667b917db1b9bbb22b1ecd3fb99cd6`; its final review, integration
procedure, source identities, raw receipts and decisions remain authoritative
within their recorded scope. The identities below are reviewed references,
not claims about the live repository at the time of execution.

## Reviewed references

- Main `1ade6e1ad128b067930ab216bc355605388eb1de`, containing merged #241/#242.
- #237 `e64e11fbf54b910aa6a32acafbeb4c3f64815f20`: evidence and maintained provenance.
- #238 `f11dba49b9ff68c48f96d1b4435c9464a1d7bb24`: N2-02.
- #239 `b08d514f9f147fd8c1b19e24ec0cba2f9ae57ef6`: N2-03.
- #240 `22e56b32e02420502cf9b091bf8b04baad2c340e`: H4 documentation and Proposed ADR-0074.
- Intended integration `70bbdc22acdfd6286d13b1a15cb445367525c5e5`, tree
  `e968d53284a38704a1f3982428a134c23252f3c3`.
- H4 nonshipping follow-up `codex/0102-h4-recovery-followup`, reviewed head
  `bbf941bd978502387c7ff22907fe0026c65d1e61`.

## Owner decisions and scope

The owner completed review and authorizes staged merging #237 → #238 → #239 →
#240 using merge commits, explicit retargeting and focused integration
resolutions. The owner authorizes temporarily disabling GitHub's automatic
head-branch deletion and restoring its original setting after dependent bases
are safely updated. Preserve the prerequisite and research branches. Verify
actual refs, setting, review state, CI, source and post-merge results; stop an
affected merge if material new defects, changes, contract issues, failed
required checks or unavailable merge-commit policy require a new decision.

The owner accepts maintained provenance infrastructure within its documented
Linux/Ninja model and explicit trust premises. Historical H4 reproduction
remains separate; replay only where relevant to current decisions.

The owner authorizes isolated H4 recovery/performance research and focused
investigation and separately reviewable production maintenance PRs for N2-04
through N2-08 under existing contracts and architecture gates. Newly created
maintenance PRs are not authorized for merge. No H4 production candidate,
exceptional-output policy, filter-representation or performance tradeoff is
authorized; ADR-0074 remains Proposed. No release, tag, version bump, manual
acceptance claim, or release publication is authorized. Preserve product
version 0.10.1, target 0.10.2, JUCE 9.0.5, accepted ADRs, plugin and parameter
identity, persistence, valid regressions and original archived evidence.

## Prior established decisions and evidence

#242 is merged; its three verification findings were resolved. Earlier hosted
cleanup and Rosetta failures remain unattributed. R-237-02/03/04 and R-239-01
were resolved within their established scopes. Historical replay work remains
open separately. N2-02 and N2-03 have independent mutation sensitivity.

N2-01 and V2-01 remain P2. A2's output-only detector misses hidden invalid dry
state. The consumption-aware candidate has scoped positive and negative
controls but not an accepted exceptional-output/aligned-history contract.
Injected invalid-state comparisons differed on 128 samples at zero latency and
122 at actual x4/six-sample latency; this is not natural reachability evidence.
N2-08 wet numerator overflow and post-Enable injected-kernel overflow are
separate observations. N2-04 through N2-08 remain distinct P3 maintenance
findings. Do not interpret successful independent correction PRs as acceptance
of H4, closure of unrelated defects, or release readiness.
