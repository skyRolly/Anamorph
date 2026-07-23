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
