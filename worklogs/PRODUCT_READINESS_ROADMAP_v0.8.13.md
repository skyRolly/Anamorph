# Product Readiness Roadmap (post-RH-PR-8, v0.8.13 cycle)

Supersedes the execution-phase portion of `POST_v0.8.12_AUDIT_AND_ROADMAP.md` (its Phase-1
items are now shipped: state harness PR #82, JUCE 9 migration PR #83, RH-PR-8 pipeline PR #84
+ rehearsal PR #85). This is a forward roadmap, not an audit; drift review was limited to
correctness-affecting items (none found — the doc set was synced continuously through the
shipped PRs).

## 1. Maturity assessment

| Dimension | Level | Basis |
|---|---|---|
| **Engineering stability** | **High** | Bit-exact-proven DSP core (twin-dump discipline incl. across the JUCE 9 bump); 914 automated checks (140 DSP + 774 state) + pluginval strictness 10 both modes ×3, blocking on 3 OSes; serialization/parameter surface under regression protection; 16 Accepted ADRs + 1 Proposed (0022) |
| **Release readiness** | **Medium-high** | Tag-triggered pipeline validated end-to-end by rehearsal (run 30011792515); artifact integrity proven with CI-built bytes; **one human audition + a tag away** from the first traceable release. Missing: signing/notarization (macOS quarantine friction), Authenticode, installers |
| **Commercial readiness** | **Low** | No licensing/entitlement (RH-R1), no EULA/LICENSE file, **JUCE 9 licence tier undecided** (AGPLv3 vs commercial — a closed-source commercial release requires the paid JUCE licence; owner decision), no update mechanism, no support/QA matrix |
| **User-experience readiness** | **Medium** | Polished editor, 10 factory presets, tooltips, A/B + undo; but **zero user documentation** (README is a developer façade; only macOS INSTALL.txt ships), unverified DAW host matrix (KI-004), 3 small open UX issues (KI-009/010/011), macOS de-quarantine friction until notarization |

## 2. Remaining blockers

### Must do before v1.0
1. **Level-5 auditions** — one DAW session covers both outstanding sign-offs: v0.8.12's
   unrecorded audition AND JUCE 9 vs 8.0.14 (ADR-0022 acceptance). Human-gated; blocks the
   v0.8.13 tag.
2. **macOS notarization (RH-PR-3)** + **Windows Authenticode (RH-PR-5)** — Gatekeeper/
   SmartScreen make unsigned commercial distribution effectively broken. Human-gated
   (Apple Developer account, cert service, ADR-0019).
3. **Installers** (RH-PR-5b/6) — after signing.
4. **Licensing decision stack** — JUCE licence tier, product licensing model (ADR-0016/0017),
   LICENSE/EULA files. Business decisions first; engineering follows (RH-PR-4/7).
5. **User manual / quickstart** — a commercial product cannot ship with zero user docs.
6. **Host-matrix verification** (KI-004) — recorded load/automation/state evidence in the
   declared support matrix; plus serialization-schema freeze fixtures from every released
   0.8.x before 1.0 freezes the schema.

### Should do for v0.9.x
1. `auval` in CI (macos runner: `auval -v aufx Anmr Anmf`) — closes "AU built but never
   validated" headlessly; small.
2. Golden-audio regression harness (renders per factory preset, hash-compared; extends the
   twin-dump methodology into CI) — locks the audible product, not just the engine.
3. Factory-preset review/expansion (10 exist; audition-driven).
4. UX backlog: KI-009 (REAPER preset-save focus), KI-010, KI-011 hardware re-test; KI-013
   macOS reconcile (needs a platform-specific `pressedMouseButtons` query + review, or an
   upstream JUCE fix — re-verified still absent in 9.0.0).
5. PERFORMANCE_BUDGET real numbers via a repeatable bench procedure (closes RISK-002's TODO).
6. macOS dSYM restoration (`-Wl,-object_path_lto`) — the rehearsal reconfirmed the gap
   (macOS-debug artifact skipped); restores crash symbolication on the platform most likely
   to need it.
7. Supply-chain leftovers: pluginval version pin; optionally SHA-pinned actions.

### Optional improvements
Deferred DSP work stays closed absent measurements (W5-A, W5-D-without-AVX2-ADR abandoned
per the audit); Category-C idle gates only if idle CPU ever matters (one consolidated
Architecture Review); `PluginEditor.cpp`/`SpectrumImager.cpp` size watch (no preemptive
restructuring); crossover glide < 0.5 s stays escalated (Hard-Stop levers only).

## 2b. Item-by-item re-evaluation (nothing carried forward blindly)

| Item | Classification | Why |
|---|---|---|
| Level-5 auditions (v0.8.12 + JUCE 9) | **Must do now** (P0) | The only blocker between five merged infrastructure PRs and the first traceable release; one DAW session covers both; every other phase benefits from the tagged baseline |
| Flat-recombination retroactive ADR | **Must do now** (P0, tiny) | An *audible* 0.8.10 maths change with no decision record — the one true correctness debt in the doc set; must exist before 1.0 freezes history, cheapest now |
| macOS dSYM restoration | **Must do now** (P1, small) | Rehearsal re-proved the gap (macOS-debug artifact skipped); crash symbolication missing on the platform most likely to need it; CI-only, reversible |
| pluginval version pin | **Must do now** (P1, small) | The release gate currently downloads "latest" — an upstream release could change gate behaviour underneath a release build; one-line class of fix |
| User documentation | **Should do before 1.0** (P0-commercial) | Zero user docs is untenable for a paid product; scheduled as a real Phase-2 deliverable (needs product care, not a stub) |
| QA matrix / host validation (KI-004) | **Should do before 1.0** (P0-commercial) | Declared-support claims are Unverified until recorded per-host evidence exists; hardware/human-gated, so start early in 0.9.x |
| Golden-audio regression | **Should do before 1.0** (P1) | Locks the audible product (not just engine bit-exactness) against regressions; depends on the preset bank stabilizing first |
| Presets (review/expand) | **Should do before 1.0** (P1) | 10 exist and load-tested; content quality is audition-driven; precedes golden-audio |
| RH-PR-3 notarization | **Should do before 1.0** (P0-commercial) | Gatekeeper friction is a support wall; first Phase-3 engineering item once the Apple account exists; slots into the validated release.yml |
| RH-PR-5 Authenticode | **Should do before 1.0** (P1-commercial) | SmartScreen interstitials; parallel to RH-PR-3 after ADR-0019; cert-service-gated |
| RH-PR-5b/6 installers | **Should do before 1.0** (P1-commercial) | After signing by dependency (installers wrap signed bundles) |
| RH-PR-9 QA + full signed rehearsal | **Should do before 1.0** (P1) | The commercial exit gate; depends on 3/5/5b/6 |
| Performance-budget formalization | **Nice to have** (P2) | Engineering hygiene; nothing release-gates on it; the optimization program is correctly closed without new numbers |
| Deferred DSP (W5-A/W5-D/W3-10, Cat-C gates) | **Defer** | Unchanged verdicts — measured, recorded, closed absent new evidence or the AVX2 ADR |

## 2c. Independently-found gaps (not in the previous roadmap)

1. **Steinberg VST3 SDK licence compliance** (commercial, P0-before-sale): the VST3 SDK ships
   inside JUCE and is dual-licensed GPLv3 / the proprietary *VST 3 Licence Agreement* —
   closed-source commercial VST3 distribution requires the signed Steinberg agreement (and
   the VST-compatible-logo obligations). **No document in the repo mentions this.** Owner
   action + a compliance row; recorded as **RH-R10** in RELEASE_HARDENING_PLAN §2. Same
   review should produce the **third-party NOTICES file** (JUCE-vendored harfbuzz, sheenbidi,
   lunasvg (new in JUCE 9), Oboe/FLAC/ogg-vorbis as applicable, VST3 SDK) — required for any
   commercial distribution, trivial to assemble (P1-commercial, small).
2. **Support workflow** (P2, small): no GitHub issue templates, no SUPPORT.md, no triage
   labels. A commercial product needs an intake path; even pre-1.0 a bug-report template
   (version/build-number from the About box, host, OS) would raise report quality. Cheap,
   reversible, worth doing alongside the user manual.
3. **Crash reporting** (P3, defer): symbol artifacts exist per-run (ADR-0021); an in-plugin
   crash reporter stays RELEASE_HARDENING_PLAN Phase-2 — correct placement, unchanged.
4. **Update mechanism** (P3, defer): RH-PR-7 (in-plugin *check only*, never self-update) —
   correctly sequenced after licensing; unchanged.
5. **Onboarding / first-run experience** (P2, product decision): transparent-by-default is
   deliberate (amount=0); whether the first open should point at presets/tooltips is an owner
   UX call — folded into the user-manual/onboarding work, no engineering pre-commitment.
6. **Undo/gesture-coalescer test gap** (P2, small-medium): the custom undo system
   (gesture-gated coalescing, preset-switch bracketing, per-slot stacks) has NO automated
   behavioural tests — it is the largest remaining hand-verified-only subsystem. The
   AnamorphStateTests target already links the full processor, so headless coverage
   (gesture begin/end → one step; automation → no step; preset switch → one step; per-slot
   isolation) is now cheap to add. Best-value pure-engineering item on the list.
7. **"1.0" definition / support policy** (P1, docs+owner): version policy exists
   (MAJOR.MINOR.PATCH + tag convention) but what 1.0 *commits to* (schema stability window,
   supported hosts/OS versions, update cadence) is undefined; needed before marketing a 1.0.

## 3. Recommended roadmap (ordered)

**Phase 1 — v0.8.13 release completion** *(P0; small; the pipeline is validated and waiting)*
1. Pre-tag engineering nits (P1, small, no deps): pluginval version pin; macOS dSYM
   restoration; retroactive flat-recombination ADR (docs-only debt from 0.8.10). Risk: low.
2. Release prep (P0, small): version bump 0.8.13; CHANGELOG `[0.8.13]` (JUCE 9 bump per the
   0.8.8 precedent; tag/draft-release availability note); docs sync. Risk: low.
3. **Human gates** (P0): combined Level-5 audition (v0.8.12 fixes + JUCE 9 A/B) → ADR-0022
   Accepted → `git tag -a v0.8.13` → review + publish the draft release. **Closes RISK-003.**
   Risk: low-medium (JUCE 9 visual delta is the one real unknown; the audition exists to
   catch it — fallback is trivial, re-pin to 8.0.14).

**Phase 2 — user-facing product readiness (0.9.x)** *(the largest value gap now)*
1. User quickstart/manual (P0; medium; no deps) — parameters, presets, workflows,
   per-platform install; becomes a release asset. Risk: none.
2. `auval` CI step (P1, small) + host-matrix program (P0, medium, human/DAW-gated;
   KI-004) — recorded evidence per host, COMPATIBILITY_MATRIX upgraded from Unverified.
3. Golden-audio regression (P1, medium; after preset review) — per-preset renders,
   tolerance-compared in CI. Risk: low (additive tests).
4. UX fixes KI-009/010/011/013 (P2, small each; investigation-first, GUI changes need the
   usual review discipline).
5. Performance-budget numbers (P2, small-medium; repeatable procedure recorded).

**Phase 3 — commercial release infrastructure** *(human decisions unlock engineering)*
1. Owner decisions: JUCE licence tier, pricing/licensing model, Apple/cert accounts
   (blocks everything below).
2. RH-PR-3 notarization → RH-PR-5 Authenticode (parallel after ADR-0019) — both slot into
   the validated release.yml between build and draft-release. Risk: medium (secrets
   handling; protected environments per the plan).
3. RH-PR-5b/6 installers; RH-PR-4/7 licensing implementation (ADR-0016/0017 first);
   LICENSE/EULA files. Risk: medium (licensing touches the wrapper — serialize with GUI work).

**Phase 4 — v1.0 preparation**
RH-PR-9 QA matrix + full signed-release rehearsal on clean machines; schema-freeze fixture
sweep (state blobs from every released 0.8.x/0.9.x into `tests/fixtures/`); support-matrix
declaration; 1.0 audition + tag. Risk: low (process, not code).

## 4. Documentation review (action taken)

* **User documentation: missing** — the defining gap; scheduled as Phase 2 item 1 (a real
  product deliverable, not an inline afterthought — creating a stub here would be cosmetic).
* **Developer documentation: sufficient** — policies/ADRs/procedures/worklogs are current
  and were synced through every shipped PR; no correctness-affecting drift found.
* **Release documentation: complete** — RELEASE_PROCESS (incl. §Tagging), RELEASE_POLICY,
  RELEASE_COMPATIBILITY_CHECKLIST, PACKAGING all match the rehearsed reality.
* Implemented in this pass: this roadmap record + the HANDOVER Roadmap pointer update
  (the previous roadmap's execution phase is complete, so the pointer was stale in the
  maintenance-blocking sense: it directed the next agent at finished work).

## 5. Technical recommendations (order rationale)

1. **Ship v0.8.13 first** — everything else benefits from a tagged, traceable baseline.
2. **auval-in-CI before the host-matrix program** — free automation first, hardware after.
3. **Presets before golden-audio** — freeze the bank, then lock it with renders.
4. **Signing before installers** — installers wrap signed bundles, not the reverse.
5. **Licensing last among Phase 3** — it needs ADRs + touches the wrapper; keep it off the
   critical path of notarization (which unblocks macOS UX immediately).
6. **Performance budget numbers when convenient** — engineering hygiene, not release-gating;
   the optimization program itself remains correctly closed without new measurements.

## 6. Outdated assumptions (explicitly retired)

* "Next Phase-1 items: state harness / JUCE SHA pin / RH-PR-8" — **all shipped** (PRs
  #82/#83/#84/#85); pointers updated. The audit-era roadmap's execution phase is history.
* "No tag-triggered release flow / artifacts collected by hand" — retired by RH-PR-8 +
  rehearsal; release docs now describe the pipeline reality.
* "Artifact trees ship as-is" — retired by the archive-at-source integrity fix; artifacts
  carry one permission-preserving zip each.
* "JUCE bump = audible-risk unknown" — partially retired: engine numerics are now
  *provably* bit-identical across a major bump (twin-dump method); the residual unknown is
  editor visuals only, which is exactly what the pending audition covers.
* "The sandbox cannot reach github.com at all" (ADR-0012-era) — partially retired: source
  fetches work here; the pluginval release download remains egress-blocked (re-verified 403),
  so pluginval stays a CI-side gate.
* "Commercial licence review = JUCE tier only" — retired by this pass: the **Steinberg VST3
  SDK agreement** and a third-party NOTICES file are additional, previously-unlisted
  obligations (RH-R10).

## 7. Documentation health verdict (this pass)

No factual errors, contradictions, or broken workflow instructions found — the doc set was
synced continuously through the shipped PRs and the release docs match the rehearsed
pipeline. The only substantive omission found is the missing VST3-SDK/NOTICES compliance
requirement, fixed by adding **RH-R10** to RELEASE_HARDENING_PLAN §2 (this pass's single doc
edit beyond the roadmap itself). Cosmetic citation drift was deliberately not hunted.
