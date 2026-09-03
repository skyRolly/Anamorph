# Engineering Review Programme — the standing worklog

**What this is.** The persistent record of the ongoing engineering review & improvement
programme: an evidence-driven, round-based sweep of the whole repository — problems first,
then fragile/incomplete areas, then optimisations — with every significant finding
adversarially verified before it is acted on or filed. One section per round, newest first.
The rendered companion `ENGINEERING_REVIEW_REPORT.html` beside this file is the **live
dashboard** — a VIEW of this worklog, updated and re-committed whenever findings, decisions,
implementations or the roadmap materially change. This worklog is what a later round
reconciles against (the repository's standing worklog rule, `docs/REPOSITORY_MAP.md`).

**Finding IDs.** `ER-<LENS>-<NN>` — lens ∈ {DSP, RT, STATE, GUI, TST, CI, DOC, DEP}, NN from
the round that raised it. IDs are stable across rounds and are what code comments, KI/RISK
entries and CHANGELOG notes cite.

**Method (every round).**
1. Parallel read-only investigation lenses over the subsystems, each primed with the policies,
   `KNOWN_ISSUES`/`FUTURE_RISKS`/`POSTMORTEMS`, the Accepted ADRs and prior worklogs, so known
   or deliberate behaviour is not re-reported.
2. Every finding of severity medium+ gets independent adversarial verification (2 verifiers
   for high/critical — one correctness lens tracing the code path, one context lens checking
   for deliberate/known/rejected status) before it may enter this record as more than a note.
3. Triage per finding: fix now / investigate / regression-test / docs / CI / optimise / defer /
   reject — with Architecture-Review-Gate items **filed, never unilaterally fixed**.
4. Implementations land with regression tests exercising the real code path, the triggered doc
   syncs (`DOCUMENTATION_LIFECYCLE_POLICY`), and full validation before push.
5. Negative results (areas inspected and found sound) are recorded — they are the map of where
   NOT to dig next round.

---

## ADR-0034 — 2026-09-03 — the reported latency stops following Drive (a maintainer instruction, not a review round)

Out of band from the review programme and recorded here because this worklog is what a later round
reconciles against. **A hard-stop category** — `ARCHITECTURE_REVIEW_GATE.md`: *"Latency change —
sources, engagement condition, or reported value"*, plus a signal-flow change (a new delay element)
— **discharged by the maintainer's own instruction and by ADR-0034**, which is Accepted and amends
ADR-0003's latency clause.

### The report

> *"when Drive is at zero, the latency reported to the host is zero; then, when it is turned up
> slightly, it will report a latency value. During this adjustment, the plugin's own latency
> changes, causing an interruption / glitch in the sound."*

with the requirement stated exactly: a latency change is allowed **only** at the moment Oversampling
itself is switched, and the existing CPU optimisations — naming the Drive-0 oversampling skip —
must be **kept**.

### Reproduced before any change

`AnamorphTests --os-latency-probe` on the pre-change build, 48 kHz, linear algorithm:

| factor | Drive 0.005 dB | Drive 6 dB |
|---|---|---|
| 2x | 0 | **4** |
| 4x | 0 | **6** |
| 8x | 0 | **6** |

The same step on an Algorithm change at Drive 0. `osActiveFor` was answering four questions with one
bit — does the wrap RUN, what is latched into `osEngaged`, does the change need a duck, and how much
latency is reported. Only the fourth was an accident.

### The fix, and the two wrong ones it is built to exclude

Both accessors read the oversampling **selection** and nothing else, through one helper so they
cannot drift apart. Where the wrap is skipped but a factor is selected, `osCompDelayBuffer` — a
2-channel integer ring, no arithmetic — supplies the wrap's group delay **in the wrap's own place in
the chain**, which is what keeps the five downstream `-lat` reads measuring from an unchanged point.
`osActiveFor`, `osEngaged`, `currentOversampler()` and the `discreteDiffers` term are untouched.

* **Always running the wrap** would make the number constant and delete the optimisation. Measured
  cost of the round trip alone at 48 kHz/128: 211.12 / 265.46 / 380.00 ns/sample at 2x/4x/8x against
  152–160 with it skipped. Test 52 leg C rejects such a build by requiring the skipped state's
  output to be the OS-off output delayed **bit for bit** — measured on that exact counterfactual at
  **1.31–1.57 absolute** on a ±1 stream.
* **Reporting a latency the chain does not have** would leave every other track early by 4–6
  samples. Test 52 leg B measures the impulse through the bypass path and requires it at exactly the
  reported sample.

### A second click, found by auditing the change and measured before being claimed

In true bypass the output IS the raw-input ring read at `-lat`, and the Bypass crossfade is applied
AFTER the switch duck's gain — so while bypassed the duck attenuates nothing and a moving `lat`
jumped the read position at full level. On a 220 Hz / 0.5 sine (largest smooth step 0.01440), worst
sample-to-sample step **0.07162** at 2x and **0.09988** at 4x and 8x. After the change: exactly the
bound. Its first draft ran at ONE start phase and **passed against the defective engine** — the jump
is invisible when it lands on a peak of the sine — so leg D sweeps 16 phases and takes the worst.

### What it cost, stated rather than discovered later

* Selecting 2x/4x/8x now shows latency in the host on a fully linear chain. Three places in the
  user manual and the Settings tooltip promised the opposite and are corrected.
* A forced swap (A/B, preset, undo) crossing the Drive threshold with a factor selected is now
  latency-NEUTRAL, so it keeps its dry fill instead of dipping to silence. Correct, audible, declared.
* **RELEASE_POLICY precondition 7 reopens for v0.9.7** — the Level-5 audition is per-version and
  this build changes audible behaviour. The compatibility checklist's latency box was **re-run**.

### Three state tests re-instrumented, one of which would have HUNG

22, 24 and 27 drove the latency through Drive because it was the only automatable latency-bearing
control; they now drive the Oversampling Setting through a `juce::Value` captured on the message
thread. State test 27's ER-STATE-14 barrier is JUCE's **change-only** listener notification, so
after the fix a Drive move produces no callback, its worker spins on `inDelivery` forever and
`worker.join()` never returns — a CI job would burn its whole timeout. Caught locally by a suite run
that stopped producing output; diagnosed from a live backtrace, not from the test's source.
State test 24 keeps its invariant but **loses its discrimination** for the original ER-STATE-07
defect, because no parameter bears latency any anymore; the test says so rather than implying otherwise.

### Validation

Preflight exit 0 with every stage run (the citation gate first exited 1 on anchors this change
shifted; re-anchored on both bases, six `DELIBERATE_REAIMS` targets retargeted and two new
transitions declared for ADR-0003, whose cited lines were themselves rewritten). DSP **304 checks /
0** (was 282; Test 52 adds 22, of which 9 fail pre-change), state **1506 / 0**, both under GCC 13
and again under the clean gcc-16 `-flto` build. `check-docs` 118 files clean; realtime lint 47 files
/ 0 violations; citation self-test 176 cases. Clang-22 and GCC-16 warning gates green on clean
builds — and the **Clang baseline SHRANK 3 → 1** for `-Wswitch-enum` in `AnamorphEngine.cpp`, since
folding the two latency switches into `osLatencyFor` with every enumerator named removed two
accepted sites. No workflow, no CMake structure and no GCC baseline changed.

---

## Round 27 — 2026-09-03 — a repair that changed nothing on screen was not written to the file

One fix. It completes ER-STATE-21's durability contract for the APVTS parameter family rather than
redesigning it.

### ER-STATE-25 — FIXED — the serialized repair was gated on the LIVE value moving

**Reproduced before any change.** `channelMode`, `<PARAM value="abc"/>` with `raw` removed, restored
into a fresh instance:

| | pre-fix | post-fix |
|---|---|---|
| live parameter | default (correct) | default (correct) |
| live APVTS `@value` | **`"abc"`** | `"0.0"` |
| the re-saved session's `@value` | **`"abc"`** | `"0.0"` |
| after a SECOND poison → restore → save cycle | **`"abc"`** | `"0.0"` |

The live value was never the problem, which is why "restore → parameter == default" passes on the
broken build and cannot be the assertion a test rests on.

**Root cause, exactly.** `applyNorm`'s tree write-back sat inside

```cpp
if (! (std::abs (norm - rp->getValue()) <= 1.0e-6f))   // src/PluginProcessor.cpp:522
```

so the durability of a repair depended on an unrelated fact: whether the corrupt file happened to
resolve to the value already loaded. **It usually does.** `apvts.replaceState` runs first and pushes
`@value` through JUCE's own parser, where unusable text reads as the denormalised **0**; `applyNorm`
then computes `norm` = the parameter **default**, because `SerializedNumber` refuses the same text.
So the gate is false precisely when `convertTo0to1(0) == getDefaultValue()` — true of every
parameter whose range starts at its default (Drive 0..24 dB default 0, Amount 0..1 default 0,
Channel Mode's first choice, …). The finding's own summary is exact.

**Which categories share it — checked, not assumed.**

| input | pre-fix | shares the defect? |
|---|---|---|
| `value="nan"` | gate TRUE (NaN makes the comparison false, so `!` is true) | no — this is State test 20's case, and why it passed |
| `value="abc"` where the default coincides | gate false | **yes** |
| `raw="-7"` (usable, out of range) | clamps to the value in force, gate false | **yes** — measured, `raw` stayed `"-7"` |
| valid text equal to the default | gate false, and correctly so | no — nothing to repair |
| stepped parameter snapping `"0.4"` | gate may fire | no — snapping is the parameter doing its job on a legitimate value |

**The fix: separate "the input was repaired" from "the live value moved".**

```cpp
const bool valueMoves = ! (std::abs (norm - rp->getValue()) <= 1.0e-6f);
if (valueMoves) { ...setValue / atomic, exactly as before... }

if (repaired)                                  // NOT conditional on valueMoves
    if (auto node = apvts.state.getChildWithProperty ("id", rp->paramID); node.isValid())
    {
        node.setProperty ("value", rp->convertFrom0to1 (norm), nullptr);
        if (node.hasProperty ("raw")) node.setProperty ("raw", norm, nullptr);
    }
```

`repaired` is decided by the caller **on the INPUT, before any clamp hides the evidence**: text that
is not a usable number, or a usable one outside the field's range (`raw` is normalised, so 0..1; a
`value` is checked against `getNormalisableRange()`). The non-finite guard inside `applyNorm` sets it
too. Snapping deliberately does not.

**Why this preserves valid unchanged values.** The condition is the *provenance* of the value, not
its equality with anything, so a genuinely valid `"0.000000"` on a default-valued parameter takes no
branch at all and its text is left byte-identical — asserted as a control leg. This is the
alternative to the "always rewrite every parameter" behaviour the round warned against, and it keeps
the existing fast path intact.

**Why both attributes.** `raw` is re-stamped from the live parameter by `copyStateWithRawValues` on
every save, so a corrupt `raw` cannot reach a *file* — but it can sit in the live tree, which A/B
slots and undo snapshots copy and which the next restore **prefers** over `value`. Writing both on
repair makes the live tree canonical at the moment of the repair rather than at the next save.

**No new notification.** The tree write fires `valueTreePropertyChanged → setNewState →
setDenormalisedValue`, whose `approximatelyEqual` early-return sees the value the parameter already
holds and stops — no host notification, no gesture, no recursion. That is what lets it run outside
the value-moved branch without becoming a host-visible edit, and `silentSoundChange` stays gated on
the value actually moving.

**Regression: State test 36, 30 checks, 11 failing pre-fix.** It asserts on the serialized artefact —
the live tree `copyState()` reads and the bytes `getStateInformation` emits — and runs the full
poison → restore → save → reload cycle **twice**, so corruption cannot survive one cycle and return
on the next. The victim parameter is **searched for** by the arithmetic precondition
(`convertTo0to1(0) == getDefaultValue()`) rather than hard-coded, so the test survives a range
change. Three boundary legs: a valid default-valued text stays exactly as written, an out-of-range
`raw` is rewritten canonically, and a malformed `raw` beside a valid `value` still falls back to the
value (§6's contract, unchanged).

### The focused audit found the defect at exactly one site

Every other user of the same repair mechanism was read:

| site | verdict |
|---|---|
| `InternalState::restoreState` (`repairedValue`) | writes **every** Setting unconditionally — no equality gate, so ER-STATE-21's Policy B was already durable |
| `PresetManager::applySoundTree` (`readSerializedValue`) | applies through `setValueNotifyingHost`, which fires the adapter's flush, so `copyState()` sees it; and a preset file on disk is not the plug-in's to rewrite |
| `migrateFromLegacyApvts` | writes the clamped value unconditionally |
| `readSlot` (A/B) | accepts or rejects a whole tree; no per-field gate |

No second instance, so nothing else was touched.

### No new race class

The change adds no shared state and no new thread pairing — one extra `ValueTree::setProperty` on
the restore path, which already writes that tree. Nothing for D-2 to absorb; its four measured race
classes stand, deferred.

### Carried unchanged

ER-STATE-21 FIXED (contract completed, not redesigned) · ER-STATE-24 and ER-GUI-06 FIXED and
untouched · ER-DSP-10, ER-DSP-11 FIXED · drag recovery REFUTED · D-1 approved and implemented ·
D-2 / RISK-007 deferred · RISK-008 real-host validated for REAPER with its residual, **no host test
performed** · cross-file realtime-lint boundary unchanged.

## Round 26 — 2026-09-03 — a preset the plug-in refused still ducked the audio

One fix, closing the last actionable Review Bug. It is the other half of round 24: that round made a
refused preset a no-op for STATE, and this one makes it a no-op for AUDIO.

### ER-GUI-06 — FIXED — the duck was raised by the caller, before the load could refuse

**Reproduced before any change, through the REAL editor's production `onClick`.** An engaged widener
on a mono stimulus, settled over 60 blocks, then the Next-preset button clicked so `step(+1)` lands
on a foreign-rooted row that round 24's `parseSoundFile` refuses:

| | side RMS of the next block |
|---|---|
| after a REFUSED preset step | **0.201786** |
| control, identical run with no click | **0.443549** |
| ratio | **0.4549** |

The widener's stereo width fell to 45 % of its settled level for ~32 ms, on a load that applied
nothing. Round 24's state contract held throughout — sound, preset name and menu tick all unchanged
— which is precisely what made this the *audio* half of the same bug.

**Root cause, and the exact order.** All three editor call sites raised the duck before asking the
manager to load:

```
PluginEditor:360/361   requestDuck(); presets.step (±1);      -> step() calls load(index)
PluginEditor:2064-65   requestDuck(); presets.load (r - 1);   -> load() returns void
PluginEditor:2090-91   requestDuck(); if (presets.loadFile (file)) { ...knob sweep only... }
```

`load()` returns `void`, so the menu site could not have checked; the chooser site *did* check
`loadFile`'s result, but only to decide whether to sweep the knobs — the duck was already
unconditional. When `parseSoundFile` then refused, `duckRequest` stayed set and the next
`processBlock` opened a forced duck with nothing to mask, dry-filling real audio. It is the same
fault `AnamorphEngine::primeParameters` already documents in its own header for the activation
route (ER-DSP-06 / Test 48) — "a duck whose swap already happened … masks nothing and merely
dry-fills" — arriving here by a different path.

**The fix: move the request to the load path's own success boundary, and delete it from the callers.**

```cpp
presets.onAboutToLoad = [this] { pollUndoCoalesce(); engine.requestDuck(); };
```

`onAboutToLoad` is the one point that is **both** after every check that can refuse — the missing
factory id, the unparsable file, and since ER-STATE-24 the foreign root — **and** before the first
`setValueNotifyingHost`. Both halves matter, and they are why the fix is not "duck after a
successful load": raising it afterwards would leave a window in which an audio block hears the
swapped parameters unmasked, weakening a guarantee the round explicitly protects.

**Why this boundary rather than a success flag on the callers.** One line fixes **all three** call
sites at once, including `step()`, which the finding did not name; the two loaders cannot drift
apart because neither owns the decision any more; no validation is duplicated; no new mechanism and
no new asynchrony is introduced; and every future caller of `load`/`loadFile` inherits the invariant
instead of having to remember it. `onAboutToLoad` fires from nowhere else — `onSaved` is a separate
hook, and a save must not duck.

**Regression: State test 35, 16 checks, 2 failing pre-fix.** It drives the real editor, because the
defect was in the editor's ordering and nothing below it could see the bug: `presetPrev`/`presetNext`
carry `setComponentID ("presetnav")` and differ by button text, so the child walk reaches the actual
production `onClick`. The harness writes two files whose names put the foreign row immediately after
a valid one — **asserted, not assumed** — loads the valid one so `currentIndex()` points at it, then
clicks Next. The observable is Test 48's, chosen for the same reason: a MONO stimulus into an engaged
widener makes every trace of output side energy the widener's own, so a dry fill collapses it, and
twin processors driven identically are compared to each other rather than to a threshold. **Both
directions are asserted**, which is what stops "delete the duck" from passing: a refused step must
leave the next block identical to the un-clicked control (to 1e-9), and a successful `loadFile` must
still collapse it. Post-fix the two numbers swap places exactly — 0.443549 = 0.443549 on the refused
path, 0.201786 against a 0.443549 control on the successful one.

**Malformed files take the same invariant**, asserted in the same test: `parseSoundFile` refuses an
unparsable document at the identical point, so it too reaches no duck. The round asked whether the
no-side-effect rule should apply to all failed loads rather than foreign roots only — it does, and it
falls out of the fix rather than needing a second case.

**No new race class.** `requestDuck` is a relaxed atomic store, moved between two functions that both
run on the message thread (`PresetManager`'s loaders are message-thread only by contract). No new
shared state, no new thread pairing, nothing for D-2 to absorb.

### Carried unchanged

ER-STATE-24 (foreign-root rejection, `parseSoundFile`, identity preservation) **untouched and still
green**; ER-DSP-10, ER-DSP-11, ER-STATE-21 FIXED and untouched; drag recovery REFUTED; D-1 approved
and implemented; D-2 / RISK-007 deferred; RISK-008 real-host validated for REAPER with its
host-specific residual, **no host test performed**; the cross-file realtime-lint boundary unchanged.

## Round 25 — 2026-09-03 — a minimal `[0.9.6]` Change Log correction pass

**Documentation only. No source, test, workflow, CMake or baseline file changed.** The release date
moves to **2026-09-03** and four wording claims are corrected, each because the current
implementation contradicts them.

| entry | claim | why it was wrong |
|---|---|---|
| ER-DSP-11 (balance) | "The addition now stays in range" | the float `llSlow + rrSlow` **still overflows**; round 23 recovers the denominator in double on that overflow. Now: "When that addition does run out of range, the split is now worked out at higher precision instead" |
| ER-DSP-10 (phase) | "that energy calculation ran out of range" | each energy stays finite; it is their **product** `ll * rr` that overflows. Now: "the two energies multiplied together ran out of range internally, though each of them on its own was still fine" |
| ER-DSP-10 (phase) | "The calculation now stays in range" | same overstatement as ER-DSP-11's. Now: "That multiplication is now worked out at higher precision when it runs out of range" |
| ER-STATE-24 (foreign preset) | "both accept any file you point them at" | present tense, and after round 24 they do **not** accept any file. Now: "both let you point them at any file on your machine" — true before and after, and unambiguous |

**A fourth correction the audit turned up, and the only one not named in the brief.** The round-17
Scope Persistence entry ended "the stored value itself is untouched". That was true when written and
is now contradicted **by an entry higher in the same section**: round 18's Policy B repairs
`int_scopePersist` during restore and persists the repaired value (`InternalState::restoreState`
writes `repairedValue(...)` for every Setting, and that tree is what the next save emits). Corrected
to "the stored value is repaired separately, by the Settings repair described above" — the consumer
guard this entry is about is unchanged, only the sentence about the file.

**Audited and left alone.** Every test number the section cites resolves in the current tree (DSP 43,
44, 45, 47, 48, 49, 50, 51; State 4, 16-24, 26-34). The measured figures cross-check against the
worklog and `HANDOVER.md` (A/B Level-Match −1.040 / −2.438 dB; the activation duck's 0.4 %; Test 48's
0.003 of settled; Test 47's −6 dB adopted as 0.0; the round-20 fade ratios 0.17 / 0.09 / 0.29 / 0.39
/ 0.35). The 1.3e19 and 4.3e9 thresholds are correct as stated — for the balance sum at least one
channel must reach ~1.3e19, since two channels below it cannot sum past `FLT_MAX`. Historical
figures were checked for contradiction, not re-measured. **D-2 / RISK-007, RISK-008 and KI-015 are
absent from the Change Log and stay absent**: none is a fixed, user-visible change, and the section
has no Known Issues structure that would require them.

## Round 24 — 2026-09-02 — a foreign preset was loaded as if it were ours

One fix, closing the last actionable Review Bug. The finding was real, and the mechanism is one step
worse than the review's wording.

### ER-STATE-24 — FIXED — a valid preset from another plug-in replaced the current sound

**Reproduced before any change**, against a five-parameter non-default sound, loading a well-formed
`<SomeOtherPluginPreset>` carrying two `PARAM` children with *our* `id` spellings:

```
  before foreign load: drive=0.3100 width=0.7700 algorithm=0.6600 monoMakerFreq=0.4200 chorusRate=0.5800
  after  foreign load: loadFile returned TRUE | drive=0.0396(default 0.0000) width=0.0250(default 0.5000)
                       algorithm=0.3333(default 0.3333) monoMakerFreq=0.5000(default 0.5000)
                       chorusRate=0.3832(default 0.3832)
```

**The review said "treats every missing parameter as its default". That is half of it.** `drive` and
`width` did NOT become their defaults — they became **the foreign file's values** (0.95 and 0.05
plain, i.e. 0.0396 and 0.0250 normalised). `applySoundTree` resolves each parameter with
`getChildWithProperty ("id", …)`, which searches by PROPERTY and does not care what the root is
called, so a foreign document does two wrong things at once: every parameter it *names* is adopted,
and every parameter it does not name takes the "absent means default" branch written for a genuinely
missing `PARAM` node. Both loaders then reported success. `loadFile` returned `true`; `load(index)`
opened the undo bracket and adopted the file's identity.

**Neither loader validated the root.** `load(int)` checked only `parseXML != nullptr`; `loadFile`
the same. The only validation anywhere on the path was `readSerializedValue`, which guards the value
*inside* a matched child — it never had the chance to ask whether the document was ours.

**The contract, taken from the repository rather than invented.** ER-STATE-02 settled exactly this
question for A/B slot payloads: `readSlot`'s `adoptIfAnamorph` accepts only `apvts.state.getType()`
and refuses a foreign-typed tree *precisely as it refuses an unparsable one*. A preset file is the
same kind of payload asking the same question, so it gets the same answer rather than a second one —
no new API, no maintainer decision needed. Concretely: **`loadFile` returns `false`; `load(index)` is
a clean no-op**; sound, preset name and menu tick all untouched.

**One shared choke point, not two special cases.** Both loaders already did the same three steps —
parse, check null, `applySoundTree` — so the acceptance test replaces the null check in one new
private helper that both resolve through:

```cpp
juce::ValueTree PresetManager::parseSoundFile (const juce::File& f) const
{
    if (auto xml = juce::parseXML (f))
        if (auto t = juce::ValueTree::fromXml (*xml); t.hasType (apvts.state.getType()))
            return t;
    return {};
}
```

In `load(int)` it lands **before `onAboutToLoad()`**, which is where that function's own comment
already requires every failure to be resolved ("a failure must be a clean no-op, never an
`onAboutToLoad()` with no matching `onLoaded()`"). The rule cannot hold on one path and not the
other, because there is now only one path.

**Why the root and not the fallback (the boundary the round asked about).** Making the
per-parameter fallback keep the current value instead would have left a foreign preset **accepted
and merely inert** — the loader still reporting success, the identity still moving, and any future
caller of `applySoundTree` still unprotected. The distinction had to be *foreign preset → rejected
as foreign*. `applySoundTree` also cannot host the check: it looks parameters up by property, so it
genuinely cannot tell a foreign tree from ours, and it returns `void` so it could not report the
rejection to `loadFile`'s `bool` anyway. Its header now states the precondition instead of carrying
a check that could never be reached.

**Regression: State test 34, 29 checks, 6 of them failing against the pre-fix build.** The sentinel
is **five** parameters, each asserted to differ from its own default — the failure mode *is*
"everything becomes its default", so a one-parameter probe could pass by coincidence. Preservation
is compared **exactly**, against what the parameters actually hold after being set rather than
against the literals requested: a stepped parameter quantises what it stores (`chorusRate` moves by
3e-5), and the claim under test is preservation, not equality with a literal. The foreign document
deliberately carries children with our `id` spellings, so the rejection cannot be attributed to
unrecognisable children. **Both loaders** are exercised — the chooser path and the menu path, the
latter after a `refresh()` that is itself asserted to have listed the file — and the whole rule is
re-run from a **second** distinct sound. Three legs guard what must not change: malformed XML keeps
its existing rejection, a valid `<ANAMORPH>` root carrying one `PARAM` still adopts that one and
still defaults the rest, and a full valid preset still round-trips from either starting state.

### No new race class

The change is a file parse and a type test on the message thread, inside a `const` helper with no
shared state; `applySoundTree`'s behaviour for accepted trees is byte-identical. Nothing in the
concurrency surface moved, so no probe was re-run and **no duplicate D-2 finding was filed**.
**D-2 / RISK-007 stays deferred** with its four measured race classes unchanged.

### A count correction, measured rather than carried

The state suite's *test* count was one ahead in every document. Measured from `main`'s registered
test functions: **32 at `HEAD`** while the documents said 33 — so State test 34's arrival makes the
true figure **33**, not 34. Corrected in `TESTING.md`, `TESTING_POLICY.md`, `README.md`,
`RELEASE_HARDENING_PLAN.md` and `HANDOVER.md`, with the counting method recorded beside it. The
*check* count was never wrong: 1431 → **1460**.

### Carried unchanged

**RISK-008** — real-host validated for REAPER; host-specific residual unverified; no host test
performed. **ER-DSP-10**, **ER-DSP-11**, **ER-STATE-21** all FIXED and untouched — in particular the
foreign-root test is kept clear of ER-STATE-21's territory: this is about the ROOT, that is about
malformed fields under a valid root, and State test 34 asserts them apart. **Drag recovery** REFUTED,
**D-1** approved and implemented, the cross-file realtime-lint boundary unchanged.

## Round 23 — 2026-09-02 — the other overflow in the same class: an unequal pair read as dead centre

One fix. It is in the same file and the same regime as round 21's, and it is a **different
operation** — which is exactly why round 21 did not catch it, and why round 21's own note about it
was wrong. That note is corrected in place above rather than edited away.

### ER-DSP-11 — FIXED — a finite `ll + rr` overflow erased the channel imbalance

**Reproduced first, with the module's own code.** `CorrelationMeter`, 48 kHz, settled over 400,000
samples (~50 time constants of the 600 ms slow pole):

| input `l` / `r` | published balance | true balance | verdict |
|---|---|---|---|
| 0.5 / 0.5 | 0 | 0 | correct |
| 0.5 / 0.25 | −0.6 | −0.6 | correct |
| 0.25 / 0.5 | +0.6 | +0.6 | correct |
| 1.0e9 / 0.5e9 | −0.6 | −0.6 | correct |
| 1.5e19 / 1.5e19 | 0 | 0 | correct (equal channels) |
| **1.8e19 / 1.0e19** | **−0** | **−0.528302** | a heavily L-weighted pair read as DEAD CENTRE |
| **1.0e19 / 1.8e19** | **0** | **+0.528302** | the mirror, same failure |
| 1.8e19 / 0.2e19 | −0.975616 | −0.975610 | correct — and this is the discriminator |

**The complete arithmetic path, instrumented** (`l = 1.8e19`, `r = 1.0e19`):

| step | value | finite? |
|---|---|---|
| per-sample squares `l*l`, `r*r` | 3.24000014e38, 9.99999968e37 | yes |
| accumulators `llSlow`, `rrSlow` | 3.23707947e38, 9.98539635e37 | **yes — `sanitize()` ACCEPTS, never fires** |
| numerator `rrSlow - llSlow` | −2.23853994e38 | **yes — the imbalance is intact here** |
| **`llSlow + rrSlow` (float add)** | **+Inf** (exactly 4.2356191e38 in double, vs `FLT_MAX` 3.40282347e38) | **NO ← the overflow, and the only one** |
| guard `sum > 1.0e-12f` | **true** — does not fire | — |
| `num / sum` | **−0** | yes |
| the same expression in double | **−0.528503598** | yes |

**Root cause.** `ll` and `rr` are mean-square values, so each scales as the square of the input. The
balance divides by their **sum**, and a float add of two finite mean-squares leaves float once the
sum passes `FLT_MAX`: from steady input above **1.30438174e19** for equal channels, and anywhere an
unequal pair sums past it, up to the **1.84467435e19** at which `l*l` would itself stop being
finite. Two things then conspire to make the failure silent rather than loud: the numerator cannot
overflow — `rr − ll` lies in `[−ll, rr]` for non-negative operands, so it stays finite and carries
the *whole* imbalance — and `+Inf` sails past the 1e-12 small-signal guard, leaving `finite / +Inf`,
which is a perfectly well-formed **0**. Zero is the meter's value for "perfectly centred".

**This is NOT ER-DSP-10, and the two must not be merged.** That defect is the phase meter's
`ll * rr` PRODUCT inside `correlation()`; this is the balance's `ll + rr` SUM in `publish()`. Fixing
the product did nothing for the sum — verified, because the round-21 build reproduces this defect at
full strength. Nor is it the Test 45 poison class: every accumulator here is finite and `sanitize()`
never fires, which the regression asserts rather than assumes.

**The fix, at the overflowing operation and nowhere else.**

```cpp
const float sum = llSlow + rrSlow;
float bal;
if (! std::isfinite (sum))
{
    const double d = (double) llSlow + (double) rrSlow;   // >= FLT_MAX here, never 0
    bal = (float) ((double) (rrSlow - llSlow) / d);
}
else
    bal = sum > 1.0e-12f ? (rrSlow - llSlow) / sum : 0.0f;
```

Only the **sum** moves to double, only when the float sum is non-finite. Two finite floats sum to at
most ~6.8e38, nowhere near `DBL_MAX`; the quotient is then bounded by 1 in magnitude because
`|rr − ll| ≤ rr + ll` for non-negative operands, and the existing clamp stays as the backstop. The
float expression is untouched character for character and the double path is unreachable while the
sum is finite.

**Normal range preserved bit-for-bit, measured not asserted.** Pre- and post-fix expressions
compared over **19,671,802** randomised finite-sum energy pairs spanning `ll`/`rr` from 1e-40 to
1e38: **zero differing bit patterns.**

**Scale invariance restored, swept across the edge itself.** At a fixed 3:1 energy ratio the true
balance is −0.5 at every scale. The pre-fix build holds −0.5 up to `s = 8.5e37` and drops to **−0.0**
at `s = 8.6e37`, the first point where the sum stops being finite; the fixed build holds −0.5 across
the whole sweep. That is the defect and its repair in one measurement.

**Regression: DSP Test 51, 12 checks, built on the overflow edge rather than on level.** The
discriminator is `1.8e19 / 0.2e19`: *more* lopsided than the failing pair, sum 3.277e38 (under
`FLT_MAX`), and correct in **both** builds — so level is demonstrably not the variable, and a fix
that merely rejected loud audio would fail it. The defect legs assert the **value** in both
directions, because `−0.0` is finite *and* symmetric with `+0.0`: neither "is it finite" nor "does
swapping the channels flip the sign" would have caught this, and both of those checks pass against
the broken build. Three normal-range controls at three distinct values kill the degenerate fixes —
"always 0" fails the unequal legs, "always non-zero" fails the balanced leg, "always one-sided"
fails one direction. A premise leg asserts the accumulators are healthy (a perfectly correlated
extreme pair must still read +1, which a flushed accumulator could not), an **ER-DSP-10-intact** leg
re-checks the phase reading at Test 50's own input, and a final leg re-asserts Test 45's poison
contract. **2 of the 12 fail against the pre-fix build, 0 after.**

**`energy` inspected again and left alone, this time by tracing its consumer rather than by
assertion.** `llFast + rrFast` also reaches +Inf in this regime, and its only reader is
`gui/CorrelationMeter.cpp`'s `source.getEnergy() < 6.0e-9f` silence predicate — `+Inf < 6e-9` is
false, so the meter is correctly "not silent", the target is then the (now truthful) balance or the
(finite) correlation, and the glide arithmetic never sees the infinity. No demonstrated defect, and
this time the claim rests on the consumer.

### The round-21 record, corrected in place

Round 21 inspected this same sum and wrote that `balance = 0` "still means centred, which is what an
equal-energy pair should read". The threshold in that note was right; the conclusion was wrong, and
wrong in the way that matters — it holds only when the channels really ARE equal, which is the one
case a balance meter is not for. Both copies of that note (the round-21 worklog section above and its
`DOCUMENTATION_COVERAGE.md` entry) now carry the correction beside them, kept as written with the
error named rather than edited away.

### Carried unchanged

- **D-2 / RISK-007** — deferred. This round produced no new evidence: `src/PluginProcessor.cpp` and
  `.h` are still unchanged since round 16, and the only source touched here is a static balance
  computation inside `publish()` with no shared state. No probe was re-run because nothing in the
  concurrency surface moved, and no duplicate finding was filed.
- **RISK-008** — real-host validated for REAPER; host-specific residual unverified. **No host test
  performed**, and no production change.
- **ER-STATE-21** FIXED · **drag recovery** REFUTED · **D-1** approved and implemented ·
  **cross-file realtime lint** boundary unchanged (47 files, 0 violations, self-test 93).

## Round 22 — 2026-09-02 — the `docs` gate went red on a line that began with a pipe, and on a filtered preflight

A CI round. **No production code changed, and none was justified.** One real documentation defect,
one validation-procedure defect that let it through, and a settled-record sweep that found nothing
to correct.

### ER-CI-06 — FIXED — `check-docs` failed on a table fragment in `DOCUMENTATION_COVERAGE.md`

**The exact failure**, `docs` job → step **"Lint documentation structure"**, run 33617522593, job
100206715110, head `f42a1d8`:

```
docs/DOCUMENTATION_COVERAGE.md:7365: table fragment with no header/separator (1 pipe line(s))
  -- a block was inserted mid-table, or the separator row is missing

check-docs: 1 finding(s) across 117 file(s).
##[error]Process completed with exit code 1.
```

**Repository content, not a stale run, not the environment.** It reproduces on a bare local
checkout at exit 1, and the classification was checked rather than assumed:

| candidate | ruled out by |
|---|---|
| stale / superseded run | the finding reproduces on the current tree, locally, today |
| workflow or environment difference | the job runs `python3 scripts/check-docs.py` with no environment input; the same command fails identically here |
| citation or count drift | the citation gate is a different checker and is clean on both bases |
| the checker being wrong | it is right — see below |
| a duplicate-run artifact | see the "two runs, one SHA" note below |

**Root cause.** Round 21's entry in `DOCUMENTATION_COVERAGE.md` wrapped the absolute-value notation
`|l| ≈ 1.3e19` onto the START of a line. In Markdown a line beginning with `|` is a table row, so
that lone line is a table with no header and no separator — which is what the checker reports, and
what a renderer would actually do with it. **The gate is correct and was not touched.** The prose
was reflowed to say "an input magnitude of ≈ 1.3e19" instead, which removes the line-initial pipe
without changing the claim. A repo-wide sweep for the same hazard
(`grep -rn '^|[a-zA-Z]' --include='*.md'`) finds no other instance.

### ER-CI-07 — the reason round 21 reported a green preflight while this was failing

Worth its own entry, because the documentation defect above is trivial and this is not.

`scripts/preflight.sh` is `set -euo pipefail` and `check-docs.py` is its **SECOND** command. So on
the round-21 tree preflight aborted at that second command — meaning not only that the finding was
real, but that **every later stage of that invocation never ran**: the portability lint, the
realtime lint, the four warning-gate self-tests, the ABI floor, the citation gate against all three
bases, and both suites. The round nonetheless reported "preflight green".

**The mechanism was the reporting, not the script.** The round ran
`./scripts/preflight.sh 2>&1 | grep -iE "FAIL|error|violation|drift|no longer point|::error" ; echo "PREFLIGHT-DONE"`.
The finding's wording — "table fragment with no header/separator" — contains none of those tokens,
so the filter printed nothing; the pipe replaced preflight's exit status with `grep`'s; and the
trailing `echo` ran unconditionally. Three independent hatches, all opened by one habit. (The
round's substance survived only by luck: the citation gate, the realtime lint, both warning gates
and both suites were also run as separate direct commands, so their results in that report stand.
The word "preflight" in it does not.)

**Fixed where the next reader will meet it**, and by strengthening nothing away: a paragraph in
`preflight.sh`'s own header — beside its existing "NO SILENT SKIPS" rule, which is the same
argument — and a matching one in `CI_CD.md` §preflight, both stating that the script fails fast, so
a non-zero exit means the stages after it are an UNKNOWN result rather than a green one, and that a
filtered view of its output is not a result. **No check was weakened, no exclusion widened, no
workflow changed.**

### Two runs on one SHA — the thing that looks like flakiness and is not

`f42a1d8` carries two `Build & Validate` runs three seconds apart: 33617522593 (**failure**) and
33617526870 (**success**). They are not a retry and not a flake — the first is the `push` event and
the second the `pull_request` event, and in the PR run **`docs` is skipped**, along with every job
except `merge-check` (12 of 13 skipped; the run finished in four minutes against the push run's
twenty). The workflow does that deliberately so a PR does not duplicate the whole matrix. **A green
`pull_request` run therefore says nothing at all about `docs`** — which is exactly the misreading
the round-21 report would have invited, and is recorded here so a later round does not make it.

### Nothing was hidden behind the red gate

Every other job in the failing run passed on the same commit: `source-lint`, `linux` (both suites,
pluginval ×3 in both modes, the Clang warning gate, the ABI floor), `linux-lto-tests` (the GCC
warning gate + LTO suites), `sanitizers` (ASan/UBSan + valgrind), `realtime` (RTSan), `fuzz`,
`windows`, `windows-avx2-ab`, `macos`, `macos-intel`, `macos-crossslice`. The failure was confined
to documentation structure, and no engineering defect was masked by it.

### Settled-record sweep — nothing to correct

Checked against the LIVE documents, and each was already right:

| item | recorded state | verified |
|---|---|---|
| ER-DSP-10 | FIXED (round 21) | `DSP_ALGORITHMS.md`, `TESTING.md` Test 50, CHANGELOG |
| ER-STATE-21 | FIXED, Policy B, repaired value persisted | `SERIALIZATION_REGISTRY.md`; the only other hits are dated round-16/17 history |
| Drag recovery / ER-GUI-05 | REFUTED | no document presents it as open |
| D-1 | APPROVED / IMPLEMENTED | `THREADING_POLICY.md`, `LATENCY_MODEL.md`, KI-027 |
| D-2 / RISK-007 | DEFERRED, four measured races | `FUTURE_RISKS.md`, incl. round 21's re-measurement |
| RISK-008 | real-host validated for REAPER; host-specific residual unverified | register row, Likelihood bullet, and the probe's own printed EVIDENCE LIMIT |

The three surviving "no host available" strings are all **explicitly dated round-18 history** or
scoped to the review harness ("no real Linux VST3 host is available *here*"), sitting beneath the
round-19 real-host bullet that supersedes them — the disposition round 19 chose deliberately over
deleting them. Not rewritten.

## Round 21 — 2026-09-02 — the phase meter's own arithmetic overflowed on extreme-but-finite audio

One fix and one re-measurement. The fix is numerical, at the exact operation that overflows.

### ER-DSP-10 — FIXED — extreme BUT FINITE audio made the phase meter read "fully decorrelated"

**Reproduced before anything was changed**, with the module's own code and no test harness in the
way. `CorrelationMeter`, 48 kHz, settled over 200,000 samples of a steady value:

| input `l = r` | published `fast` | published `slow` | verdict |
|---|---|---|---|
| 0.5 | 1 | 1 | correct |
| 1.0e9 | 1 | 1 | correct |
| 4.0e9 | 1 | 1 | correct |
| **1.0e10** | **0** | **0** | a perfectly correlated MONO signal read as fully decorrelated |
| 1.0e10 / −1.0e10 | **−0** | **−0** | anti-phase read as decorrelated, not −1 |
| 1.8e19 | **0** | **0** | as above |

**The complete arithmetic path, instrumented at every step** (`l = r = 1e10`):

| step | value | finite? |
|---|---|---|
| input samples | 1e10 | yes |
| per-sample products `l*r`, `l*l`, `r*r` | 1.00000002e20 | yes |
| accumulators `lrFast`, `llFast`, `rrFast` | 9.99746693e19 | **yes — so `sanitize()` ACCEPTS them and never fires** |
| `ll * rr` (float) | **+Inf** | **NO — the overflow, and the only one** |
| `std::sqrt(ll * rr)` | +Inf | no |
| `denom < 1.0e-12f` | **false** — so the small-signal guard does not catch it | — |
| `lr / denom` | **0** | yes |
| the same expression in double | **1** | yes |

**Root cause, stated precisely.** `ll` and `rr` are *mean-square* values, so they scale as the
SQUARE of the input; their product scales as the fourth power. `float` runs out at
`FLT_MAX = 3.40282347e38`, so the product overflows once `ll = rr` exceeds
`√FLT_MAX = 1.84467435e19`, i.e. from steady input above **|l| = 4.29496723e9**. Nothing before that
point is non-finite, which is exactly why the existing guard is no help: `sanitize()` is the
recovery for a poisoned ACCUMULATOR (ER-DSP-04, Test 45) and the accumulators here are healthy. And
the failure is silent rather than loud — `+Inf` sails past the `< 1e-12` small-signal floor and
`lr / +Inf` is a perfectly finite **0.0**, which is both a plausible-looking meter reading and the
exact opposite of the truth.

**Reachability is not hypothetical.** The tap runs on the monitored output
(`AnamorphEngine::process`, the metering tap), and the engine's NaN/Inf self-heal says of itself, in
the source, "This is NOT a level limiter: it touches ONLY non-finite samples, so valid audio
(however loud) is passed through untouched". Under Bypass the tap additionally sees the host's raw
buffer, which is why the sanitize guard had to live in `publish()` in the first place.

**The fix, at the overflowing operation and nowhere else.**

```cpp
if (! std::isfinite (ll * rr))
{
    const double d = std::sqrt ((double) ll * (double) rr);
    const double c = (double) lr / d;
    return c < -1.0 ? -1.0f : (c > 1.0 ? 1.0f : (float) c);
}
// ...the original float expression, untouched...
```

**Why double, and why only inside the branch (requirement 14).** The double product of two finite
floats is *exact* — a float significand is 24 bits, so a double holds their 48-bit product with no
rounding — and at most ~1.16e77, which needs no case analysis at all. The float-only alternative,
re-associating as `sqrt(ll)*sqrt(rr)`, was **measured** rather than assumed: sweeping every
representable pair in float's top binade against `FLT_MAX` and against itself (2 × 8,388,608 pairs)
it never overflows, largest exact product 3.402823264e38 against `FLT_MAX` 3.402823466e38. So it
would also have worked — but its safety rests on which way a correctly rounded `sqrt` happens to
land near the top of the range, which is a worse thing to depend on than an exact product, and it
costs a second `sqrt`. Putting the double *inside* the branch is what keeps the promise that
matters: the ordinary range never reaches it.

**Normal range preserved bit-for-bit, measured not asserted.** The pre-fix and post-fix expressions
were compared over **19,995,466** randomised finite-product triples, `ll`/`rr` log-uniform from
1e-40 to 1e19 with `lr` swept across ±1.2·√(ll·rr): **zero differing bit patterns.** By construction
too — the float expression is unchanged character for character and the branch is unreachable unless
`ll * rr` is already +Inf.

**Regression: DSP Test 50, 13 checks, built on the threshold rather than on large values.** 4.0e9 and
5.0e9 are one binade apart and differ in exactly one respect — whether `ll * rr` overflows — and both
must read +1. The pre-fix build prints the mechanism in one line:
`correlated mono: 4.0e9 -> fast 1.0000 | 5.0e9 -> fast 0.0000`. The premise is asserted rather than
assumed: `getEnergy()` (which IS `llFast + rrFast`) must read finite and > 1e19, so a flushed
accumulator would fail the setup leg and the test could not be mistaken for Test 45's. A
scale-invariance leg requires 0.5 and 1.0e10 to agree to 1e-6 — the contract the overflow actually
violated. **Three normal-range controls** refuse the degenerate fix, including a decorrelated input
that must read ~0, which "always return +1" cannot pass; and a final leg re-asserts Test 45's poison
contract so the new branch cannot have rescued a genuinely non-finite sample instead of healing it.
**6 of the 13 fail against the pre-fix build, 0 after.**

**Inspected and deliberately left alone.** `balance` and `energy` use `llSlow + rrSlow` and
`llFast + rrFast`, which are SUMS, not products: they overflow only above |l| ≈ 1.3e19, three orders
beyond the regime that breaks the phase reading, and at that point `energy = +Inf` still means
"playing" and `balance = 0` still means "centred", which is what an equal-energy pair should read.
Different regime, different meter, no demonstrated defect — recorded here so a later round has the
map rather than re-deriving it.

> **Corrected by round 23 (ER-DSP-11), and the paragraph is kept as written rather than edited.**
> The `energy` half holds: its only consumer is a `< 6e-9` silence predicate, which `+Inf` answers
> correctly. The `balance` half was wrong, and wrong in the way that matters — "balance = 0 still
> means centred" is true only when the channels really ARE equal, which is the one case the meter
> is not for. For an UNEQUAL pair the overflowing sum erased the imbalance: 1.8e19/1.0e19 published
> `-0.0` where the true figure is −0.5285. The map this paragraph offered was accurate about the
> threshold and wrong about the consequence.

### ER-STATE-23 (re-raised) — ENTIRELY COVERED BY THE DEFERRED D-2 / RISK-007 — no production change

The same finding as round 20's item 3, at the same source line, with one sentence added: "the
documented macOS AU race remains open." That sentence is RISK-007's own Likelihood bullet restated —
the macOS AU is where the exposure is genuinely unguarded — not new evidence.

**What was checked this round rather than carried over.**

1. **The surface has not moved.** `src/PluginProcessor.cpp` and `src/PluginProcessor.h` are unchanged
   since round 16; the code the finding names is byte-identical to what round 20 measured. Round
   20's own changes were in `InternalState.h` (called from the restore, same thread) and in the DSP
   modules (engine plain state, whose writers are `prepareToPlay` and `processBlock`); this round's
   change is inside a *static, stateless* function.
2. **The probes were re-run under ThreadSanitizer on the current tree**, not cited from last round.

| probe | reports |
|---|---|
| `--state-thread-probe` | `juce_Atomic.h:82` ×1, `PluginProcessor.cpp:990` ×1, `PluginProcessor.h:184` ×2 |
| `--state-prepare-race-probe` (restore ‖ `prepareToPlay` ‖ editor tick) | **the same four, no others** |
| `--reprepare-race-probe` | **silent** — ER-STATE-19 / D-1 remains closed |

**One-to-one correlation with what RISK-007 already records**, member by member:

| # | shared state | writer | reader | already in the register? |
|---|---|---|---|---|
| 1 | `abActive` (`src/PluginProcessor.cpp:990`) | `setStateInformation` | `canUndo()` | yes |
| 2, 3 | `abUndo[]` vector internals (`src/PluginProcessor.h:184`) | `UndoStacks::operator=` | main-thread iteration / `empty()` | yes |
| 4 | `juce::String` refcount (`juce_Atomic.h:82`) | PresetManager metadata assignment | `juce::String` copy ctor | yes |

**The premise, once more, is a category error.** `latencyUpdateRequest` has exactly three touch
points in the whole tree — one release-store on the request side and two acquire-exchanges on the
delivery side — and it publishes one bit: "a latency update is pending". No restore, A/B, preset or
engine state is written before that store for a reader to acquire through it, so "the atomic latency
values do not synchronize concurrent restore / prepare / A-B / preset / engine state" is a true
statement about a mechanism that never claimed to.

**Classification: entirely covered by deferred D-2 / RISK-007.** No mutex, no `callAsync`, no
`AsyncUpdater`, no state-architecture change; D-2 stays deferred and unreopened. Recorded as a
re-measurement under the existing RISK-007 entry rather than as a second finding.

### RISK-008 — carried unchanged

No host test performed, and none permitted. The real-host half remains the maintainer's Linux +
REAPER result from round 19, recorded with its limits.

## Round 20 — 2026-09-02 — a restored session stops fading in; a malformed boolean stops switching things on

Two fixes and one disposition. Both fixes are at the source of the defect, not at the point where it
becomes visible.

### ER-DSP-09 — FIXED — a restored non-default session opened by GLIDING into its own sound

**Reproduced first, with the product's own signal.** `AnamorphStateTests --restore-fade-probe` saves
a real non-default session, restores it into a FRESH instance in the ordinary host order (state,
then activate) and prints each module's per-block deviation over twelve blocks. Before the fix, the
first block sat at **0.17** (Haas), **0.09** (Velvet), **0.29** (Chorus), **0.39** (Dimension-D) and
**0.35** (Mono Maker crossover) of the settled figure, arriving over the next ~10–100 ms. The user
statement of that is the one in the CHANGELOG: opening a project played the *previous* settings
sliding into the saved ones.

**Root cause, at the ordering rather than the symptom.** `AnamorphEngine::prepare()` runs
`module.prepare()` for each module, and each module's own `prepare()` already snaps its smoothers.
But `updateDerived()` — which installs the restored snapshot's targets — runs *after* that, so
every module snapped to whatever targets existed BEFORE the restored session was pushed in: a fresh
instance's defaults, or the previous session's on a reused one. `reset()` then re-zeroed the chorus
blend outright. The engine's own `snapSmoothers()`, added in round 2 for exactly this class, does
not reach inside the modules.

**The fix is four calls at the end of `prepare()`**, after `snapSmoothers()`, plus the
`snapToTargets()` each module needed to expose:

| module | what it had to land | note |
|---|---|---|
| `HaasProcessor` | `currentSamples`, `currentAmount` | header-inline, mirrors its own prepare |
| `VelvetNoise` | `currentDensity`, `currentAmount` | its `prepare()` now CALLS this, so the two cannot disagree |
| `MonoMaker` | `currentFreq` + the crossover cutoff | same; the cutoff must be pushed into the filter, not just stored |
| `ChorusEngine` | `currentWet` now, `currentDepth` deferred | the depth target is in SAMPLES at the working rate, and the working rate arrives per block (`setWorkingRate`), so a one-shot `snapDepthPending` is consumed at the top of the next `processBlock` — the only place that expression exists |

**Where it is NOT placed, and why.** Not in `reset()` and not in `snapSmoothers()`: both also run at
the silent bottom of a switch duck and on the NaN self-heal, so folding the snap in would change how
LIVE edits settle — the opposite of the requirement. `prepare()` is the one moment where snapping is
unambiguously right, because all delay and filter state has just been cleared and there is nothing
for a glide to protect.

**Regression: DSP Test 49** (11 checks). Each subject is compared against a REFERENCE of the same
module settled on the same targets, so the claim is "these are the same signal" rather than a level
threshold; the whole residual belongs to the reference's asymptotic settling (~1e-8 at 200 blocks),
four orders under the 1e-5 bound and eight under the ~0.2-of-full-scale defect.

**The test's first version was not discriminating, and that is the round's methodological note.**
The four module legs call `snapToTargets()` themselves, so they passed with the engine's four calls
DELETED — verified by deleting them. The added **engine leg** drives the real
`prepare` → `setParameters` → `process` path and fails without them. It uses the Mono Maker
deliberately: the one affected module with no delay line, so an empty history cannot be mistaken for
the glide under test (and it is an ADVANCED-mode control, so the leg must enable advanced mode or
`toEngine` never maps it at all). A **live-smoothing control** closes the other half of the
requirement: a parameter moved AFTER prepare must still glide, and a fix that simply disabled
smoothing fails it.

**The probe's verdict line was corrected in the same round.** Its ratio sees two things at once —
a smoother gliding from the wrong start (the defect) and empty delay-line/filter history filling up
(not a defect; `prepare` clears that by contract, and Haas's own 28 ms line outlasts the block the
first point covers). So the ratio rises when the fix lands without reaching 1.0 (0.17→0.72,
0.09→0.18, 0.29→0.68, 0.39→0.90, 0.35→0.58), and the probe now says so instead of reporting
"NOT settled" as a verdict. Test 49 is the discriminating instrument; the probe is the magnitude.

### ER-STATE-22 — FIXED — a malformed numeric boolean SWITCHED A SETTING ON, durably

An implementation correction to the already-approved ER-STATE-21 policy, not a new policy decision.

Round 18 implemented Policy B with `v != 0.0` for the three boolean settings. That is the C
coercion, not the field's domain: a boolean has exactly two valid serialized spellings, `0` and `1`,
which are the two this plug-in's own writer emits (`juce::var(bool)` reaches XML as "0"/"1"). Under
`v != 0.0` a corrupted `-1`, `-2`, `2` or `0.5` **enabled a setting the file never asked for**, and
the repair then persisted that as a genuine `true`. The asymmetry gives the rule away: `0` was the
only value on the whole real line that could not turn a setting on.

The repair now resolves anything outside `{0, 1}` to the field's documented default — `metersOn`
false, `tooltipsOn` false, `uiAnimations` true — which is also what an ABSENT field resolves to.
Booleans are the one `Kind` with no nearest-valid-value to clamp toward, so the default IS their
finite-out-of-domain rule; the ComboBox and unit-range clamps are untouched. The comparison uses
`juce::exactlyEqual` — the repository's idiom for an intentional exact compare — so the
`-Wfloat-equal` gate is not widened.

**Regression: State test 33, 30 → 39 cases.** Nine added (`-1`, `-2`, `0.5`, `2` across the three
boolean fields) and **one existing expectation corrected**: `int_metersOn` = `"2"` had been written
down as resolving to `true`, i.e. the defect recorded as an expectation rather than caught. The
boolean rows now carry six of the ten valid-value controls, including `int_uiAnimations` = `"0"`, so
a fix that merely forced every boolean to its default cannot pass. **12 checks fail against the
`v != 0.0` build**, and they are the whole delta — nothing outside the boolean rows moves. Against
the pre-policy (round-17) build the extended test fails **83** of its checks, re-measured at the new
size (it was 62 at thirty cases).

### ER-STATE-23 — NO PRODUCTION CHANGE — entirely covered by the existing deferred D-2 decision

The finding: the D-1 latency atomics "do not synchronize concurrent restore, prepare, A/B, preset,
or engine state". They do not, and were never meant to — `latencyUpdateRequest` carries the latency
REQUEST and nothing else — so the premise is a category error rather than a defect. The question
worth answering is what those states actually do, and it splits three ways.

1. **The restore / A/B / preset tail is exactly RISK-007.** `--state-thread-probe` under TSan
   reports the same four races the register already records: `abActive`, the `abUndo` vector twice,
   and a `juce::String` refcount exchange. Known, measured, deferred under D-2.
2. **The ENGINE's plain state does not race at all.** `setStateInformation` never writes it; the A/B
   and preset paths reach the engine only through atomics (`injectMatchGainDb`, `requestDuck`); the
   two writers that remain — `prepareToPlay` and `processBlock` — are mutually excluded by the host
   contract on VST3 and by JUCE's own AU callback lock.
3. **The one pairing D-2's recorded scope does not name** — restore on one host thread,
   `prepareToPlay` on another, editor tick reading — was measured for this round with a new probe,
   `AnamorphStateTests --state-prepare-race-probe` (three threads, under TSan). Result: **the same
   four reports and no new ones.**

Classified **known deferred D-2 risk**, recorded in `FUTURE_RISKS.md` RISK-007. **Nothing was added
to suppress the report** — no mutex, no `callAsync`, no `AsyncUpdater`, no state-architecture
redesign: that would pre-empt a decision that is the maintainer's, and would silence the evidence
D-2 is waiting on.

### RISK-008 — carried unchanged

No host test was performed this round, and none was permitted: the real-host half is the
maintainer's REAPER result from round 19, and the register entry already records it with its limits.

### Also corrected while in the files

- `CHANGELOG.md` carried the round-17 Scope-Persistence entry **twice**, verbatim — a double-apply
  from that round's batch. One copy removed; no wording changed.
- `docs/architecture/API_REFERENCE.md`'s `AnamorphEngine::prepare` row now states the snap contract
  the code has, rather than "allocates; resets".

## Round 19 — 2026-09-02 — RISK-008 gets its real-host half; the settled set audited for consistency

A disposition round. **No production code changed, and none was justified.**

### RISK-008 — REAL-HOST VALIDATED FOR REAPER; NO ACTIONABLE DEFECT DEMONSTRATED; HOST-SPECIFIC RISK REMAINS UNVERIFIED

Round 18 closed with the mechanism confirmed and its cost measured, and one half missing: no Linux
VST3 host was available to the review environment, so nothing could be said about whether any
shipping host produces the state the probe measures. **The maintainer supplied that half.**

**The real-host result, recorded as manual real-host evidence.** On **Linux, in REAPER, with the
real Anamorph VST3**, the reported latency **updates successfully with the Anamorph editor OPEN and
with it CLOSED.** The editor-closed case is exactly the observable this risk predicts would fail,
and it did not fail. This round performed no host testing of its own; the experiment was the
maintainer's and is recorded as theirs.

**The three kinds of evidence, kept apart deliberately**, because collapsing them is how a register
entry starts overclaiming:

| evidence | kind | what it establishes |
|---|---|---|
| the wrapper lifecycle (`messageThread->start()` in exactly one place, the `EventHandler` destructor at unload) | code reading, pinned tree | that an editor close in an `IPlugFrame`-only host CAN leave the queue unserviced |
| `--risk008-probe` | synthetic, labelled as such | what an unserviced queue COSTS: the request is deferred, not dropped, and lands 22 ms after servicing resumes |
| Linux + REAPER + real Anamorph VST3 | **manual real-host** | that the predicted failure does NOT occur in REAPER, editor open or closed |

**What the REAPER result does not establish, stated rather than glossed.** It does not show how
REAPER supplies `Linux::IRunLoop`. The repository contains no evidence on that point — checked this
round: every REAPER reference in the tree concerns unrelated matters (KI-009's preset-save focus,
VST3 parameter listing, rescan instructions, trademark attribution), and none touches the run-loop
path. So whether REAPER hands the loop over through the factory host context, through `IPlugFrame`,
or by another route is **not established here and is not guessed at**. A pass is consistent with
REAPER simply not exhibiting the suspected lifecycle, and consistent with explanations this
repository cannot distinguish between without evidence it does not have. It also says nothing about
any other Linux VST3 host.

**Disposition, and why it is not "disproved".** The predicted failure was looked for on a real host
and did not occur, so the entry is no longer "mechanism confirmed, prevalence unknown" and no longer
carries a pending host census. But one host is not every host, and the residual — a host using a
different `IPlugFrame`/`IRunLoop` lifecycle — is real and unverified. Recorded as **real-host
validated for REAPER, no actionable defect demonstrated, host-specific risk unverified**, with the
likelihood moved from Unknown to **Low** on that evidence. **No production change**: the residual
does not justify one, and the REAPER result is if anything evidence against needing one. D-1 is not
reopened, and its architecture is untouched — message-thread changes synchronous, the atomic request
off-thread, the processor-owned 20 Hz timer delivering on the message thread, no editor polling, no
`AsyncUpdater`, no `callAsync`, no mutex.

### Consistency audit of the settled set — four items, one correction

Every settled item was checked against the LIVE documents (registries, policies, procedures,
architecture, README, HANDOVER) rather than the worklog, which is historical by construction.

- **ER-STATE-21 — FIXED.** Policy B implemented in `InternalState::restoreState`, repaired values
  persisted, State test 33. The registry now records the decision and its per-field resolution table
  in place of the open question it carried through rounds 16 and 17.
- **ER-GUI-05 (drag recovery) — REFUTED.** No production change required; a wrapper would prevent the
  gesture opening rather than strand it, and State test 21 already covers the direct-child
  relationship. Not presented as open anywhere.
- **Cross-file realtime lint — verified, unchanged.** The closure is same-file and transitive, the
  cross-file seeds are an explicit manual registry, and the documents say so. Ninth consecutive
  verification.
- **D-1 — approved and implemented**, recorded consistently in KI-027's row and banner,
  `THREADING_POLICY.md`, `LATENCY_MODEL.md`, and the processor's header and implementation comments.
  The repository can verify its own record's internal consistency; it cannot establish the external
  authority behind the sign-off, and does not claim to.
- **RISK-007 / D-2 — deferred**, exactly as decided; no mutex, no `callAsync`, no state-architecture
  change. **KI-015** remains the one release blocker, and it is a legal/licensing action, not an
  engineering one.

## Round 18 — 2026-09-02 — the approved Settings recovery policy implemented; RISK-008 investigated and classified

### ER-STATE-21 — CLOSED — Policy B implemented: repair on restore, persist the repaired value

Rounds 16 and 17 gathered the evidence and refused to pick the rule; the maintainer picked it. The
decision, recorded verbatim in `SERIALIZATION_REGISTRY.md`: **a present-but-invalid modern Setting is
repaired during restore to a deterministic valid value, and the repaired value is retained in the
persisted state.** Implemented this round.

**The six settings, their domains and their resolutions.**

| field | domain | default | valid present | finite out of domain | not usable as a number |
|---|---|---|---|---|---|
| `int_oversample` | ComboBox id 1..4 | 1 | preserved | clamp to nearest id | `1` |
| `int_uiScale` | ComboBox id 1..5 | 3 | preserved | clamp to nearest id | `3` |
| `int_scopePersist` | double 0..1 | 0.5 | preserved | clamp into 0..1 | `0.5` |
| `int_metersOn` | bool | false | preserved | non-zero is `true` | `false` |
| `int_tooltipsOn` | bool | false | preserved | non-zero is `true` | `false` |
| `int_uiAnimations` | bool | true | preserved | non-zero is `true` | `true` |

**Before.** `restoreState` wrote `src.getProperty(id)` through unchanged. Measured over nineteen
malformed inputs: 19 of 19 survived into the next save, 8 left an out-of-domain ComboBox id in the
tree and 3 left a non-finite `int_scopePersist`. The damage was durable and re-interpreted on every
load.

**Root cause.** The registry stated a Default for each field's ABSENCE (settled in round 14) and no
rule at all for a value that is present but malformed, so the restore had nothing to apply and
adopted what the file said. That gap is what the decision closes.

**Implementation, at the narrowest layer.** Three changes, all in `src/InternalState.h`:
1. The `settings()` table now carries the DOMAIN beside the default (`Kind` plus a choice count).
   The two belong together for the same reason ER-STATE-18 merged the previous two lists: "what this
   field may hold" and "what it means when it is not there" are one contract, and two lists drift.
2. `usableNumber()` is the shared predicate — `SerializedNumber.h`'s `looksLikePlainNumber` plus
   `isUsableSerializedValue`, applied to the input text rather than the converted value. The legacy
   migration's own inline lambda, which was a verbatim copy of exactly this, now calls it. **One
   copy, and State test 28's 49 discriminating checks prove the legacy behaviour is unchanged.**
3. `repairedValue()` applies the table's rule and returns a correctly TYPED var, so the tree holds an
   int / double / bool rather than the file's text and the next save writes the canonical spelling.
   A ComboBox id is clamped in double BEFORE the integer conversion, so that conversion is defined
   for every input reaching it — the discipline ER-STATE-17 established for the same `[conv.fpint]`
   reason. `restoreState` writes `repairedValue(...)` for a present field and the documented default
   for an absent one, which is the only line that changed in it.

**Proof that the repaired value is what persists**, which is the half that makes this Policy B rather
than "clamp at the read": State test 33 restores each mutated session, then SAVES, then asserts the
malformed text is gone from the saved `ANAMORPH_INTERNAL` node, then RELOADS that save into a fresh
instance and asserts the same value comes back. The probe's own table shows the same thing end to
end — `99` → `4`, `-5` → `1`, `2.7` → `2`, `nan` → `0.5`, `maybe` → `0`, each with the resaved text
matching the repaired value rather than the input.

**Regression coverage: State test 33**, thirty cases over all six settings and every malformed class,
each asserted three ways (live value, saved text, reload). **Eight of the thirty are valid-value
controls** — including `int_uiAnimations = 0`, so a setting deliberately turned OFF must survive —
which is what stops a fix that merely resets everything to defaults from passing. A final leg pins
ABSENT as the separate rule it is. **62 checks fail against the pre-policy build, 0 after**, and the
controls pass in both, which is what makes the 62 meaningful.

**Nothing else moved.** No schema change, no property renamed, no `restoreState` redesign, legacy v0.2
semantics unchanged (State test 28), partial-settings reset unchanged (State test 29), the
byte-identical save/load/save round-trip unchanged (State test 3), and round 17's finiteness guard in
`Vectorscope::setPersistence` kept as the defensive backstop the decision asks for.

**Drift found and corrected while there:** the registry paragraph this round rewrote existed TWICE,
duplicated by a round-17 edit batch that partially applied before failing an assertion and was then
re-run. Collapsed to one.

### RISK-008 — CLASS B: technical risk confirmed, no actionable user-visible defect demonstrated

**The lifecycle, read from the pinned wrapper.** JUCE services a Linux VST3 plug-in's messages from
its own `detail::MessageThread` until the host registers an `IRunLoop`. The pinned SDK lets a
conformant host hand that over either through the factory host context or only through `IPlugFrame`,
which JUCE registers at editor attach (`attached()` → `viewRunLoop.emplace`) and unregisters at
editor removal (`removed()` → `viewRunLoop.reset()`). The decisive detail is the asymmetry:
`messageThread->stop()` runs from `updateCurrentMessageThread()` the moment a host loop is
registered, and `messageThread->start()` appears in **exactly one place in the whole wrapper — the
`EventHandler` destructor**, which runs at unload. So after an editor close in such a host the fds
are attached to nothing and the internal thread is stopped, with nothing to restart it.

**Why that reaches D-1.** `juce::Timer` delivers only by posting a `CallTimersMessage` for the
message thread to run (pinned `juce_Timer.cpp`). No servicing means no timer callbacks, so the
processor-owned 20 Hz consumer cannot run and a stored request stays stored.

**Measured, and it corrects the entry's original wording.** `--risk008-probe` (synthetic, labelled as
such in its own output) makes an off-message-thread request and then withholds servicing: across a
1000 ms window — 20 timer periods — the reported latency does not move; 22 ms after servicing
resumes the request is delivered in full and the reported value is the one the settled state
predicts. **The request is DEFERRED, not dropped**: the atomic flag holds it, so the host is stale
for exactly the unserviced window rather than permanently. No sleep stands in for synchronisation —
the negative phase asserts a state that cannot become true later without servicing, and its deadline
only bounds the run.

**Scope of the exposure, stated precisely.** Only requests raised OFF the message thread stall: host
automation of Drive/Algorithm with oversampling engaged, and an off-message-thread re-prepare
(ER-STATE-19's path). Anything on the host UI thread is unaffected, because that thread REMAINS
tagged as the JUCE message thread after the editor closes — nothing resets `messageThreadId` — so
`requestLatencyUpdate()` still delivers synchronously there. That covers state restore and any
Settings-driven oversampling change, neither of which can stall.

**Why this is class B and not class A.** The decision rule requires a reproducible real host-visible
failure. **No Linux VST3 host is installed in this environment and none was obtainable**, so whether
any shipping host supplies `IRunLoop` only through `IPlugFrame` remains unknown. What is established
is the mechanism and its cost, not that the cost is ever paid. Recorded in `FUTURE_RISKS.md` with
that limitation stated in terms; **no production change**, and the entry does not reopen D-1 —
which stays approved, implemented and untouched. The next step is unchanged and is a host census, not
a code change.

### ER-RT-05 — verified on both sides, no change

The walk is still same-file and transitive; `AUDIO_FN` still carries both ER-RT-02 cross-file seeds
(`setParameters`, `toEngine`); 47 files scanned, 0 violations, self-test 93 cases. Documentation was
verified accurate in round 16 and corrected there where it understated the reach. Eighth consecutive
verification, nothing to change.

### D-1 approval record — verified, no change

Approved and implemented, recorded as such in KI-027's row and banner, `THREADING_POLICY.md`,
`LATENCY_MODEL.md`, and the processor's own header and implementation comments. No document
describes it as pending or gated. The repository cannot establish the real-world authority behind its
own gate sign-off and this round does not claim to.

## Round 17 — 2026-09-02 — two investigations resolved: one defect at the end of the unclamped read, one finding refuted

### ER-STATE-21 — SPLIT DISPOSITION: a concrete defect FIXED, the contract question DEFERRED

Round 16 surveyed nineteen malformed MODERN Settings and found no crash, no undefined conversion on
the restore path, and every DSP-facing read clamped at source, but also that the values are kept
verbatim and survive into the next save. It closed with the contract question open. This round
finished the job the survey started: it followed the values to their consumers.

**The six settings, end to end.** Stored representation is always XML text, so every value arrives
as a `juce::var` string and each consumer's own conversion is what decides the outcome.

| setting | domain | default | consumer conversion | clamped at the read? | invalid value re-serialized? |
|---|---|---|---|---|---|
| `int_oversample` | ComboBox id 1..4 | 1 (Off) | `jlimit (0, 3, (int) v - 1)` → `osAtomic` | **yes** | yes, verbatim |
| `int_uiScale` | ComboBox id 1..5 | 3 (M) | `jlimit (0, 4, (int) v - 1)` | **yes** | yes, verbatim |
| `int_scopePersist` | double 0..1 | 0.5 | `(float) (double) v`, then the editor's Slider → `pow(v, 0.737f)` → `Vectorscope::setPersistence` | **no** | yes, verbatim |
| `int_metersOn` | bool | false | `(bool) v` | total coercion | yes, verbatim |
| `int_tooltipsOn` | bool | false | `(bool) v` | total coercion | yes, verbatim |
| `int_uiAnimations` | bool | true | `(bool) v` + `animFloat` | total coercion | yes, verbatim |

Five of six are safe by construction: two clamp at the read, three take a `var`→`bool` coercion that
is total (every input maps to true or false). `scopePersist` is the exception, and it is the only one
whose value can travel.

**Following it produced a real defect.** The chain, as the editor actually builds it, is Value →
`Slider` (range 0..1) → `applyScopePersist`'s `pow(value, 0.737f)` → `Vectorscope::setPersistence`'s
`jlimit(0, 1, ...)` → `windowFrames()`'s `jmap` → **`(int)`**. That last conversion is UNDEFINED for
a non-finite float ([conv.fpint]) — the same class round 12 closed on the legacy path — and two
inputs reach it non-finite:

| stored value | slider value | after `pow` | after `jlimit` | `(int)` conversion |
|---|---|---|---|---|
| `0.25` | 0.2500 | 0.3600 | 0.3600 | defined |
| `5.0` | 1.0000 | 1.0000 | 1.0000 | defined (the Slider clamps a too-high value) |
| `inf` | 1.0000 | 1.0000 | 1.0000 | defined (likewise) |
| `abc` | 0.0000 | 0.0000 | 0.0000 | defined |
| **`nan`** | nan | nan | **nan** | **UNDEFINED** |
| **`-1.0`** | −1.0000 | **−nan** | **−nan** | **UNDEFINED** |

Two things make this worth the round. First, `juce::jlimit` returns its argument when NEITHER
comparison is true, which is exactly what a NaN does — so a clamp that reads as total is transparent
to the one value that most needs clamping. Second, and less obvious: **a perfectly finite,
in-the-file value becomes the non-finite one**. Any NEGATIVE persistence is raised to a fractional
power on the way in, and `pow(-1.0f, 0.737f)` is NaN. So the defect does not need an exotic file;
`-0.5` is enough, and round 16 measured that opening the editor does not repair a negative value.

**Reproduced end to end, not modelled.** State test 32 leg 2 restores four malformed sessions into a
real processor, constructs the REAL editor, finds the `Vectorscope` among its children and reads the
persistence back off the component. Before the fix: `nan` → nan, `-1.0` → −nan, `-0.5` → −nan. After:
0.6000 in each case, the member's own initialiser.

**Fix, at the point where the invariant is declared.** `Vectorscope::setPersistence` promises "0..1"
in its own comment and did not deliver it for a NaN; it now substitutes the default for any
non-finite input. That is one line, local, and correct for every caller present and future, and it is
the recovery the meters and the correlation display already apply to a non-finite sample (ADR-0009).
A public `getPersistence()` was added alongside it so the guard is testable through the real editor
rather than by inspection. **Deliberately NOT fixed at the restore**: sanitising in `restoreState`
would be defining what a malformed present value MEANS, which is the contract question below, and
the brief for this round forbids inventing that. The tree still keeps whatever the file said.

**What stays open, precisely.** `SERIALIZATION_REGISTRY.md`'s `ANAMORPH_INTERNAL` table states a
Default for each field's ABSENCE (settled in round 14) and no rule for a value that is present but
malformed. Choosing between "clamp at the read", "repair at restore as the legacy path does" and
"adopt verbatim, since the consumers are safe" changes what a damaged file means to every version
that reads it, and it is a maintainer decision, not a lint. The registry now records the measurement
and the open question rather than a rule this programme picked. **Disposition: the defect is FIXED;
the contract question is DEFERRED with its evidence complete.**

**Not a compatibility defect, on the evidence.** The durable invalid state does not break the
compatibility promise: a session written by any version still loads in any other, the malformed value
is confined to the field it was written in, and every consumer that could act on it now yields a
value inside the documented domain. The residual risk is narrow and worth stating rather than
implying: a FUTURE version that reads one of these fields more strictly, or that changes a ComboBox
domain, would inherit whatever a damaged file carries — which is an argument for settling the
contract question, not evidence of a defect today.

### ER-GUI-05 — REFUTED — the direct-child sweep cannot strand a gesture, because a wrapper would prevent the gesture existing

The finding: *"`abortAbandonedDragGestures` searches only direct children. Wrapping the value box
later will silently strand host gestures again"* (`src/PluginEditor.cpp:1370`). The premise is
backwards, and the code says so in three places.

**The gesture owner is the value box, and it is the only one.** `anamorph::gui::DragGestureOwner` has
exactly one implementer, `ValueBox` in `LookAndFeel.cpp`, created only by
`AnamorphLookAndFeel::createSliderTextBox`.

**Every value box is a direct child of its slider, by JUCE's construction, not ours.**
`juce::Slider` does `valueBox.reset (lf.createSliderTextBox (owner)); owner.addAndMakeVisible
(valueBox.get());` (pinned `juce_Slider.cpp:601-602`). The plug-in never parents it.

**Every slider is registered.** Thirteen `Knob` members exist; twelve go through `setupRotary` and
`scopePersistK` through the explicit list, and all thirteen reach `registerAnimated`. There is no
slider outside the sweep, and `ABControl` — the one custom composite in the list — has no child
components at all.

**And the wrapper scenario fails safe, which is the actual answer.** `ValueBox` depends on
`rotaryParent (getParentComponent())` in `mouseDown` (which OPENS the gesture), in `mouseDrag`, and in
`abortDragGesture`. Wrap the value box and `getParentComponent()` stops being the slider, so
`rotaryParent` returns null and **no gesture is ever opened** — there is nothing to strand. The
failure would be loud (drag-to-edit and its undo step stop working) rather than silent, and it is
already covered: State test 21 locates the box as a direct child of a slider and asserts "the press
registers on the knob" before testing the reconcile, so a wrapper fails that test twice over.

The traversal depth and the gesture's own precondition are therefore the same fact, and cannot
diverge. **No production change**, per the brief's instruction not to generalise the traversal for a
hypothetical wrapper. The existing comment at the sweep already states the relationship correctly
("`animated` holds the SLIDER and the gesture lives one level down"), so no documentation was
misstated and none is changed.

Worth recording for completeness: `scopePersistK` is `LinearHorizontal`, and `rotaryParent` accepts
only rotary styles, so its value box never opens a gesture at all. The sweep covers it regardless.

### ER-RT-05 — implementation and documentation both verified, no change

Verified on both sides this round rather than the documentation alone. Implementation: the walk is
still same-file and transitive (the lint says so at its own `:618`, and three self-test cases pin it,
including a two-hop transitive case); `AUDIO_FN` still carries both ER-RT-02 cross-file seeds,
`setParameters` and `toEngine`, so neither documented hole has regressed. Runtime: 47 files scanned,
0 violations, self-test 93 cases. Documentation was verified accurate in round 16, when the one
inaccuracy found — `REPOSITORY_MAP.md` UNDERSTATING the reach with pre-closure seed-only phrasing —
was corrected. Nothing further to change.

## Round 16 — 2026-09-02 — the previous project's A/B Level-Match gains survived a restore

### ER-STATE-20 — CONFIRMED and FIXED — and it leaked on a second path the finding did not name

The finding: *"after a restore without A/B data, `abResetToDefaults` retains the previous session's
level-match gains. The first A/B switch can apply that stale gain"* (`src/PluginProcessor.cpp:799`).
Confirmed exactly as filed, and one path wider.

**The state, and why nothing overwrote it.** `abMatchGain[2]` is a processor member holding, per A/B
slot, the gain the loudness matcher had settled on when that slot was last left. `abSwitchTo` stores
into the slot it leaves and restores into the one it enters:

```
abSlot[abActive]    = currentStateSet();
abMatchGain[abActive] = engine.getMatchGainDb();   // remember the slot being left
abActive = slot; abApplySlot (slot);
engine.injectMatchGainDb (abMatchGain[slot]);      // restore the slot being entered
```

It is the ONE part of a slot that is never serialized, which is precisely why it leaked: every other
piece of slot state is written by the restore, so it is necessarily overwritten, while this one had
no writer on the restore path at all. `abResetToDefaults()` reset `abSlot[]` and `abActive` and
stopped there. A host restores into one live instance repeatedly, so the values simply stayed.

**Reproduction, on the real paths.** State test 31 seeds a "previous project" whose two slots have
genuinely MEASURED and distinguishable matches (Level Match on, width 0.95 then 0.05, 60 blocks of
deterministic noise each): slot A −2.438 dB, slot B −1.040 dB. It then restores a session with no
A/B data into that same instance and performs the first switch. Before the fix:

| restore path | stale value | first switch injected | a fresh instance injects |
|---|---|---|---|
| v0.2 bare APVTS (`abResetToDefaults`) | −1.040 dB (slot B) | **−1.040 dB** | 0.000 dB |
| modern root, no `AB` node (`abResetToDefaults`) | −1.040 dB (slot B) | **−1.040 dB** | 0.000 dB |
| `AB` node present, no payloads, `active` = 1 (`readSlot`) | −2.438 dB (slot A) | **−2.438 dB** | 0.000 dB |

Not approximately the stale value — the stale value, to the digit.

**The second path, found by tracing rather than by the finding.** An `AB` node that EXISTS but
carries no usable slot payload never reaches `abResetToDefaults`: `readSlot` resets each slot in
place instead. That is still "a restore without A/B data" in substance — `readSlot`'s own rule is
that an absent payload means the default — and it is the only path that exposes slot A, because
`abActive` comes from the blob (so it can be 1) and the first switch overwrites the slot it leaves
before reading the one it enters. Measured: with the reset confined to `abResetToDefaults`, that leg
still injected −2.405 dB. Fixing only the named function would have left the wider half of the
defect in place and a green test beside it.

**Which entry is observable, stated precisely rather than overclaimed.** After a restore that sets
`abActive` = 0, `abMatchGain[0]` is overwritten by the first switch before anything reads it, so
only `abMatchGain[1]` is reachable there; the `active` = 1 path is what makes `abMatchGain[0]`
reachable. Both are reset anyway — the cache is slot state, and which index is "the unread one" is a
function of `abActive`, which the same code resets.

**Fix.** Reset the cache wherever a slot is reset: in `abResetToDefaults()` for the two paths with no
`AB` node, and in `readSlot` per slot for the path that has one. Both are unconditional, beside the
`slot = {}` / `dst = {}` they belong to. 0.0f is the member's own initialiser, not a chosen
sentinel, and 0 dB clears the engine's `> kNoInject` guard, so it is APPLIED as unity rather than
skipped — which is exactly what a never-switched fresh instance injects. No serialization change,
no format change, nothing masked downstream: the state itself is correct.

**Not a re-opening of ER-STATE-13.** Round 9 measured the AUDIBLE consequence of an injected stale
value and refuted it — `setParameters` re-targets `matchGainSmooth` from the live measurement every
block, so the level recovers — and that conclusion is unchanged and was re-run this round
(`--legacy-match-probe`, same verdict). What round 9 examined was the impact; what this round fixes
is the state, which was wrong independently: the Level Match readout showed the previous project's
number and the new project's matcher re-converged from it.

**Regression coverage: State test 31**, four legs, deterministic. Three restore paths (the table
above), each asserted two ways — the injected value is not the stale one, AND the reused instance is
indistinguishable from a FRESH instance restored from the identical blob, which is the
state-isolation contract stated directly. Plus a fourth leg proving a session that DOES carry valid
A/B data still restores both slots' own sounds and its own active slot. **6 checks fail without the
fix, 0 with it.**

Two details of the harness are worth recording, because both were wrong in a first draft and the
measurements are the reason:
- **Asserting "injects exactly 0 dB" is not right, and the fresh-instance comparison is.**
  `LoudnessMatch`'s feed-forward predict is an absolute function of Drive and Mix and lowers the
  published gain when the restored session implies more boost, so the reading sits off 0 by however
  much the restore moved those controls (−3.161 dB against the v0.2 fixture, −0.052 dB against a
  modern save). The fresh control experiences the identical predict, so comparing against it cancels
  that term and leaves only the injection.
- **The test performs the host's ordinary post-restore activation** (setState, then `prepareToPlay`
  — the sequence that function's own comment calls the ordinary VST3/AU order). Without it the
  reused instance still holds the previous project's audio in its delay lines and oversamplers,
  which flushes through the first blocks and moves the reading 0.052 dB. That residue is engine
  history, not A/B state, and is not what this test is about.

The observation itself is exact rather than a tolerance game, and for two product reasons: after a
restore with no A/B data both slots are re-seeded from the SAME restored state, so the switch is
parameter-neutral; and `LoudnessMatch` HOLDS its published value on silence by documented design
("when the input decays to silence the measurement WAITS ... it never drifts toward 0"). A switch
performed over silent blocks therefore leaves `getMatchGainDb()` reading the injected value verbatim.

### ER-STATE-21 — INVESTIGATION ONLY — modern Settings validation: no defect actionable, evidence recorded

The finding: *"`restoreState` accepts present modern settings verbatim, unlike legacy migration.
Define recovery semantics before malformed persistence or coerced booleans become durable state"*
(`src/InternalState.h:128`). Investigated with a new measure-only probe
(`--modern-settings-probe`), nineteen malformed values written one at a time into a genuine modern
save's `ANAMORPH_INTERNAL` node. **No production code changed.**

**Ingress first, because it bounds everything else.** The modern values are written by exactly four
things: the constructor's `settings()` defaults table, `restoreState` (from a file),
`migrateFromLegacyApvts` (clamped at source since ER-STATE-17), and the Settings widgets, whose
ComboBox ids and Slider range are valid by construction. A malformed MODERN value can therefore only
arrive from a **hand-edited or corrupted file**; the plug-in cannot produce one.

**What the nineteen cases actually do:**

| question | answer |
|---|---|
| crash | **no**, 0 of 19 |
| undefined behaviour | **no** — `juce::var`→`int` on a string is a safe parse, not the float cast that made the legacy path undefined in round 12 (ER-STATE-17). Round 14's reading is confirmed by measurement |
| invalid *DSP-facing* state | **no** — `oversampleIndex()` clamps through `jlimit(0,3)` and `uiScaleIndex()` through `jlimit(0,4)` at the read, so neither can index out of range whatever the tree holds. Measured in range in all 19 |
| invalid *stored* state | **yes** — 8 of 19 leave an out-of-domain ComboBox id in the tree, and 3 leave a **non-finite** scope persistence (`nan`, `inf`, `1e39`). `scopePersist()` is the one consumer with no clamp at its read |
| durable across a save | **yes** — 19 of 19 persist verbatim into the next save |
| repaired by opening the editor | **partly, and inconsistently** — 4 of 19. The Slider's range constrains a too-high or overflowing persistence (5.0 → 1.0, `inf` → 1.0, `1e39` → 1.0) and the ComboBox coerces a fractional id (2.7 → 2). A NEGATIVE persistence, `nan`, and every out-of-domain integer id survive with the editor open |

**Disposition: no production change, on the brief's own terms and on the evidence.** There is no
crash, no undefined behaviour, and no audio-path exposure — the three things that would make this
actionable now. What remains is a stored value that is invalid and durable, reachable only from an
edited file, and the question of what a malformed *present* value SHOULD mean is exactly the one the
finding says to define: `SERIALIZATION_REGISTRY.md`'s `ANAMORPH_INTERNAL` table states a Default for
each field's ABSENCE and says nothing about malformation, and the ABSENT rule was itself only settled
in round 14 (ER-STATE-18). Choosing between "clamp at the read", "repair at restore like the legacy
path" and "leave it, since the consumers are already safe" is a serialization-contract decision with
a compatibility consequence, and inventing one from a probe is what the brief forbids and what this
programme files rather than does. Recorded here and in `TESTING.md`; the one asymmetry a decision
would most naturally start from is that `scopePersist()` is the sole unclamped consumer.

### ER-RT-05 — the lint boundary is described accurately; one document UNDERSTATED it, corrected

Seventh consecutive verification, and the first to find anything. No document anywhere claims
automatic or cross-file discovery of audio-path callees — the boundary is stated verbatim in the four
places that describe it (`REALTIME_AUDIO_POLICY.md` "That closure is same-file",
`REALTIME_SAFETY_AUDIT.md` "the same-file transitive closure … not a whole-program one",
`CI_CD.md` "every callee defined in the same file, transitively", and `build.yml`, which explicitly
denies the cross-file reach and points at `-Wfunction-effects` for it). The lint's own header says the
same. So the finding's concern — that the boundary is untracked — does not hold.

What DID need correcting is the opposite error. `REPOSITORY_MAP.md` still described the lint with the
pre-2026-08-18 seed-only phrasing: "scans the bodies of the functions `REALTIME_AUDIO_POLICY` binds",
omitting both the transitive same-file closure and the two ER-RT-02 seeds (`setParameters`/`toEngine`).
It **understated** the reach rather than overclaiming it, which is why six rounds of checking for
overclaims never caught it. Corrected in place, with the boundary named. No lint change, no `AUDIO_FN`
expansion, no redesign — exactly as the brief requires.

One precision note about this round's own predecessors, recorded rather than acted on: rounds 8–15
each reported that "ADR-0029 describes the same-file closure correctly". ADR-0029 is in fact **silent**
on the closure — its scope paragraph is the pre-closure state ("7 scanned bodies to 35"; `CI_CD.md`
records the closure taking 35 → 61). Silent is not wrong, and an ADR is a dated decision record that
should not be retrofitted, so nothing is changed there; but the claim in those worklog entries was
generous and this entry says so rather than repeating it an eighth time.

### D-1 approval record — the normative record is correct; two stale spots in the RENDERED dashboard

The `docs/`, `src/` and `tests/` record is accurate everywhere D-1 is described: KI-027's registry row
and its detail banner, `THREADING_POLICY.md`, `LATENCY_MODEL.md`, both communication tables, the
processor's own comments, and the round-4/12/15 worklog entries all say **approved by the maintainer
and implemented**. Every surviving "awaiting sign-off" string sits inside struck-through row text or
under a banner stating in terms that the language is historical. No re-approval is asked for and the
threading architecture is untouched.

Two genuinely stale spots were found, both in the rendered companion `ENGINEERING_REVIEW_REPORT.html`
and both outside the scope earlier audits used (they checked four `docs/` locations and grepped for
"awaiting"; these say "Gated" and live in frozen round-2 blocks):
- **§7 Roadmap** carried `D-1 implementation (if approved) … Gated` with no round attribution, so a
  reader met it as open work. The table is a round-2 snapshot; it now says so, and points at §5 and §8
  for live status.
- **§8** carried four unticked round-2 carry-overs — `D-1 decided`, `D-2 decided`, `D-3 Level-5
  audition`, `Round 3 executed` — which, because rounds 7–16 were inserted above them, had drifted to
  render under Round 6. All four are long since closed (D-1 approved r4, D-2 deferred r4, D-3 PASSED),
  and they are now ticked with their outcomes named. One decision card's "What you are approving"
  became "What was approved", matching the `APPROVED & IMPLEMENTED r4` chip beside it.

## Round 15 — 2026-09-02 — the off-message-thread re-prepare race: confirmed under ThreadSanitizer, closed inside D-1

### ER-STATE-19 — CONFIRMED and FIXED — a host that prepares off the message thread raced the plug-in's own latency timer

The finding: *"when a host re-prepares off the message thread, `timerCallback` can concurrently
read changing engine latency and report it; the host can retain stale delay compensation, and the
unsynchronized reads invoke undefined behavior"* (`src/PluginProcessor.cpp:203`, pre-fix numbering;
the line numbers in this section are the pre-fix ones the review cited unless marked otherwise).

**The shared state, traced.** Two things, and the review named both correctly.
1. `AnamorphEngine::latency2/4/8` — plain `int`s written by `engine.prepare()`
   (`AnamorphEngine.cpp:54-56`) on whatever thread the host activates on, and read by
   `predictLatency()` (`:463-473`) from `timerCallback → deliverLatency` on the message thread.
2. `juce::AudioProcessor::latencySamples` — a plain `int` compared and assigned inside
   `setLatencySamples` (pinned JUCE `juce_AudioProcessor.cpp:415-421`, no lock), reached from BOTH
   threads: `prepareToPlay → updateLatency → deliverLatency` on the host's thread and
   `timerCallback → deliverLatency` on the message thread. The listener walk after it
   (`updateHostDisplay`, `:430-436`) takes the listener lock only to fetch each pointer and runs
   the callback unlocked; the VST3 wrapper's `lastLatencySamples` sits behind it.

The request FLAG (`latencyUpdateRequest`) is not the problem and was never in question: it is
atomic, and its release/acquire pair is what D-1 and ER-STATE-14 pinned. The problem is the
VALUE that flag publishes, and who else writes the host-facing number.

**A real C++ data race, not a theoretical one — conditional on the thread.** The only
happens-before edge on this path is that flag: `requestLatencyUpdate`'s release store paired with
the `acquire` `exchange` in `timerCallback` / `updateLatency`. That edge orders the writes made
BEFORE the store. A request raised before the host activates — `setStateInformation`'s
unconditional trailing request (`:1110`), or an audio-thread automation write — is served by a
tick that can land anywhere inside the host's `prepareToPlay`; `engine.prepare()`'s writes come
AFTER that store, and `prepareToPlay`'s own `exchange(0, acquire)` (`:152`) is an acquire, not a
release, and follows them anyway. Nothing else on the path locks: `engine.prepare` has no
atomics, `setLatencySamples` has no lock, the APVTS listener locks are not on it. Two unordered
accesses to a non-atomic object: a data race per [intro.races], undefined behaviour. Stated
precisely: the tick has to land between that release store and the host's own `exchange`; if the
host clears first, that tick does nothing and that run has no race.

**The stale ending, exactly.** The tick reads `latency2` before the host's write — 0 on an
instance never prepared, since the oversamplers do not exist yet; the host writes
`latencySamples = N` and notifies; the tick's `setLatencySamples(0)` then compares against N,
stores 0 and notifies; the flag is already 0 on both sides, so nothing corrects it until the next
unrelated parameter move or re-prepare. Reachable — and narrower than the review's wording: the
oversamplers' latencies do not depend on the sample rate, so every prepare after the first
rewrites identical values and `setLatencySamples`' guard suppresses the notification. The
observable stale PDC needs an instance's FIRST prepare and the tick preempted inside a few
instructions; the undefined behaviour is on every off-thread prepare that overlaps a tick.

**Which hosts prepare off the message thread — read from the pinned wrappers, not assumed.**
- **VST3, in spec, desktop.** `setActive` is `[UI-thread & Setup Done]` (`ivstcomponent.h:190-196`)
  and is the only caller of `prepareToPlay` (`juce_audio_plugin_client_VST3.cpp:2684-2716` via
  `preparePlugin`, `:3772`; `setupProcessing` and `initialize` pass `CallPrepareToPlay::no`).
  JUCE asserts the message thread for `setState` (`:2917-2921`) and NOT for `setActive`; the
  `FLStudioDIYSpecificationEnforcementLock` (`:3618-3655`) exists because FL Studio's Patcher
  does not honour that threading. On the host's UI thread — the thread JUCE tagged as its message
  thread at instantiation — there is no race. That is the common case, and it is unchanged.
- **VST3 on Linux, in spec.** JUCE services the plug-in's messages, this timer included, from its
  own `detail::MessageThread` (`juce_LinuxMessageThread.h`) until the host registers an
  `IRunLoop` (`:120-330`: *"Until then JUCE messages are serviced by a background thread internal
  to the plugin"*). The pinned SDK lets a conformant host hand the run loop over through the
  factory context — registered at `createInstance`, so the host thread is tagged from construction
  — OR only through `IPlugFrame`, which JUCE registers at editor attach (`:1992`). In the second
  kind of host the whole pre-editor phase — restore, then `setActive` — runs off the JUCE message
  thread while that background thread ticks the timer; a host that never provides a run loop
  stays there for the plug-in's life.
- **AU under pluginval — the repository's own macOS release gate.** pluginval runs every test on a
  `std::thread` (`Validator.cpp:244`) and hops to the message thread for `prepareToPlay` ONLY
  for VST3 (`TestUtilities.h:198-206`, `callPrepareToPlayOnMessageThreadIfVST3`); for AU that
  reaches `AudioUnitInitialize` on the test thread (`juce_AudioUnitPluginFormatImpl.h:1162`, no
  lock) → the wrapper's `Initialize → prepareToPlay` (`AU_1.mm:225-236`), while the plug-in's
  timer dispatches on the main run loop (`juce_MessageQueue_mac.h:48`). Its parameter and state
  tests raise the flag from that same thread first. So the CI gate that has been green exercises
  exactly this interleaving — with nothing to see, because a data race is not a crash and the gate
  does not run a sanitizer. Verified against pluginval's current sources this round, not assumed.
- **AU hosts** initialising, resetting (`AU_1.mm:258`) or toggling offline render (`:801-810`)
  off main: no AU spec pins the thread — RISK-007's class.
- **Standalone.** `AudioDeviceManager` starts the device on the message thread and the
  CoreAudio/WASAPI restarts go through `AsyncUpdater`; `AudioProcessorPlayer` prepares under its
  lock on that thread. No race.

**Reproduced.** `AnamorphStateTests --reprepare-race-probe` (new, opt-in, the
`--state-thread-probe` pattern): a non-message thread moves Drive — raising the flag exactly as
State test 22 does — and re-prepares, 200 times × 20 instances, while the main thread serves the
20 Hz timer. Pre-fix under ThreadSanitizer (`build-tsan`, clang-22): **two reports, both
predicted** — `AnamorphEngine::prepare` writing `latency2` (`:54`) against `predictLatency`
(`:469`) reading it from `timerCallback`, and `AudioProcessor::setLatencySamples` (`:417` read on
the host thread against `:419` write on the message thread). Pre-fix plain build: **3,980 of
4,000 deliveries ran on the host thread.** The stale ending was **not observed** in 4,000
iterations (0 of 20 instances left the host wrong): it needs the tick's compare-and-store to
straddle the host's, and only the first prepare can change the number — the probe reports counts
and asserts nothing, so that limit is stated rather than papered over. Post-fix: 0 deliveries on
the host thread, ThreadSanitizer silent over the probe and over the whole suite.

**Root cause.** D-1 made `requestLatencyUpdate()` the one entry for every re-report and left
`prepareToPlay` calling `updateLatency()` directly, on the silent assumption that activation is a
message-thread call. `deliverLatency`'s own comment claimed *"message thread only, by
construction"*; the construction had one gap, and the documents (`LATENCY_MODEL.md`,
`THREADING_POLICY.md`) carried the assumption as a fact.

**Fix — inside D-1, nothing new.** (1) `prepareToPlay` calls `requestLatencyUpdate()`: on the
message thread that is the identical synchronous clear-then-deliver (byte-for-byte the previous
sequence, so Wavelab's `setBusArrangements`-inside-`setLatencySamples` shape, JUCE
`:2691-2696`, is untouched); anywhere else it is the existing release store, and the timer reports
within one tick — so the message thread is the ONLY writer of `latencySamples`, and the flag's
release/acquire pair now also orders a prepare's latencies before the tick that reports them.
`requestLatencyUpdate()` additionally delivers synchronously when no `MessageManager` exists at
all, because there is then no timer to serve a request — the constructor already guarded the
timer on that condition; no in-tree harness needs it (`fuzz_state.cpp` creates one), it is there so
the two guards agree. (2) `latency2/4/8` become relaxed `std::atomic<int>` at all ten use sites —
the payload the flag publishes, with no ordering role of their own; a relaxed load is a plain load
on x86-64 and AArch64, so `getLatencySamples()` on the audio thread pays nothing and
`check-realtime.py` sees nothing new. (1) alone is measurably NOT enough — it closes the
`latencySamples` race and leaves the `predictLatency`-vs-`prepare` one, which a tick serving an
EARLIER request still reaches; (2) is what makes that overlap a race-free stale-or-new read that
the prepare's own request then supersedes. No mutex, no `callAsync`, no `AsyncUpdater`, no editor
polling; the processor-owned 20 Hz timer is unchanged; the reported VALUE is unchanged. Cost,
stated: a host that activates off-thread now learns the activation latency up to 50 ms late,
through the same `restartComponent(kLatencyChanged)` path automation already uses (JUCE
`:1553-1557`; a re-prepare converges with no second restart, since equal values early-out).

**Gate note — filed, not self-cleared.** `THREADING_POLICY.md` names "a new shared-state path or a
new atomic ordering" as an Architecture-Review-Gate trigger. This change adds neither: the flag,
its release/acquire pair and the message-thread consumer are D-1's, approved in round 4;
`prepareToPlay` becomes one more producer on that flag, exactly as the brief specified; and
`latency2/4/8` were ALREADY read across threads (that is the defect) — the atomics legalise an
existing path under D-1's ordering rather than open one. The round's adversarial synthesis
nonetheless asked that the maintainer confirm that reading rather than have it inferred, and this
worklog records the request. The four hard-stop categories are untouched: no parameter change, no
serialization change, no signal-order change, no reported-latency VALUE change.

**Regression coverage: State test 30**, deterministic. A message-thread twin establishes the truth
(4 samples at 2×, Drive up) and its control — the notification happens INSIDE `prepareToPlay`, on
the message thread, exactly once, so a build that deferred the message-thread path too would fail
here rather than pass the off-thread legs vacuously. A worker thread then prepares an instance
reporting 0; a listener records the thread of every `latencyChanged` notification. Asserted: no
notification from the preparing thread (pre-fix the worker itself delivered — the discriminating
check); the report unchanged after the join (nothing has dispatched on the message thread yet —
equally deterministic on both sides); then the tick serves the PREPARED value, non-zero and equal
to the twin's — the actual latency, not the flag. **Fails 3 checks without the fix, 0 with it**,
measured both ways. No test-side synchronisation was needed beyond the join and the existing
bounded timer polls. ER-STATE-14's barrier leg (State test 27) is untouched and passes;
`deliverLatency` still never touches the flag; `timerCallback` still clears exactly once; an
off-thread prepare now performs no clear at all, only a store.

**Adversarially verified**, three lenses and a synthesis, none refuting: the memory-model lens
sharpened the two preconditions recorded above; the host-contract lens ADDED the two
configurations that matter most — the CI-gated AU-under-pluginval run and the `IPlugFrame`-only
Linux host — and found the pre-existing item below; the fix-regression lens measured that (1)
alone is insufficient and that the message-thread path is byte-identical.

### Reported drift, corrected while there
- `THREAD_MODEL.md`'s and `THREADING_POLICY.md`'s communication tables never listed the D-1
  request path at all, four rounds after it was approved and implemented; both now carry the row
  (flag, orderings, producers, consumer).
- `HANDOVER.md`'s Test-Status row still said "28-test … 1237 checks" — round 14's count sweep
  claimed to have updated it and had not. Now 30 / 1278, with the round-14 and round-15 steps
  spelled out.
- `RISK-007`'s *"pluginval structurally cannot produce the window"* holds for VST3 `setState`
  (JUCE hosting's `MessageManagerLock`) and not for AU. Worse for that entry: pluginval's
  `BackgroundThreadStateTest` holds the editor open on the message thread and calls
  `getStateInformation` / `setStateInformation` from a background thread — on AU, with no hop,
  that IS RISK-007's window, exercised on every green macOS run. Narrowed in place; the entry's
  Likelihood is now explicitly about shipping hosts, not about whether the window is produced.
- `LATENCY_MODEL.md` and `THREADING_POLICY.md` had `prepareToPlay` on the message-thread side of
  D-1 as a fact rather than an assumption; both now say which hosts break it and what happens then.

### Newly identified, NOT acted on — RISK-008
The host-contract lens found, and I verified in the wrapper, that a Linux VST3 host which provides
its `IRunLoop` only through `IPlugFrame` leaves the plug-in's JUCE message queue unserviced once
the editor closes: `attached()` registers the view's run loop (`:1992`), `removed()` unregisters
it (`:2040`), and nothing restarts the internal `MessageThread` until the shared `EventHandler` is
destroyed at unload (`:165-171`). Every JUCE-message consumer in the plug-in pauses until the
editor reopens — the D-1 timer (an audio-thread latency request is then served not within 50 ms
but when the editor next opens) and the APVTS's own value-flush timer. `prepareToPlay` is
unaffected: the host UI thread stays the tagged message thread, so its delivery is synchronous.
Whether any shipping host hands the run loop over only that way is not something this tree can
establish; recorded as RISK-008 with the likelihood stated as unknown, not fixed — a fix is a
threading-model change.

### ER-RT-05 — verified accurate for the sixth consecutive round, no change
`AUDIO_FN` is a manual registry and `check-realtime.py` says so; `REALTIME_AUDIO_POLICY`,
`REALTIME_SAFETY_AUDIT`, `CI_CD` and ADR-0029 describe the same-file closure correctly, and none
claims automatic cross-file discovery. Checked while here: `std::atomic` loads are not in the
lint's forbidden classes, so `getLatencySamples()`'s new relaxed loads trip nothing, and
`prepare()` is out of scope as before — the lint runs clean on the fixed tree.

### D-1 approval record — correct, no change
Four places, unchanged since round 12's correction: KI-027's row and banner, `THREADING_POLICY.md`'s
rule, `LATENCY_MODEL.md`. This round EXTENDS D-1's mechanism to one more caller and records that
in each of the four; it does not alter the decision, its approval, or its attribution (role-level,
as before).

---

## Round 14 — 2026-09-01 — partial modern Settings inherited the previous project: confirmed, and on the opposite path from the one reported

### ER-STATE-18 — CONFIRMED and FIXED — but not where the review looked

The finding was *"when a modern session omits an optional setting, `migrateFromLegacyApvts` never
resets it"*, filed against `src/PluginProcessor.cpp:1038` — the **v0.2 branch**. The brief asked
whether this was real, a contradiction with the documented missing-node behaviour, or a confusion
between the legacy and modern paths. Measurement answers all three at once, and the answer is the
interesting kind: **the symptom is real, and the named mechanism is exactly backwards.**

`--partial-settings-probe`, on a reused instance, one field omitted at a time from an otherwise
real modern save:

| omitted field | session A | documented default | after restoring B | |
|---|---|---|---|---|
| `int_oversample` | 3 | 1 | **3** | inherited |
| `int_uiScale` | 5 | 3 | **5** | inherited |
| `int_scopePersist` | 0.9 | 0.5 | **0.9** | inherited |
| `int_metersOn` | true | false | **true** | inherited |
| `int_tooltipsOn` | true | false | **true** | inherited |
| `int_uiAnimations` | false | true | **false** | inherited |

**Modern path: 6 of 6 inherited. Legacy path: 0 of 6.** `migrateFromLegacyApvts` — the function the
finding names — has always written all six unconditionally; it is the one path that was already
correct. The defect is in `InternalState::restoreState`, which wrote only the fields `src` carried.

**Not a contradiction with "missing nodes use normalized defaults".** That informational item is
about `applyNorm` inside `reassertParameters`, which covers APVTS **parameters**. The host-hidden
Settings are a different subsystem with their own tree, and the two never shared this code.

**Root cause and contract.** `SERIALIZATION_REGISTRY.md`'s `ANAMORPH_INTERNAL` table marks every
field **"Required: No"** with a stated **Default**. That means the default is *applied* when the
field is absent — but `restoreState` read "not required" as "skip". `tree` is a processor member and
a host restores into one live instance repeatedly, so skipping an absent field means keeping the
previous project's value, which the next save then writes out as if the session had always held it.
This is the same state-isolation class rounds 2, 8 and 11 closed for the Settings on the v0.2 path
(ER-STATE-08), the A/B slots (ER-STATE-12) and a root with no sound child (ER-STATE-15) — the fourth
member, and the last of the four processor members that restore into a reused instance.

**Fix.** `restoreState` now writes all six unconditionally, taking the field from `src` when present
and the documented default when not. The six defaults, previously hand-written twice (constructor
and — by omission — nowhere else), now live in one `settings()` table that the constructor seeds
from and `restoreState` falls back to, so they cannot drift into two different answers for "absent".
No serialization format change; a session that carries a field restores it exactly as before.

**Regression coverage: State test 29**, four legs — each field omitted in turn from a real modern
save (with the re-save asserted to carry the reset value, not the inherited one); a session that
**explicitly carries** every field, which must still restore those values, so the fix cannot pass by
resetting unconditionally; the **legacy** path, pinned so the two can never be confused again; and
**malformed-state repair**, asserted unchanged — a malformed modern Setting is not the absent case
and is still adopted and clamped by its consumer. **Verified discriminating: 12 checks fail without
the fix, 0 with it.**

Checked while there, and *not* changed: a malformed value on the MODERN path carries no undefined
conversion. `syncAtomics` clamps through `jlimit`, and `juce::var`→`int` on a string is a safe parse,
not the float cast that made the legacy path undefined in round 12. Recorded rather than acted on.

### ER-RT-05 — verified accurate for the fifth consecutive round, no change

`AUDIO_FN` is a manual registry and `check-realtime.py` says so; `REALTIME_AUDIO_POLICY`,
`REALTIME_SAFETY_AUDIT`, `CI_CD` and ADR-0029 all describe the same-file closure correctly, and no
document claims automatic cross-file discovery. Nothing to correct.

### D-1 approval record — correct, no change

The record says what it should, in four places: KI-027's registry row (**RESOLVED … decision D-1 —
APPROVED by the maintainer and implemented**), the dated banner over its detail section,
`THREADING_POLICY.md`'s rule, and `LATENCY_MODEL.md`. Round 12 corrected the two documents that had
still said "awaiting sign-off"; nothing has regressed since. The only remaining "awaiting" strings
sit inside the struck-through row text and inside the round-1/2 diagnosis under a banner that says
in terms that the language is historical.

One limit worth stating rather than glossing: the record attributes the approval to **"the
maintainer"** — the role, not a named individual — which is the same convention every other gate
sign-off in this repository uses (ADR-0024's serialization sign-off, the Level-5 audition, the
Architecture Review clearances). Whether the person who gave it holds that authority is not
something this repository can establish from the inside, and this round does not claim to have
verified it.

---

## Round 13 — 2026-09-01 — ER-STATE-17 verified on the real pre-0.8.4 fixture; the compatibility gate closes on attestation

### ER-STATE-17 — the fix verified against the genuine legacy file, not a synthetic shape

Round 12's State test 28 exercised the pre-0.8.4 `AnamorphRoot` shape with a hand-built root. It
now also takes the repository's **real frozen fixture** — `tests/fixtures/legacy_pre_0_8_4_view_params.xml`,
the file State test 6 guards, with its `My Vocal` preset, its two A/B slots and no
`ANAMORPH_INTERNAL` child — and mutates it **in place**: only the six Settings `PARAM` values are
replaced; width, mix, the preset name and baseline and both slots stay byte-for-byte what the file
carries. Three restores:

| restore | Settings in the file | expected | surroundings |
|---|---|---|---|
| untouched | `2.0 / 1.0 / 0.25 / 0 / 1 / 1` | State test 6's values, re-asserted: ids 3 / 2, 0.25, false / true / true | intact |
| every Setting malformed | `nan / 1e39 / inf / abc / nan / -inf` | defaults: ids 1 / 3, 0.5, false / **false** / true | intact |
| finite, outside every domain | `7 / 7 / 5` | clamped: ids 4 / 5, 1.0 | intact |

"Intact" is asserted, not assumed: width 0.8 and mix 0.65 restore, the preset name is `My Vocal`,
the active slot is 0, slot B's name survives a re-save, the re-saved Settings ids are in domain, and
the DSP atomic agrees with the tree. The one value that *changes* under the malformed restore —
`tooltipsOn`, true in the file, false after `"nan"` — is the documented rule (malformed → the
field's default), stated in the check's own label so it cannot be mistaken for a regression.

**Verified discriminating on the real file too:** the real-fixture legs fail without the fix and
pass with it; the untouched restore passes either way, which is the point — the fix moves nothing
that was valid.

### Compatibility checklist items 5 and 7 — CLOSED on the maintainer's attestation

The maintainer has confirmed **Host matrix verified** and **Automation playback verified** as
completed and verified. Rounds 7–10 declined to tick these from the Level-5 audition record, whose
per-item outcomes are NOT RECORDED, and said what would close them: *"Either the maintainer confirms
those groups were exercised, or the two items are run on their own."* This is the maintainer
confirming.

Recorded the way round 6 recorded the audition — the verdict, the date and the performer — with
the fields the confirmation did not supply (hosts, operating systems, plug-in formats, automation
lanes) marked **NOT RECORDED** rather than inferred from `COMPATIBILITY_MATRIX.md` or from the
audition protocol. Nothing was performed in this environment, which is headless; nothing is claimed
to have been.

**Consequence.** `RELEASE_POLICY.md` precondition 2 is satisfied; the checklist is **eight of eight**
(six measured, two attested). The engineering/process precondition list for v0.9.6 is now empty.
**One tag blocker remains — the missing licence, KI-015** — and it is an owner/legal action.
`HANDOVER.md` (Release Status, Known Blockers, Roadmap, the tag-order sentence) and
`COMMERCIAL_STATUS.md` §6 say so.

---

## Round 12 — 2026-09-01 — undefined behaviour in the legacy Settings path, a deterministic latency test, and ER-STATE-13 on AArch64

### ER-STATE-17 — CONFIRMED and FIXED — malformed legacy Settings hit an undefined conversion

**Reproduced first**, through the real v0.2 restore, with `--legacy-settings-probe`. Round 11's
ER-STATE-15 work is what exposed this: it routed more inputs into `migrateFromLegacyApvts`, whose
`(int)` cast is undefined for NaN, ±infinity and out-of-range doubles ([conv.fpint]) — and JUCE's
own text parser **accepts "nan" and "inf" as numbers** (`juce_CharacterFunctions.h:254-273`), so
those reach the cast rather than being rejected as text.

Measured on x86-64, per input, in the tree after restore:

| `value=` | `int_oversample` after migration | consequence |
|---|---|---|
| `"nan"`, `"inf"`, `"-inf"`, `"1e39"`, `"-1e39"` | **−2147483647** | impossible ComboBox id (domain 1..4), **re-saved with the session** |
| `"2147483647"` | **−2147483648** | a *second* UB — signed overflow in the `+ 1` |
| `"7"` | 8 | finite but out of domain |
| `"abc"`, `""`, `"0x10"` | 1 | already safe (parse → 0) |

`scopePersist` was worse: NaN, ±inf and out-of-range values passed through **unclamped** into a
0..1 field. And the corruption was platform-dependent — AArch64 saturates the same inputs
differently (NaN → 0, positive overflow → INT_MAX, which the `+ 1` then overflows).

**Fix** (`src/InternalState.h`): each legacy value now passes the *same* usability predicate the
session and preset paths already share (`SerializedNumber.h` — plain decimal text, finite after the
float narrowing), so anything malformed means the field's **default**, exactly as an absent node
does; and the choice indices are clamped into the ComboBox domain **in double, before** the integer
conversion, so that conversion is defined for every input the lambda can return and the `+ 1`
cannot overflow. A finite out-of-domain value lands on the nearest valid choice, which is what
`NormalisableRange` does for an out-of-range parameter. No serialization format change.

**Regression coverage: State test 28** — 88 checks over both legacy shapes (the bare v0.2 tree and
the pre-0.8.4 `AnamorphRoot`), asserting for each field that the tree value is in domain, that the
DSP atomic agrees with the tree, and that a re-save writes the repaired value. **Verified
discriminating: 49 checks fail without the fix**, 0 with it. Valid legacy migration is unchanged —
State tests 5 and 6 pass untouched.

### ER-STATE-14 — the latency regression gap is now closed DETERMINISTICALLY

Round 11 said out loud that its stress leg did not discriminate the fix. It now does, without a
test hook in production code and without sleeps standing in for synchronisation.

The barrier is one the **product already provides**: `AudioProcessor::setLatencySamples()` notifies
its `AudioProcessorListener`s synchronously, from inside the call, whenever the reported value
changes (pinned JUCE 9.0.1, `juce_AudioProcessor.cpp:415-436`), and the listener lock is released
before each callback (`:425-429`), so a listener may block. A test listener holds delivery #1 open
while a real off-message thread makes request #2 *inside* it. The interleaving is forced, not hoped
for: delivery starts → another thread requests → delivery completes → the second request must still
be served.

**Measured both ways, three runs each.** Against a `clear-AFTER-deliver` build the leg fails
deterministically — `delivery #1 -> 4 with request #2 made inside it; next tick -> 4 (expected 0)`,
3/3. Against the shipped code it passes 3/3.

**What it still does not reach**, stated so nobody reads green here as more than it is: the
round-11 double-clear window itself lay between `timerCallback`'s `exchange(0)` and the second
`store(0)` the old `updateLatency()` did at its entry — two adjacent atomics on one thread with no
call between them. No external mechanism can place a request there, so the pre-round-11 code passes
this leg too. That fix, and the `relaxed`→release/acquire change beside it, rest on inspection.

The two waits are bounded polls for the processor's own 20 Hz timer (deadline 40 periods), not
sleeps: the outcome is fixed the moment the request is or is not in the flag.

### D-1 record consistency — TWO documents were stale, corrected

No production change (D-1 is approved and implemented; not reopened). The check found the record
had **not** kept up:

- `KNOWN_ISSUES.md`'s KI-027 row still read *"**fix gated** — … awaiting maintainer sign-off"*, and
  the entry was still OPEN, three rounds after the approval landed. Row struck through and marked
  RESOLVED under D-1, with a dated banner over the detail section keeping the round-1/2 diagnosis
  as the record.
- `THREADING_POLICY.md` still listed KI-027 as a *"Known violation … awaiting Architecture
  Review"*. Rewritten: the rule now holds by construction, with the mechanism named.

### ER-RT-05 — verified accurate for the fourth consecutive round, no change

`AUDIO_FN` is a manual registry and the script says so; `REALTIME_AUDIO_POLICY`,
`REALTIME_SAFETY_AUDIT`, `CI_CD` and ADR-0029 all describe the same-file closure correctly and none
claims automatic cross-file discovery. No document changed.

### ER-STATE-13 on AArch64 — conclusion UNCHANGED, code untouched

Cross-built `AnamorphTests` for `aarch64-linux-gnu` and ran it under `qemu-aarch64-static`. The full
DSP suite passes there: **245 checks, 0 failures**. The engine-level twin probe
(`--match-inject-probe`, contaminated engine vs control engine, same material, injection exactly as
`abSwitchTo` performs it) was run on both architectures over six scenarios.

| scenario | x86-64 | AArch64 |
|---|---|---|
| transparent chain, settled, inject +9 | 8.263 dB | **8.263 dB** |
| engaged chain, settled, inject +9 | 9.017 dB | **9.017 dB** |
| engaged chain, no settle (worst case), inject +9 | 10.734 dB | **10.734 dB** |
| fixture chain, settled, inject the REAL stale −1.0 | 0.225 dB | **0.225 dB** |
| fixture chain, settled, inject +9 | 9.014 dB | **9.014 dB** |
| large-delta chain, inject −1.0 | 0.133 dB | **0.133 dB** |

Identical to three decimals in every scenario; the single difference anywhere in the output is one
ULP of an RMS accumulation at one block (0.78801 vs 0.78802), which is float summation order, not
behaviour. `std::atomic<float>::is_always_lock_free = 1` and `sizeof(float[2]) = 8, alignof = 4` on
both, so no layout, initialisation-order or atomics difference is available to expose anything.
`abMatchGain` is still not serialized on either architecture, and no reader observes it before
`abEnsureInit()` — it is not part of `StateSet` at all; `abSwitchTo` reads it directly, after
`abApplySlot`.

**Disposition: unchanged, no production change**, per the brief's condition that only a real
AArch64-specific impact would justify one. There is none.

**One refinement to record, and it is architecture-independent.** Round 9 called the stale value
"inert" from a processor-level measurement. The engine-level twin probe is the more sensitive
instrument and shows the transient is roughly **proportional to |stale − settled-at-injection|**,
decaying as the loudness measurement reasserts: ~9.7 dB of delta gives 8-10 dB, ~0.27 dB of delta
gives 0.225 dB. Round 9's "indistinguishable" holds for the case it measured — the residual sits
under that probe's RMS noise floor — but "inert" was too strong as a general statement. The
mechanism is real, bounded by the delta, and self-correcting; it is still not worth a fix, and the
value that *should* be written remains undocumented, which is why round 9 declined to choose one.

**A misread of my own was caught here too.** The processor probe printed that an A→B switch
"SWITCHED OFF" Level Match "because the harness never flushed the tree". Wrong on the cause:
`currentStateSet()` → `copyStateWithRawValues()` writes a fresh `@raw` for every PARAM from the live
parameter and `reassertParameters` reads `@raw` first, so the slot snapshot is faithful. The stale
`@value` in the tree is real but not what the slot carries, and Match going off in that minimal case
is correct A/B behaviour — slot B still held the construction snapshot, which predates the enable.
Corrected in the probe's output and comment.

---

## Round 11 — 2026-09-01 — two of three restore issues fixed, one refuted, and the changelog audited

Three reported items and a full staleness audit of the `[0.9.6]` release notes. **Two fixed, one
refuted by measurement, and one changelog entry corrected — my own, from round 8.**

### ER-STATE-14 — latency request lost between two clears — FIXED (by inspection, not by test)

`timerCallback` cleared the flag with `exchange(0)` and then called `updateLatency()`, which cleared
it a **second** time. Anything the audio thread stored between those two clears was dropped — and a
dropped request is a *permanently* stale reported latency, because the store is unacknowledged and
nothing re-raises it until the next unrelated move or a re-prepare.

Fix: split out `deliverLatency()` (delivery only, never touches the flag). `timerCallback`'s
`exchange` IS its clear, so it calls that; `updateLatency()` stays clear-then-deliver for the
message-thread and `prepareToPlay` callers. Requests landing *during* a delivery now survive to the
next tick, which is what clearing before reading buys. The store also became **release** and the
consumers **acquire**: the flag is not the payload — the parameter write that raised it is — and
under `relaxed` on both sides there is no happens-before edge between them, so a consumer may see
the flag without seeing the value. x86-64's store ordering hides that; the AArch64 targets do not.

**What the test proves, stated exactly.** State test 27's first leg pins the D-1 invariant end to
end (after off-thread automation drains, the host holds what the settled state predicts) and is a
real regression guard for the machinery. It does **not** discriminate either half of this fix:
both were measured against it on x86-64 and it passes with and without them. Both rest on
inspection, and the test says so in its own comment rather than implying coverage it lacks.

Two harness defects were found while establishing that, and both had made earlier versions lie.
The leg first compared **0 with 0** — latency only moves with drive when oversampling is on. Then it
"quiesced" with a tight loop of `callPendingTimersSynchronously()`, which fires **nothing** against
a 20 Hz timer because the countdown is never due; that produced intermittent failures which were
very nearly attributed to the product. A first version had also masked the defect entirely by
ending with a message-thread write, which delivers synchronously and repairs a lost request.

### ER-STATE-15 — a root with no sound child adopted metadata anyway — FIXED

An `AnamorphRoot` whose `ANAMORPH` child is absent restores **no parameter at all**, yet the
function fell through to the adoption block: preset name, indicator tick, dirty baseline and the
host-hidden Settings were all taken from it. The result described a session that had never loaded,
over a sound that had not changed.

That is the failure the foreign-root branch at the bottom of the same function already returns to
avoid, and the registry states the rule for both — *"input we do not recognise never becomes
state"*. A root missing its only sound-bearing child is that case wearing a recognised tag. It now
returns. `getStateInformation` appends the child unconditionally, so **no session this plug-in has
ever written reaches the new branch** and no valid session changes behaviour.

Verified discriminating: reverting the guard fails three checks of State test 27 (the preset name
adopted as "Incoming Name" over the live "Live Name", the Settings adopted, the identity adopted).

One existing test had to be repaired rather than the fix weakened: State test 7's out-of-range
`active` guard built its blob from an `AB` node **alone**, which is now not a restore, so the clamp
was no longer reached. It now builds the root from a genuine save, keeping the clamp on the path a
real out-of-range blob takes.

### ER-STATE-16 — rejected slot keeps its metadata — REFUTED

Implemented, measured, reverted. Gating `readSlot`'s name/baseline/identity reads on
`dst.params.isValid()` **changes no test outcome**, because the premise does not hold:
`StateSet::isValid()` is `params.isValid()`, so a rejected payload leaves the slot invalid, and
`abEnsureInit()` assigns `slot = currentStateSet()` — the **whole struct, metadata included**, not
just the params. Every reader of `abSlot[]` (`abSwitchTo`, `abCopyToOther`, `getStateInformation`)
calls `abEnsureInit` first, so the values `readSlot` wrote are unreachable.

The legs stay in State test 27, relabelled a **contract pin** rather than a regression guard — they
pass either way, and saying so is the difference between coverage and the appearance of it. The
reasoning is recorded in `SERIALIZATION_REGISTRY.md` beside the `AB` child so the question is not
reopened a third time.

### The `[0.9.6]` changelog audit — 18 entries, one defect, in my own entry

Every entry was checked against the tree by an independent reader, and each staleness claim was then
put to an adversarial refuter. **16 accurate. One claim refuted on verify** (entry 16's "preset" in
the headline: the engine has one forced-duck entry point and cannot distinguish A/B from preset from
undo, and the product's own comments name the class in the same words — a wording preference, not an
overclaim). **One confirmed, and it was mine.**

Entry 1 — the round-8 A/B fix — attributed the bug entirely to instance reuse: *"opened into a
plug-in instance that had already been used — which is what a host does"*. Round 8's own measurement
says otherwise, in a comment I wrote: the constructor calls `abEnsureInit()` **eagerly**, so a
**fresh** instance was affected too, keeping the open/Default snapshot instead of the restored
session (State test 26 leg 3, which failed at 0.5 against a restored 0.75). The omission misled in
the practical direction — a user opening a project cold would read the reuse clause and conclude
they were unaffected. Rewritten to name both cases.

That is the second time this programme has caught a claim of mine that the evidence in the same
change set contradicted. The pattern in both: the headline was written from the reproduction I ran
first, and not revised when a later leg widened the finding.

---

## Round 10 — 2026-09-01 — release notes reconciled with the KI-013 outcome

**Documentation only; no code touched.** A reported contradiction between the `[0.9.6]` release
notes and KI-013's RESOLVED status turned out to be one of **three** sites still describing the
pre-round-4 world, not one.

### The contradiction, and the two the report did not name

Round 4 closed KI-013 and KI-028 by giving both editor predicates a real button signal
(`anyPhysicalMouseButtonDown()` → `+[NSEvent pressedMouseButtons]`; `PluginEditor.cpp:1533` and
`:1719`). Round 8 corrected the source comments. Three documents were still saying otherwise:

1. **`CHANGELOG.md`** — *"Known limit: this reconcile is inert on macOS … so the macOS half of the
   issue remains open."* The reported one. Removed.
2. **`docs/KNOWN_ISSUES.md`, the KI-013 detail section** — written entirely in the present tense
   ("On macOS the mechanism **is** inert", "macOS behaviour **is** therefore unchanged"), and its
   closing bullet offered as a hypothetical the very fix that shipped: *"Fixable only via a
   JUCE-side change or a platform-specific `pressedMouseButtons` query (would need its own review)."*
   The registry row two hundred lines above already said RESOLVED. Given a dated RESOLVED banner and
   moved to past tense; the closing bullet now says that is what round 4 did.
3. **`docs/KNOWN_ISSUES.md`, the KI-028 detail section** — its round-3 banner still read *"The macOS
   residual is why this entry stays open."* Given a `CLOSED 2026-09-01 (round 4)` banner beneath it.

Both KNOWN_ISSUES sections keep their original diagnosis text under an explicit "everything below is
the round-2/3 record" banner rather than being rewritten — the programme's rule is not to rewrite
historical records, and the diagnosis is why the fix was findable.

**Deliberately NOT changed**, because they are accurate history rather than live claims:
`docs/KNOWN_ISSUES.md:97` (a dated v0.8.12 version-sync header recording KI-013 as *added* at that
release — true then), the dated round entries in `DOCUMENTATION_COVERAGE.md`, and the round-3/4
sections of this worklog and the older audit worklogs.

### The two Undo entries were duplicated, and one of them implied macOS was broken

`[0.9.6]` carried both *"…no longer breaks Undo on macOS"* and *"…no longer breaks Undo if you
release the mouse outside the plug-in window"*. They are **one bug path fixed in two stages within
one unreleased version**: round 3's sweep closed Linux and Windows, round 4's signal closed macOS.
A user upgrading from 0.9.5 never saw the intermediate state, so presenting it as two fixes — one of
them platform-qualified — implies macOS was separately broken in a shipped build. It was not.

Consolidated into a single entry describing the final state on all three platforms. **The one half
that genuinely shipped broken is preserved**: a knob staying visually "pressed" after a release
outside the window on macOS is KI-013, present since v0.8.12, and the merged entry still names it.
Both regression tests stay cited (State tests 21 and 23).

The `[0.9.6]` Fixed count goes 19 → 18, which happens to restore `HANDOVER.md`'s "eighteen Fixed
entries" to accuracy — that line had gone stale by one when round 8 added the ER-STATE-12 entry.
Checked rather than assumed: the section now counts 18.

The third readout entry — *"Dragging a knob's number readout now registers with Undo and with host
automation recording"* (round 1, the drag path opening no gesture at all) — is a different defect
and is left alone.

### ER-RT-05 — verified accurate for the third consecutive round, no change

Re-checked at `scripts/check-realtime.py:87`. The two facts to confirm both hold:
**AUDIO_FN is a manual registry** — the script says so in as many words (*"this list IS the scope"*),
and records why `setParameters`/`toEngine` were hand-added: `processBlock` calls both every block but
each lives in a file the same-file walk cannot reach from that seed. **Cross-file callees are not
auto-discovered** — `REALTIME_AUDIO_POLICY.md`: *"A callee whose definition lives in another
translation unit is not text this lint has, and it is covered only if its own name is a seed."*
`REALTIME_SAFETY_AUDIT.md`: *"the same-file transitive closure … not a whole-program one."*
`CI_CD.md` and ADR-0029 agree. No document claims complete automatic cross-file coverage.

Verified in rounds 8, 9 and 10 with no change required each time. Recorded here so a fourth round
can cite this rather than re-derive it: the boundary is stable, the documents are accurate, and the
census (83 matches / 12 files) has not moved since round 3.

---

## Round 9 — 2026-09-01 — the per-slot Level-Match residual: mechanism real, impact refuted

**No code changed this round.** The reported bug's mechanism exists; its reported IMPACT does not,
and the measurements that establish that are below. The review's own decision rule made this the
answer rather than a judgement call: CONFIRMED requires stale state **and** a first-switch output
level change. The first holds. The second is refuted by three independent measurements.

### ER-STATE-13 — REFUTED on impact; the stale state is real but inert

**The stale state is real, and wider than the report says.** `abMatchGain[2]` is written and read in
exactly one place — `abSwitchTo` (`src/PluginProcessor.cpp:762`, `:765`) — initialised in the header
(`:203`) and **never reset on any path and never serialized**. So `abResetToDefaults` not covering it
is not a gap peculiar to the no-A/B path: the `AB`-PRESENT restore leaves it equally stale, because
there is no `slotAMatch`/`slotBMatch` field in the format at all. Any fix scoped to
`abResetToDefaults` would close a third of the class while looking complete.

**The reported impact does not occur.** `abSwitchTo` does inject the previous project's figure —
the probe shows the engine's match gain jumping from the restored −7.10 dB to −2.18 dB on the block
the injection lands, tracking the previous project's B across two runs (prev-B −1.045 → peak
−2.181; prev-B −1.782 → peak −2.798, a 0.62 dB response to a 0.74 dB change). But the **output level
does not move**:

| measurement | contaminated | control | verdict |
|---|---|---|---|
| matched counterfactual, one instance, identical params across the switch (per-block RMS, blocks 4–10) | 0.3553 0.3631 0.3623 0.3607 0.3676 0.3603 0.3596 | 0.3592 0.3653 0.3639 0.3624 0.3642 0.3617 0.3711 | indistinguishable |
| fresh-instance control, settled window | 0.361551 | 0.366076 | −0.11 dB, within run-to-run spread |
| **worst case** — switch with NO settle, before loudness has converged | 0.3577 0.3743 0.3671 0.3705 … match −6.180 dB | 0.3578 0.3548 0.3715 0.3699 … match −6.116 dB | indistinguishable |

The matched counterfactual is the decisive one and is only possible *because* of round 8: with both
slots now holding the restored state, an A→B switch applies **identical parameters**, so the
injected match gain is the single variable. A→B→A also decontaminates the array by construction
(each switch stores the CURRENT match into the slot it leaves), so a later A→B on the same instance
is a clean control with the same instance, same loudness history and same parameters.

**Why it is inert**, stated mechanically rather than as a guess: the injection lands at the **silent
bottom of the switch duck** (`AnamorphEngine.cpp:782`, `~6 ms` fade-out), where the output is muted;
and `setParameters` re-targets `matchGainSmooth` from `loudness.getMatchGainDb()` **every block**
(`:529`), so the live measurement supersedes the injected figure before the 28 ms fade-in completes.
Level Match is a continuously re-derived measurement, not stored state — which is exactly why
overwriting it briefly changes nothing that reaches the output.

**The one real residual** is a **readout excursion**: `loudness.setDisplayedGainDb(inj)` moves the
figure the editor displays (`PluginProcessor.cpp:256`), measured at −7.10 → −2.18 dB, converging back
within 6–8 blocks (≈65–85 ms at 512/48 k). Sub-100 ms, during and just after a switch the user
initiated, on a readout that is visibly a live meter.

**Why no code change**, beyond "not CONFIRMED under the rule":
1. **The correct value is not documented.** Reset to `0.0f` (the construction default) or seed from
   the live `engine.getMatchGainDb()` (the `abEnsureInit` analogue)? `abMatchGain` appears in no
   registry, no ADR and no policy — unlike round 8's fix, whose semantics `SERIALIZATION_REGISTRY.md`
   already specified in as many words. Choosing here would be legislating, not conforming.
2. **A scoped fix would misrepresent the class.** The member is stale after *every* restore, not
   just this one.
3. **Nothing observable would change.** By the mechanism above, both candidate values are superseded
   by the running measurement before any audio at full level passes through — which is what the
   worst-case leg measures.

Recorded here so a later round re-measures rather than re-derives. Instrument:
`AnamorphStateTests --legacy-match-probe`, kept in the tree with its per-block trajectories.

**Two probe defects were found and fixed before the numbers above were trusted.** The first version
read `getMatchGainDb()` immediately after `abSwitchTo` and reported "not the stale value" — the
injection is consumed inside `processBlock`, which had not run, so it was reading the pre-switch
value. The second measured output RMS in a window starting at the switch and reported a −2.60 dB
"level change" that was the switch duck's 34 ms fade. Both are the vacuity class this programme
keeps hitting: a probe that answers a different question than the one asked.

### ER-RT-05 — DOCUMENTED, unchanged (re-verified)

Re-checked at the review's new line (`scripts/check-realtime.py:87`, the `AUDIO_FN` seed list — the
same construct round 8 examined at `:107`, moved by that round's own edits). The four documents that
describe the boundary still describe it correctly and none implies automatic cross-file coverage.
Census re-measured: **83 FORBIDDEN-class matches across 12 files**, with VelvetNoise 3, ChorusEngine
2, HaasProcessor 2 — identical to rounds 3 and 8. No code, no documentation change.

*A methodology slip worth recording:* the first re-measurement this round returned **205 matches in
29 files**, because it ran the forbidden-class regexes over raw text instead of applying the
script's own `strip_comments_and_strings` first. Same tree, different method — caught and re-run
before it was reported. A census is only comparable to a prior census run the same way.

### Informational items — fourteen checked, no contradiction

Each cited line was resolved against the current tree rather than accepted: `:157` is
`requestLatencyUpdate`, `:408` the restore-notify rationale, `:443` the negated finiteness test,
`:495`/`:504`/`:516` the malformed-value repair region, `:921` the legacy slot type guard. The DSP,
GUI and CI items are unchanged since the rounds that established them. No contradiction found.

### Settled decisions

D-2 not reopened; D-3 remains the maintainer's completed audition (PASS); D-1 and D-4 remain
implemented; KI-028 and KI-013 remain RESOLVED; round 8's ER-STATE-12 fix is untouched.

---

## Round 8 — 2026-09-01 — one confirmed defect, one obsolete comment, one boundary left alone

Follow-up on a supplied review. Three actionable items, three different dispositions, and the
dispositions were decided by running things rather than by reading them.

### ER-STATE-12 — CONFIRMED — a restore with no A/B data keeps the previous project's slots

**Reproduced first, on the current tree, before any code was touched.** The instrument is
`AnamorphStateTests --legacy-ab-probe`: seed a "previous project" with distinguishable A and B
sounds (raw width 0.90 / 0.10), restore a v0.2 session into the SAME instance, then switch slots
and read back.

| leg | pre-fix | post-fix |
|---|---|---|
| v0.2 restore, then switch to B | **0.10** — the previous project's B | 0.75 — the restored state |
| v0.2 restore with the previous project left active on B, first switch reads A | **0.90** — its A; and `active` came back as **1**, its index | 0.75; `active` = 0 |
| `AnamorphRoot` with its `AB` child stripped, then switch to B | **0.10** | 0.90 — the restored state |
| FRESH instance, v0.2 restore, switch to B | **0.5** — the Default snapshot | 0.75 |

Both slots are demonstrated stale, not just the one the review named, and so is the active index.
The restored value differs from both previous-project values, which the probe asserts before
reading anything — otherwise every row above could agree while carrying stale state.

**The fresh-instance leg refuted its own premise.** It was written expecting a vacuous pass: the
slots "start invalid", so nothing should need resetting. It failed at 0.5. The constructor calls
`abEnsureInit()` EAGERLY — deliberately, so B is not born as a copy of an already-edited A — so by
the time any restore arrives both slots are already valid, holding the open/Default state. A fresh
instance was therefore affected too, with the construction snapshot in the previous project's
place. The comment that had described this as the easy leg was corrected, not the measurement.

**Root cause.** `readSlot` already enforces the right rule — "absent means the default, not whatever
the previous session left here" — but it is *called from inside the `AB` node's branch*, so it can
only reach a blob that HAS that node. Two restore paths carry no A/B data and never reached it: an
`AnamorphRoot` with no `AB` child (every field in the `AB` table is "Required: No", so the node is
optional) and the v0.2 bare-APVTS branch, which predates the feature entirely. `abSlot[]` and
`abActive` are processor members, and a host restores into one live instance repeatedly.

**Ownership and lifetime** (review step 7): `abSlot[2]` and `abActive` are members of
`AnamorphAudioProcessor` — lifetime of the INSTANCE, meaning per-SESSION. That mismatch is the
whole defect; every other restore-side member in this function is already reset for the same reason
(`internal` via `migrateFromLegacyApvts`, the preset name, the baseline, the undo stacks).

**How other paths handle it** (step 8): the `AB`-present path resets the whole slot before
overlaying, and takes `active` from the node with a default of 0. Construction seeds both slots from
the live state. Only the two paths above did nothing.

**Which semantics are correct** (step 9) — and this was NOT inferred from the code. The repository
already specifies it. `SERIALIZATION_REGISTRY.md`'s `AB` table gives `active` a default of **0** and
the slot params a default of **"lazily initialised from current"**, and the prose beside it states
the rule in as many words: *"absent must mean the default rather than 'whatever the previous session
left' — and that has to hold for the slot as a WHOLE, not field by field."* So the answer is
**deterministic reseed**, not "clear" and not "migrate" (there is nothing to migrate: v0.2 carries
no A/B data). No new compatibility decision is required and no architecture gate is triggered — this
restores conformance to a contract already recorded, changes no schema field, and leaves every blob
that carries an `AB` node restoring exactly as before.

**The fix.** `abResetToDefaults()` — six lines beside `abEnsureInit()` — invalidates both slots and
sets `abActive = 0`, called from the two paths that carry no A/B data. INVALIDATING rather than
seeding is what makes it correct at that point in the restore: `abEnsureInit()` re-seeds from
`currentStateSet()` at first use, which is after the restore has finished, so the slots come back
holding the state that was just restored rather than a snapshot taken mid-restore.

**Regression coverage: State test 26**, five legs — v0.2/reused/B, v0.2/reused/A (reached by leaving
the previous project active on B, since switching away from a slot stores the live state into it and
would otherwise hide contamination), fresh instance, `AB`-stripped root, and a fifth leg that pins
the *other* direction: a root that DOES carry an `AB` node still restores both slots' own sounds, so
the reset cannot be erasing legitimate data. Verified to FAIL without the fix: **8 checks**, naming
both slots, the active index and the fresh-instance case.

### ER-GUI-04 — CONFIRMED stale, comment only

`PluginEditor.cpp`'s SCOPE paragraph still said the caller's predicate is *inert on macOS* and that
KI-028 was *narrowed to a macOS residual* it "does not close". Round 4 closed it:
`anamorph::gui::anyPhysicalMouseButtonDown()` reads `+[NSEvent pressedMouseButtons]`, and
`KNOWN_ISSUES.md` carries KI-028 and KI-013 as **RESOLVED**. Rewritten to say what is true now.
`PluginEditor.h`'s doc comment for the same function carried the same claim in the same tense and
was corrected with it — same claim, same function, not an unrelated comment. No behaviour change;
the two sites that already described the fix correctly (`PluginEditor.cpp` at the predicate,
`PhysicalMouseButtons.h`) were left alone, since they were already in the past tense.

### ER-RT-05 — DOCUMENTED, no change

The cross-file lint boundary is real and the documentation already states it, in both places, in
terms that cannot be read as full-program coverage: `REALTIME_AUDIO_POLICY.md` — *"**That closure is
same-file.** A callee whose definition lives in another translation unit is not text this lint
has"*; `REALTIME_SAFETY_AUDIT.md` — *"the **same-file transitive closure** of the audio-path seeds,
not a whole-program one"*; `CI_CD.md` — *"every callee **defined in the same file** is scanned too,
transitively"*; ADR-0029 likewise. Nothing found that implies automatic cross-file coverage, so
there was nothing to correct and no code was touched.

The policy instructs re-measuring the census before relying on it, so it was re-measured rather than
assumed: **83 FORBIDDEN-class matches across 12 files**, identical to the round-3 record, with the
three cross-file-reachable DSP units unchanged (VelvetNoise 3, ChorusEngine 2, HaasProcessor 2) and
every one of those still inside that module's own `prepare()`. The gap is still real and still
empty. `abResetToDefaults` adds no forbidden-class construct and is not audio-thread code.

### Informational items — checked, one contradiction found

The fourteen informational items were checked against the current tree, not accepted on the
review's word. Thirteen hold. The one flagged as conditional — `LookAndFeel.cpp:813`, "no code
change required *unless the implementation and comment are found to contradict one another*" —
carries no macOS claim at all in that region, so there is no contradiction there; the stale claim
was one level up, in the editor, and is ER-GUI-04 above. The note under
`PluginProcessor.cpp:898` ("this does NOT prove A/B slot state is reset correctly") was exactly
right, and ER-STATE-12 is what it was pointing at.

### Settled decisions, re-verified untouched

D-2 not reopened; D-3 remains the maintainer's completed audition (PASS); D-1 and D-4 remain
approved and implemented; KI-028 and KI-013 remain RESOLVED. No warning baseline widened — the
warning set is byte-identical before and after this round's source changes, checked by building
both ways.

---

## Round 7 — 2026-09-01 — the compatibility checklist completed as far as a headless environment can

**`RELEASE_POLICY.md` precondition 2 was the second of the two remaining tag blockers, and it has
NARROWED, not closed.** `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` now stands at **six of
eight boxes checked with measured evidence**, recorded in that file's §Completion record with an
§Evidence appendix naming what was run for each. Precondition 2 requires *every* item, so the gate
is still open — but what is left is a DAW session, not analysis, and this section says exactly which
two and why they were not ticked.

| # | Item | v0.9.6 | Evidence |
|---|---|---|---|
| 1 | Parameter IDs unchanged | **PASS** | State test 2; `tests/fixtures/parameter_registry.snapshot` byte-identical to `origin/main`, last touched `d6bdb13` |
| 2 | Serialization schema verified | **PASS** | State tests 1, 3 + legacy fixtures 4/5/6 |
| 3 | Presets migrated | **PASS** | State tests 8, 10, 11; the "sound identical" half via the Level-5 audition (round 6) |
| 4 | Pluginval passed (both modes) | **PASS** | run HERE under `xvfb`, not inferred from CI |
| 5 | Host matrix verified | **OPEN** | needs a DAW |
| 6 | Latency reporting verified | **PASS** | `AnamorphTests` Test 3+4, plus State tests 22 and 24 |
| 7 | Automation playback verified | **OPEN** | needs a host |
| 8 | Session reload verified | **PASS** | State test 25 against a real v0.9.5 field capture |

### Item 8 stopped being a reconstruction

The checklist had carried a caveat since v0.8.13: the three legacy fixtures are *reconstructions*
built by current code, so they can only contain what today's understanding says an old format held.
A fixture written by the current binary cannot answer "does vN read what vN−1 wrote" — it answers
"does vN agree with itself", which is a weaker question wearing the same clothes.

So the previous version's binary was rebuilt and asked. The tree at `2c5e760^` (v0.9.5) was
extracted with `git archive`, its `tests/state_tests.cpp` given an `--emit-session` hook, and
`AnamorphStateTests` built against the same JUCE pin. That binary WROTE
`tests/fixtures/field_capture_v0_9_5.session` (10,629 bytes) and a `.manifest` recording what it
believed the state was, B slot included. **State test 25** loads the capture into v0.9.6 and asserts
against the manifest, not against v0.9.6's own round-trip.

All four things item 8 names reproduce exactly: sound (5 parameters), preset name (`Gentle Width`),
dirty-star (set), and **both** A/B slots — and the two slots hold different values, so the B leg is
not vacuous. This is the only fixture in the tree that was not built by current code.

### Item 4 was run, not inherited

Pluginval was executed in this environment under `xvfb` against the built VST3, at the strictness
read from `ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml` rather than from any
number restated in a document: `scripts/run-pluginval.sh <n> deterministic vst3` and
`scripts/run-pluginval.sh <n> randomise vst3`, both three passes, both exit 0. The macOS AU and
Windows gates run in CI and are not restated as if they had been run here.

### Why 5 and 7 were NOT ticked from the Level-5 audition

The audition (round 6) exercised a DAW and PASSED, and its protocol group C covers automation of
Drive and Algorithm. Ticking items 5 and 7 from it would nonetheless be **inferring per-item results
from a verdict-level record** — that record's per-item outcomes are explicitly NOT RECORDED, which
is the same blank set round 6 refused to fill in from the protocol. Refusing to invent them in round
6 and then reading them back out in round 7 would be the same fabrication taking one extra step.
Either the maintainer confirms those groups were exercised, or the two items are run on their own.

### Documents synced

`docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (completion record, per-item ticks, evidence
appendix), `docs/HANDOVER.md` (Release Status, Known Blockers, Roadmap and Test rows),
`docs/COMMERCIAL_STATUS.md` §6 — which said the checklist "has never been completed for this
release" and now states the six/eight position and why the last two are open —
`docs/REPOSITORY_MAP.md` (the state-suite and `tests/fixtures/` rows), and the test-count sweep
below.

**Counts corrected, measured not inferred:** the state suite is **25 tests / 1077 checks** (was
recorded as 24 / 1057) and the DSP suite **47 tests + the A/B clamp guard / 245 checks**.
`TESTING_POLICY.md` was the worst drifted — its Level-2 row still said *41 DSP tests* and *15
state-compatibility tests*, and its hard-release-gate paragraph *41* and *24-test*. Also corrected:
`README.md`, `docs/procedures/TESTING.md`, `docs/REPOSITORY_MAP.md`,
`docs/architecture/RELEASE_HARDENING_PLAN.md` and `docs/HANDOVER.md`.

No CHANGELOG entry: a test addition and a checklist completion are not user-visible
(`CHANGELOG_POLICY.md` rule 3). No code under `src/` changed in this round, so no
ARCHITECTURE_REVIEW_GATE category is touched.

### Remaining tag blockers

**One and a half.** The **missing licence (KI-015)** is untouched and remains wholly an owner/legal
action. The **compatibility checklist** is down to its two host-dependent boxes. Neither is fixable
by code.

---

## Round 6 — 2026-09-01 — D-3 recorded: the Level-5 audition PASSED

**D-3 is CLOSED. The Level-5 audition was completed by the maintainer against the final v0.9.6
build and PASSED.** That discharges `RELEASE_POLICY.md` precondition 7. **v0.9.6 is no longer
blocked by D-3**; every statement in this programme that said otherwise is superseded by this
section, and the round-5 text has been marked accordingly.

The full record is `docs/procedures/LEVEL5_AUDITION.md` §Recorded auditions.

### What the record contains, and what it does not

The maintainer's report supplies the verdict (**PASS**), the build (**the final v0.9.6 build**) and
the performer (**the maintainer**). It does not supply the audition date, the DAW, the OS or CPU
architecture, the plugin format, the session used, the per-item outcomes for protocol groups A–E,
or an exact artifact/commit identity.

**Those seven fields are recorded as NOT RECORDED and were left blank deliberately.** Round 5 wrote
the audition protocol and its record table; it would have been trivial — and wrong — to fill those
rows in from the protocol, because the protocol says what *should* be exercised, not what *was*.
Inventing them would produce a record that reads as item-level evidence while resting on nothing,
which is a worse failure than an honestly partial record. Anyone extending this record later must
have been present at the audition.

### Correspondence to the final build — what was actually verified

The instruction for this round was to verify the recorded evidence corresponds to the final v0.9.6
build. Stated precisely: **no artifact or commit identity was supplied, so that correspondence rests
on the maintainer's attestation rather than on anything checkable in this repository.** That is
sufficient for precondition 7, which is a human sign-off by definition and not a machine-checkable
artifact — but the basis of the claim is recorded rather than glossed, so a later reader can see
which parts are attested and which are verified.

What this repository *can* confirm, and does: the tree carries no other v0.9.6 audition record, so
this is the only one, and there is no competing or superseding account to reconcile it against.

### Carried forward for the next release

Capturing the record table at audition time costs a minute and makes the record self-supporting.
The blank rows in the v0.9.6 entry are the argument for doing that, and the note is in the protocol.

### Remaining tag blockers

Two, not three: **the missing licence (KI-015)** and **the compatibility checklist**. Neither is
fixable by code. D-3 is closed. *(Superseded in part by round 7: the checklist is now six of eight
boxes, with only the two host-dependent ones open.)*

---

## Round 5 — 2026-09-01 — release finalisation for v0.9.6

**Entry state:** round 4 pushed at `f572479`; CI green; D-1, D-4 and KI-028 all closed. Two review
items to process, a documentation drift sweep, a testing-methodology rule to record, and the
release blocker.

**Exit state:** both review items dispositioned by measurement, three documentation corrections
made, the state-mutation cycling rule recorded as `TESTING_POLICY` rule 3a, and the Level-5
audition **specified** (and performed by the maintainer shortly afterwards — round 6).

### D-3 — not done *by this round*; subsequently PERFORMED and PASSED

> **SUPERSEDED 2026-09-01 (round 6).** The maintainer completed the Level-5 audition against the
> final v0.9.6 build and it **PASSED**. D-3 is closed and **v0.9.6 is not blocked by it**. The
> account below is kept because its reasoning is still correct about what *this programme* can and
> cannot supply — it was never a claim that the audition would not happen, only that an automated
> agent cannot be the one to perform it. Record: `docs/procedures/LEVEL5_AUDITION.md`.

**At the time of round 5 the audition was unperformed.** That round did not discharge it and did not
claim to.

The reason is definitional, not circumstantial. `RELEASE_POLICY.md` precondition 7 calls Level 5
"**the human sign-off**"; `TESTING_POLICY.md` says it "cannot gate CI" and that a green build plus
a pluginval pass is "**ready to audition, not shipped**". It requires a person, a DAW, audio output
and ears. This programme runs in a headless Linux container with no DAW and no audio device, and an
audition record not produced by a human listening is not a Level-5 record whatever it contains.
Writing a DAW name, an OS and a "result" into the checklist would have satisfied the round's
reporting format and corrupted the one gate that exists precisely because CI cannot see what it
covers. It was not done.

**What was done instead, because it is the part that CAN be done from here:** the audition had
never been *specified*. Precondition 7 said it was required and nothing said what to listen to, so
its scope lived in whoever remembered the release. `docs/procedures/LEVEL5_AUDITION.md` now derives
that scope from the `[0.9.6]` change set — twelve items in five groups (activation/restore, A/B and
undo, latency and automation, damaged-state recovery, metering and host matrix), each naming the
specific failure it looks for, plus the record format and an explicit "partial is a legitimate
record; partial described as complete is not" rule. It also states why the 2026-08-15 v0.9.4
audition does not carry over: ADR-0031/0032 changed the x86-64 machine code everywhere, and 0.9.6
changed audible behaviour in exactly the windows that were previously defective.

Item 7 of that protocol is called out for macOS specifically — the value-box release-outside path
was the last platform fixed (round 4), and its fix uses an AppKit-only query whose discriminating
test is `#if JUCE_MAC`. Automated coverage there comes from the macOS CI job; the *audible* half
has never been auditioned on any platform.

### Review item 1 — cross-file lint coverage: DOCUMENTED, code unchanged

The instruction was to verify the documentation states the boundary and to correct only a document
that falsely implies full cross-file coverage. Round 3 had put the measured census in
`scripts/check-realtime.py`'s own docstring, but two documents describing the lint had drifted:

- **`docs/policies/REALTIME_AUDIO_POLICY.md`** said "only the bound bodies are scanned". That is
  **factually stale**, and had been since round 1: the lint computes the transitive set of bodies
  the seeds reach, so a helper is scanned because it is CALLED from an audio path, not because of
  its name. The sentence also predated ER-RT-02's `setParameters`/`toEngine` seeds. Corrected, and
  the same clause now states that the closure is **same-file** and that a cross-file callee is
  covered only if its own name is a seed.
- **`docs/architecture/REALTIME_SAFETY_AUDIT.md`** described the tier as scanning "audio-path
  bodies" without qualification — in the one document whose job is to say what is and is not
  covered, that reads as whole-program. One clause added naming the same-file boundary.

No code changed; the lint was not redesigned. Both corrections point at the script's census rather
than restating a number that would go stale.

### Review item 2 — AllocationGuard on gcc-16: DOCUMENTED, priority lowered

The review carried round 4's "4 hits → 6 hits" observation. **That premise is wrong, and this round
corrected it at source.** Those counts came from the round-4 *synthetic probe TU*, which
deliberately seeds a genuine `std::free`-on-`new[]` mismatch; they describe that scaffold, not this
repository's code. Measured on the **real `tests/dsp_tests.cpp`**, same flags:

| | no `-flto` | `-flto` |
|---|---|---|
| g++-15 | 78 | 0 |
| g++-16 | **69** | 0 |

On the actual file gcc-16 emits **fewer**, not more, so the "rise" does not exist outside the
probe. Working through the review's three questions:

1. **Cause** — diagnostic-attribution drift across compiler majors over the guard's *replaced*
   global operators. Not a constexpr difference, not a sanitizer interaction (none is involved),
   and not a change in what the code allocates. GCC attributes an allocation to the replaced
   `operator new[]` and cannot follow it to the real allocator, which is the false-positive-by-
   construction the exclusion has always rested on.
2. **Where it occurs** — no-LTO only. Under `-flto` the count is **0 on every compiler measured
   (13.3, 15.2, 16)**, and the gate job builds `-flto`. It cannot reach a shipped binary at all:
   `tests/AllocationGuard.h` is included by `tests/dsp_tests.cpp` **alone** — it appears nowhere in
   `src/` and nowhere in `CMakeLists.txt`, so the VST3, AU and Standalone never contain the
   replaced operators this diagnostic fires on.
3. **Do the hits mean runtime allocation** — no. `-Wmismatched-new-delete` is a *static* pairing
   diagnostic; a hit count is not evidence about the audio path in either direction. The runtime
   question has its own measured answer: **Test 38 arms real counters around `process()` across the
   algorithm x oversampling x M/S matrix — 3,840 armed calls, worst per call new=0 malloc=0**, with
   all three guard halves reporting LIVE first so the zero is not vacuous.

By the review's own decision rule — "if this only affects no-LTO instrumentation/tests and release
builds are clean: document and lower priority" — this is instrumentation-only, no-LTO-only,
test-binary-only. **Documented in `scripts/check-gcc-warnings.py` and lowered.** No allocation-policy
change, no RT finding filed, no suppression, no baseline widened.

### Testing methodology — TESTING_POLICY rule 3a

Round 4's lesson is now a rule rather than a story: a state-mutation test must **cycle**, not just
transition — `A -> B -> C -> B`, and `valid -> invalid -> valid -> invalid` where malformed
recovery is involved — across `setStateInformation`, preset loading, parameter migration and the
restore paths. The rule records *why* it was bought: ER-STATE-11 was probed twice and refuted both
times, the second time with a working non-vacuity control, because a first restore is correct by
accident (the live `InternalState` holds an int where a round-tripped blob holds a string, so the
oversample callback fires and recomputes what the repair left stale). ER-STATE-07 fell to the same
shape a round earlier. It is written as a coverage-DESIGN rule, explicitly not a mandate to add
tests to paths already covered — this round added none.

### Documentation drift sweep — three corrections, all evidenced

1. `CHANGELOG.md` `[0.9.6]` dated **2026-08-31 → 2026-09-01**, the release date, matching where
   rounds 3-5 actually landed.
2. `docs/HANDOVER.md` roadmap row stated the Level-5 audition "was performed 2026-08-15 against the
   shipping v0.9.4 build" with nothing marking it superseded. Now says it is **INVALID for v0.9.6**
   and why, pointing at the new protocol. (The `Known Blockers` row already listed the "re-opened
   Level-5 audition" correctly and was left alone.)
3. The two lint-coverage corrections above.

**Deliberately not touched:** `docs/DOCUMENTATION_COVERAGE.md`'s account of the 2026-08-15 audition
discharging ADR-0026/ADR-0022 for the JUCE 9.0 line. That is a true historical record of what that
audition did for that build, and the brief's rule is that history stays history.

### Decisions carried unchanged

**D-2 remains DEFERRED** — no mutex, no `callAsync`, no state-architecture change, no code touched.
The measured races stand as recorded: `abActive`, the `abUndo` vectors, the `juce::String` refcount.
**D-4 remains APPROVED and implemented.** **ER-STATE-04.5 not reopened**; the saved blob format is
untouched. The fifteen informational review items were verified as already-true statements about
the tree and required no change.

---

## Round 4 — 2026-09-01 — executing the approved plan

**Entry state:** round 3 closed. CI **red** on one gate — two `-Wunused-lambda-capture` sites in
round 3's own Test 48. Four maintainer decisions arrived: **D-4 APPROVED**, **KI-028 Option B
APPROVED**, **D-1 APPROVED**, **D-2 DEFERRED**, **D-3 tracked**. This round executes them; it is
not an audit and raised no new lens.

**Exit state:** CI gate fixed at source with the baseline untouched, D-1 implemented and measured,
the KI-028 macOS residual closed at its real cause, one carried finding CONFIRMED and fixed after
two probe attempts wrongly refuted it, and ER-CI-04's gcc-16 measurement finally taken.

### What was fixed

| ID | What | Evidence |
|---|---|---|
| **CI gate** | Two `-Wunused-lambda-capture` sites in Test 48: both lambdas captured `block`, a constant expression that is never odr-used, so the captures were dead | Reproduced locally with the pinned **clang-22** on the real compile command (same two sites, same flag), fixed by deleting the captures, re-run: **0 warnings**. Baseline unchanged; nothing suppressed or excluded |
| **D-1 / KI-027** | Latency delivery from the audio thread — `setLatencySamples` takes ≥3 CriticalSections and, on a real change, allocates and `write()`s in the wrapper | State test 22. Message-thread change stays **immediate** (0→4); an off-thread change is **deferred** (stays 0) and the 20 Hz timer delivers the **correct** value 4 after **10 ms**. Reverting the routing reproduces the defect: latency 4 delivered on the automation thread |
| **KI-028 macOS residual** | The residual was never the sweep — Option B's hook reaches the value box on every platform. It was the **trigger** | Pinned JUCE 9.0.1 read, not recalled: macOS `getNativeRealtimeModifiers` (`juce_NSViewComponentPeer_mac.mm:302-307`) refreshes only the **keyboard** flags and returns cached mouse buttons. The fix calls `+[NSEvent pressedMouseButtons]` — the API JUCE itself uses 1500 lines below (`:1867`) but never wires in. State test 23 |
| **Stale latency after a malformed restore** (carried bug 1) | `replaceState` adopts a poisoned value by CLAMPING it to a range endpoint and re-reports a latency for it; `reassertParameters` then repairs it with `setValue()` + an atomic store, notifying nobody. The host keeps the poisoned number | State test 24. Measured: reported **4**, restored state predicts **0** |
| **ER-CI-04** | gcc-16 measurement, open since round 3 | Run on g++-16 16.0.1 (trunk r16-8100): `-flto` **0 hits** (the seeded REAL mismatch still unreported), no-LTO **6 hits** (4 on 13.3/15.2) with `AllocationGuard.h:350:69` still collapsing the false positive and the real one. Exclusion KEPT; no baseline widened |

### The carried latency bug was refuted twice before it was confirmed

This is the round's methodological result and it is the round-2/round-3 lesson recurring in a new
place, so it is recorded rather than smoothed over.

- **Attempt 1** poisoned `algorithm`. Refuted — but vacuously: the poisoned value and the repaired
  default both yield latency 0, so the probe compared 0 with 0.
- **Attempt 2** poisoned `drive`, whose maximum genuinely differs. Still refuted, and this time with
  a working non-vacuity control (a drive-at-max instance reports 4). The measurement was sound; the
  conclusion was still wrong.
- **Attempt 3** added a SECOND restore, and the defect appeared immediately: reported 4 against a
  predicted 0. The first restore is correct **by accident** — the live `InternalState` holds an int
  where the round-tripped blob holds a string, so `ValueTree::setProperty` sees a difference, fires
  `onOversampleChanged`, and recomputes the latency after the repair. That is the same var-type
  coincidence recorded for ER-STATE-07 in round 2. Settle the types and the coincidence is gone.

The standing rule this adds to the programme: **a probe that exercises a path only once cannot see a
defect that a first pass masks.** State test 24 therefore restores twice on purpose, and says so.

### D-1 — what the design does and does not buy

On the message thread nothing is deferred, so every UI edit, preset load and undo re-reports latency
instantly; State test 22's control leg asserts exactly that, because a regression to
"always deferred" would otherwise pass the main assertion silently. Off the message thread the audio
thread performs one relaxed atomic store and returns, and a **processor-owned** 20 Hz timer delivers
— processor-owned because the refuted editor-polling candidate does not exist when the editor is
closed. The cost, documented rather than hidden: the host can learn of a latency change **up to one
timer interval (50 ms) later** than the parameter moved.

Scope, stated plainly: the test proves DEFERRAL, which is the property this change is responsible
for. It does not prove the absence of a lock inside JUCE's own notification chain — that is what the
RTSan lane and the allocation guard cover, from the other side.

### KI-028 — the residual was in the trigger, not the design

Round 3 implemented the hook and recorded that both candidate designs shared a predicate KI-013
makes inert on macOS. That remained true, so implementing Option B again would have changed nothing:
the fix had to be the predicate. `anamorph::gui::anyPhysicalMouseButtonDown()` forwards to JUCE
everywhere JUCE is already authoritative (X11 queries the pointer, Windows calls
`GetAsyncKeyState`) and calls AppKit on macOS. It feeds **both** editor predicates — the sweep gate
and the press-glow — because closing the gesture while the knob still looked pressed would be a
worse mixed state than the defect. That also closes KI-013's glow half as a direct consequence.

**Where the macOS half is actually verified, and where it is not.** State test 23's discriminating
assertion is `#if JUCE_MAC`, and the macOS CI job runs this suite (`scripts/run-tests.sh`), so that
is the evidence. It cannot be verified on the Linux box that wrote it, and the test says so in
place: off macOS, JUCE installs its realtime hook from a live `ComponentPeer`
(`juce_Windowing_linux.cpp:67`), this suite is headless and has none, so the query falls back to the
cache and cannot discriminate. That leg asserts the forwarding identity instead. **The first version
of this test asserted the disagreement unconditionally and failed on Linux** — a platform-divergent
test that would have turned the Linux job red.

### Decisions recorded, not implemented

- **D-2 (RISK-007) — DEFERRED by the maintainer. No code changed.** The measured TSan findings stand
  and remain valid: the `abActive` race, the `abUndo` vector races and the `juce::String` refcount
  race. No mutex, no `callAsync`, no state-architecture change. Exposure is host-determined, which is
  what the deferral turns on.
- **D-3 — release blocker, tracked only.** The Level-5 audition must be a human DAW session against
  the FINAL shipping build. The 2026-08-15 audition covered v0.9.4 and is invalid: ADR-0031/0032
  changed the x86-64 machine code everywhere, and 0.9.6 changed audible behaviour in the defective
  windows.
- **ER-STATE-04.5 — not reopened.** Refuted in round 3 (the `id`+`raw`-without-`value` shape does not
  occur); no new evidence appeared.
- **Cross-file lint boundary — unchanged.** Round 3 measured it (83 forbidden-class matches, every
  cross-file DSP one inside `prepare()`) and documented it in the script. Not redesigned.

### D-4 — approved, and the round-3 implementation verified against the approval

The approval clears the gate round 3 flagged. Re-verified by measurement rather than assumed: every
malformed shape now restores the parameter DEFAULT on both paths (`"abc"`, `""`, `"0x10"`, `"nan"`,
`"inf"`, `"-inf"`, `"1e39"`, `"1e400"`, `"-1e400"` — all nine, on width, monoMakerFreq, chorusRate,
algorithm and monoMakerOn), and a repair reaches the parameter, the DSP atomic **and** the
serialized tree. The compatibility rules the approval preserves all hold: no schema field added,
removed or renamed, and no well-formed file loads differently.

---

## Round 3 — 2026-09-01 — closing the residuals: measurement before remediation

**Entry state:** rounds 1 and 2 CLOSED. Baseline `main @ e8f4422` → v0.9.6 on
`claude/anamorph-ci-workflow-8iu7yk`; DSP **46 tests / 242 checks**, state **18 tests / 941
checks**, `preflight.sh` exit 0, citation self-test green, realtime lint green, pinned clang-22
gate reporting no new first-party warnings, GCC baseline unchanged. Three maintainer decisions
carried (D-1, D-2, D-3), all evidence-complete and none implementable without a decision.

**This round is not an audit.** It has a fixed priority order and a fixed discipline, both set by
the maintainer: close remaining measured correctness risks, resolve deferred hypotheses, complete
selected engineering gates, and keep this worklog matching executable reality.

**The discipline, restated because rounds 1 and 2 each violated it once.** Reading code and
inferring a mechanism is NOT a confirmed defect. ER-STATE-01 (round 1) and ER-STATE-07 (round 2)
were both derived from careful reading of a real code path and both turned out not to occur —
`apvts.replaceState` was already doing the thing they claimed was missing. Round 3 therefore
classifies nothing as CONFIRMED without reproduction, a failing probe, or a measured number, and
every investigation below states in advance what its probe prints if the mechanism is real and
what it prints if the mechanism is absent, so that a wrong prediction is itself a usable result.

**Method for this round.** Eight parallel investigation lenses, one per prioritised item, each
required to produce an executable probe rather than a conclusion; then an adversarial pass over
each, briefed to attack the citations, the mechanism and the probe's vacuity; then the probes are
compiled and RUN, and the classification comes from what they print — not from what the
investigation predicted.

**Outcome: 5 confirmed and fixed, 2 refuted, 2 boundaries measured.** Every probe below was written
by the round, compiled, and run; three of them had to be rewritten after their first run because
they were VACUOUS, and those rewrites are recorded rather than tidied away — they are the round's
most transferable result.

### What was fixed

| ID | What | Evidence that it was real |
|---|---|---|
| **KI-028 / ER-GUI-04** (P1) | **A value-box press whose release is never delivered holds a host gesture open**, and `pollUndoCoalesce` commits nothing while `openGestures > 0` | State test 21, with a POSITIVE CONTROL first (a deliberately unclosed gesture → `canUndo = 0`, proving the yardstick can fail). Un-released press → `canUndo = 0`; after the reconcile → `1`. Disabling the fix reproduces 2 failures |
| **ER-DEP-06 residual** (P2) | **An unparsable value pins the control to its range MINIMUM** — `"abc"`, `""`, `"0x10"`, on BOTH the preset and session paths | Measured per parameter through the real loaders: width normalised **0.000000** against a 0.500000 default, on every parameter type. State test 19 |
| **ER-STATE-09** (P4, new) | **An infinity pins the control to a range ENDPOINT**, defeating the round-2 non-finite guard entirely | `inf` / `1e39` / `1e400` → **1.000000** (maximum); `-inf` / `-1e400` → **0.000000** (minimum); `nan` → **1.000000** on the SKEWED `monoMakerFreq`. State test 19 |
| **ER-STATE-10** (P3, new) | **A repaired parameter never reached the saved state**: the restore repaired the live value and then wrote the corruption straight back out | Measured: after restoring `value="nan"`, the live APVTS node still read `"nan"` and the re-saved blob read `value="nan" raw="0.5"`. State test 20 |
| **ER-DSP-08** (P8, new) | **A duck requested while inactive fires on activation**, collapsing the stereo image for 32 ms of the first audio | Measured against a control engine driven through the identical sequence: side-energy ratio **0.002780** at block 5, **24 blocks (32.0 ms)** off level. Test 48 |

### P2 + P4 are one defect, and the round-2 guard was in the wrong place

Round 2 added a `std::isfinite` check to both restore paths and both these findings walk straight
past it, for the same reason: **the check ran on the CONVERTED value.** `convertTo0to1` clamps, and
so does the `juce::jlimit` on the session path's `raw` branch, so an infinity arrives at the
finiteness test already laundered into a perfectly finite range endpoint. The guard could only ever
catch the one case that survives conversion as NaN — which is why it looked like it worked, and why
it silently failed on `monoMakerFreq`, whose skew turns NaN into 1.0.

The fix moves the test to the INPUT and puts it in one place: `src/SerializedNumber.h`, in the
spirit of `AbSlotIndex.h` — one restore invariant, dependency-free core, guarded directly (26 cases,
run standalone before it was wired in). Both paths call the same predicate, so a malformed value
cannot mean one thing in a preset and another in a session, which is exactly how they drifted apart
before. The text rule is deliberately stricter than any general parser: it accepts the plain decimal
shape JUCE's writer emits and rejects everything else, which is what makes `"0x10"` a rejection
rather than 16 — a strtod-family parser would accept hex floats, `inf` and `nan` as legitimate
numbers, and those are precisely the inputs the guard exists to refuse.

### Three probes were vacuous on their first run — the round's most transferable result

1. **P8 measured 1.000000 and looked REFUTED.** A forced duck is dry-**filled**, not silenced, so on
   a transparent chain the fill IS the output and a level probe cannot see the duck at all. Rewritten
   with the widener engaged and measuring SIDE energy, it showed the collapse immediately.
2. **The rewrite was still vacuous:** a 1 kHz tone through the default 12 ms Haas delay is exactly
   12 periods, so the channels re-align and the side energy is identically zero — a numerology
   accident. Deterministic noise has no such coincidence at any delay, and the defect appeared.
3. **The KI-028 probe had no positive control.** "No leak" and "the yardstick is blind" print the
   same thing. Adding a deliberately unclosed gesture first is what made every later leg mean
   something — and it is what let the round REFUTE the adversarial claim below rather than accept it.

### Refuted

- **ER-STATE-04.5 (P5) — REFUTED.** The `id`+`raw`-without-`value` shape does not occur. Measured
  both arms (the omitted parameter at its default and off it) and both save
  `@value="1.0" @raw="0.5"`. Round 2's recorded partial answer — that the shape appears when the
  parameter was already at its default — was wrong, and so was this round's own prediction: the
  closing `flushParameterValuesToValueTree()` in `updateParameterConnectionsToChildTrees` writes for
  every adapter regardless. **No gate item and no decision remain.**
- **"Editor teardown also leaks the gesture" — REFUTED.** The adversarial pass argued from
  `PluginEditor.h` member order (Knobs at :432-435, `sliderAtts` at :482, so `~sliderAtts` runs
  first and removes the attachment listener before `~ValueBox` fires `sendDragEnd`) that closing the
  editor mid-drag leaks `openGestures` into the processor, which outlives it. The order is real; the
  consequence is not. Measured: `canUndo = 1` after destroying the editor mid-press. The RAII close
  still lands. Guarded as leg (5) of State test 21 so the refutation cannot quietly rot.

### Boundaries measured rather than argued

- **ER-CI-04 (P6).** The `-Wmismatched-new-delete` exclusion's two empirical legs **reproduce
  unchanged from gcc-13.3.0 through gcc-15.2.0**: under `-flto` the flag emits nothing — not even
  for a genuine `std::free`-on-`new[]` mismatch seeded in, so it still cannot fail in the lane that
  reads the log — and without LTO the false positive and that seeded real mismatch are both
  attributed to `AllocationGuard.h:350:69`, so a per-file baseline would still mask a real bug.
  **gcc-16 itself remains UNMEASURED and is recorded as such**: it is PPA-only here and installing a
  toolchain to measure it is the maintainer's call. The exact container command is now in the
  script, with the result that would retire the exclusion. **Exclusion KEPT; no baseline widened.**
- **ER-RT-05 (P7, new).** The realtime lint's same-file boundary is real. It is also currently
  EMPTY, and that is now a number rather than a claim: of 83 FORBIDDEN-class matches across `src/`,
  every one in a DSP translation unit the audio thread reaches cross-file — VelvetNoise (3),
  ChorusEngine (2), HaasProcessor (2) — is `container growth` inside that module's own `prepare()`,
  where allocation is required by policy and which is not audio-thread code. The other 76 are in the
  wrapper, preset manager and GUI, which the audio thread never enters. **Documented, not parsed:**
  the cost of a cross-translation-unit walk is a real parser and the measured benefit is zero. The
  docstring now records the census and the shape that would make the gap non-empty.

### Decision required before merge

**D-4 — the P2/P3/P4 fixes change malformed-value recovery and saved-state contents.** The brief is
explicit that any such change needs `ARCHITECTURE_REVIEW_GATE` approval **before merge**, so they are
implemented on the branch and flagged here rather than treated as ordinary fixes. What changes: a
malformed or non-finite serialized value now restores the parameter DEFAULT instead of a range
endpoint, and a repaired value is written into the live tree so the next save carries it. No schema
field is added, removed or renamed; no well-formed file loads differently; the serialized shape is
untouched. `SERIALIZATION_REGISTRY.md` already specifies "per-parameter defaults" for the absent
case, and this makes the malformed case agree with it. **Not merged pending sign-off.**


---

## Round 2 — 2026-08-31 — CI recovery, the activation defect, and two confirmed silences

**Entry state:** round 1 merged to the branch; CI **red** on two warning gates; five carried
roadmap items; three maintainer decisions open. **Exit state:** CI gates fixed at source, **five**
confirmed defects fixed with regression coverage, RISK-007 measured, D-1 materially corrected,
one new issue filed, and one round-1 finding refuted by measurement — with every document that
repeated its premise corrected.

### What was fixed

| ID | What | Evidence that it was real |
|---|---|---|
| CI gates | `-Wshadow-uncaptured-local` (Clang) / `-Wshadow` (GCC) from round 1's `adoptIfAnamorph` lambda shadowing `xml`; four `-Wfloat-equal` from bare `!=` on floats in Tests 43/44/46 | The gate output itself. Fixed at source, **no baseline widened**: the lambda parameter renamed, the comparisons moved to `juce::exactlyEqual` (JUCE's helper for deliberate exact comparison, already the idiom in `state_tests.cpp`). Re-verified with a pinned clang-22 rebuild (no NEW warnings, 17 accepted sites) and a GCC rebuild (1 shadow site in `PluginProcessor.cpp`, the pre-existing baselined one, down from 2) |
| **ER-DSP-06** (new) | **Every activation ducked the audio to near-silence for ~35 ms**, and a restored session additionally opened at the wrong level for ~20 ms | Measured through the real wrapper, before and after. Before: min block RMS / settled = **0.0014** (fresh instance) and **0.0011** (restored), first block **2.4×** too loud. After: 0.982 / 0.983 / 1.000. State test 16 |
| **ER-STATE-03** | **A `value="nan"` in a session or preset silenced the plug-in permanently**, and round-tripped through save | Measured: output peak **0.000000** over 8 blocks before the fix, 0.699720 after. State test 17, which also drives the preset path through a real poisoned file |
| **ER-STATE-08** | **A v0.2 session restored into a reused instance kept the previous project's host-hidden Settings** — all six of them | State test 4's InternalState assertion was vacuous; made non-vacuous (Oversampling 4x, non-default UI Scale set first) it fails **twice** before the fix and passes after |
| **ER-STATE-06** | **A preset entry with no saved value set that control to its range MINIMUM**, not its default — a silent mono collapse for Width | Measured through the real `loadFile` on a real file: width normalised **0.000** before, 0.500 (the default) after. State test 18 |
| **ER-DSP-07** | **`reset()` never cleared `pendingForced`**, so a forced duck in flight at a host re-prepare latched it true — and the Level-Match consumer at the end of `process()` runs only `if (! pendingForced)` | Test 47 injects −6 dB after a re-prepare mid-forced-duck: **0.000 dB** adopted before the fix (silently dropped), −5.868 dB after |
| ER-STATE-04, ER-GUI-02, ER-CI-02/03/04/05/06, ER-DOC-04 | Eight verified comment/diagnostic corrections | Each checked against the pinned JUCE or the actual workflow before editing; see below |

### ER-DSP-06 — the root cause was an ordering contract, not the reported symptom

The review item said `snapSmoothers()` "may capture stale engine defaults". That is the symptom.
`AnamorphEngine::prepare()` settles the **whole** engine from its own snapshot `p` — it reads
`p.bypass` and `p.mbEnable` directly, then runs `updateDerived()` and `snapSmoothers()` from it —
so prepare()'s contract is *"`p` is already what the host wants"*. `prepareToPlay` called
`prepare()` first and pushed the parameters in afterwards, breaking that contract on every
activation.

The consequence was **universal, not restricted to restored sessions**, and this is the part the
report did not contain: the engine's struct defaults and the snapshot the wrapper builds disagree
on a discrete field even for a brand-new instance. `dimMode` is the always-active one — the APVTS
choice defaults to index 1 and `toEngine` maps choice→mode as `index + 1`, so the first snapshot
says 2 while `EngineParameters::dimMode` is 1. (Advanced sessions add `mbEnable`: APVTS `true`,
struct `false`. The rest of the Advanced block is gated off in Simple mode and keeps the struct
defaults by design — `toEngine`'s `if (advanced)`.) A discrete difference is exactly what the
click-free switch machine reacts to, so **every** activation got the ~6 ms fade to silence +
~28 ms fade back in that a real settings change deserves.

Fix: `AnamorphEngine::primeParameters()` — adopt a snapshot wholesale, no duck, no ramp — called
from `prepareToPlay` before `prepare()`. Valid precisely because nothing is audible yet, and
documented as **not** a substitute for `setParameters` once audio flows. Two false starts are
recorded here because they cost time and would cost it again: an assertion that `Mix=0` must be a
bit-exact null through the processor is wrong when the multiband allpasses are engaged (the
phase-matched dry is not the input), and a level assertion on an engaged Dim-D session measures
the algorithm's delay lines filling from empty, which is correct behaviour, not a duck.

### ER-STATE-03 — round 1's mechanism was half wrong

- **REFUTED:** `raw="nan"` never reaches the parameter. `reassertParameters`' write gate
  `|norm - current| > 1e-6` is **false** when either side is NaN, so the raw branch is dead on NaN
  — it neither injected the value nor repaired it.
- **CONFIRMED, different ingress:** JUCE's own `apvts.replaceState()` →
  `updateParameterConnectionsToChildTrees` → `setDenormalisedValue` → `setValueNotifyingHost`
  reads `@value`, and its `approximatelyEqual` guard is likewise false for NaN. Second, fully
  independent ingress: `PresetManager::applySoundTree`, which had no gate at all.
- **Impact is not cosmetic:** a NaN continuous parameter latches its smoother target, every output
  sample goes non-finite, and ADR-0009's *sample-level* self-heal then zeroes the block and resets
  the engine on every block — permanent silence with plausible-looking controls, persisted by
  `getStateInformation` writing `nan` straight back out.
- **Fix:** two guards, because the families are disjoint. `reassertParameters` substitutes the
  parameter default for a non-finite value **and** its gate becomes a negated `<=`, so a NaN on
  either side counts as "differs" and is repaired rather than skipped — that inversion is what
  makes it a repair of `replaceState`'s damage rather than a filter. `applySoundTree` takes the
  fallback it already uses for an absent child.

### R2-2 — RISK-007 is now measured

`AnamorphStateTests --state-thread-probe` (committed; never run by the suite, because if the risk
is real then running it *is* the undefined behaviour) drives host `setState`/`getState` from one
thread against the editor tick's reads on the main thread. Under ThreadSanitizer it reports **four
data races**, on exactly the members round 1 reasoned about: `abActive` (write in
`setStateInformation` vs read in `canUndo()`), the `abUndo` vector's internals twice
(`UndoStacks::operator=` vs the main thread's `empty()`), and a `juce::String` reference-count
exchange against a `String` copy. **The code half of D-2 is settled**; what remains is the host
question (VST3 forbids it; the macOS AU does not).

### D-1 corrected — two of its candidate fixes are refuted

Re-verification narrowed KI-027 on three axes and broke two options:

- **Reachability is lower than filed.** Oversampling is **not a host parameter** — it lives in
  `InternalState` (`int_oversample`, default "Off") and no automation lane can move it. At factory
  defaults `predictLatency` is identically 0, so the expensive branch is unreachable until the user
  has selected 2x/4x/8x by hand.
- **Rate is bounded:** VST3 delivers at most one listener dispatch per parameter per block.
- **The inversion is milder on POSIX:** `juce::CriticalSection` enables `PTHREAD_PRIO_INHERIT` on
  Linux/macOS; Windows has none.
- **REFUTED — the editor's 24 Hz poll.** It is the only message-thread tick in `src/`
  (`PluginEditor.cpp` `startTimerHz (24)`, verified by grep) and does not exist with the editor
  closed, so a closed-editor render would never learn about a latency change at all — a worse,
  user-visible defect than the one being fixed.
- **REFUTED — an `AsyncUpdater`.** Its trigger reproduces the same `postMessage` (lock + possible
  reallocation + `write()`) on the audio thread; it removes only the inversion.
- **Surviving design, and what D-1 now asks for:** keep the synchronous call when
  `juce::MessageManager::existsAndIsCurrentThread()`, otherwise set one relaxed atomic flag
  consumed by a processor-owned ~20 Hz `juce::Timer`.

### New finding, filed not fixed

**KI-028 (ER-GUI-04)** — round 1's own value-box gesture fix leaks an open host gesture when the
mouse release is lost. The editor's release-outside reconcile clears the visual `dragging` flag but
cannot reach the `ScopedDragNotification`: `ValueBox` lives in an unnamed namespace inside
`LookAndFeel.cpp`. While the gesture is open `pollUndoCoalesce` commits **no** undo step. Strictly
better than the pre-0.9.6 state (no gesture at all, so no undo step ever), and the two candidate
designs are a decision, so round 3 picks one.

### Verified corrections (checked before editing, none behavioural)

- **ER-STATE-04 — CONFIRMED and worse than filed.** The comment claimed `replaceState` "swaps only
  the tree". In the pinned JUCE it propagates to the parameters, the DSP atomic, the editor's
  attachments **and** the host. The comment now states the real residual `reassertParameters`
  exists for (absent PARAM nodes; exact `raw` vs snapped `value`), so the function's necessity
  survives the correction instead of being undermined by it. Its "trade-off" clause was wrong too:
  an open editor *does* track a host restore.
- **ER-GUI-02 — CONFIRMED on reachability, but the published docs were already right.** The
  over-claim was one clause of one code comment (`cancelInlineTextEdits` has a single call site,
  behind three gates, inert on X11). Wording narrowed; the code deliberately **not** widened — a
  general "leaving the application never writes a half-typed value" guarantee is not reachable at
  that layer.
- **ER-CI-02 — worse than filed.** `build.yml`'s header still described the pre-2026-08-15 macOS
  ordering and contradicted both its own in-job comment and `CI_CD.md`. Rewritten line-count-neutral
  (2→2, 7→7) so no citation moved. Windows is now the only platform validating pre-staging.
- **ER-CI-03 / ER-CI-06** — `codeql.yml` builds with the runner's distribution g++ while claiming to
  match the Linux job (pinned Clang since ADR-0030), and its header over-stated coverage as
  "src/ + tests/" when only `tests/dsp_tests.cpp` is compiled. Comments corrected; the compiler is
  deliberately not pinned (CodeQL's alert set comes from its extractor, and pinning would be a
  Build System change for no analysis benefit).
- **ER-CI-04** — `check-gcc-warnings.py`'s exclusion label still called gcc-13.3.0 "this job's
  pinned pair" after the move to the floating `gcc:16` container. The exclusion still stands on its
  structural leg; the empirical leg is now scoped to the compiler it was measured on, with
  re-measurement a round-3 item. `GATED_FLAGS` unchanged.
- **ER-CI-05** — `release.yml` reported a transient tag fetch failure as "not an annotated tag",
  telling the maintainer to re-create a tag that was almost certainly fine. Infrastructure failure
  and verdict now say different things; both still exit 1.
- **Also corrected:** round 1's own batching rationale for the CI items ("build.yml line shifts
  re-anchor many citations") was over-cautious — only `build.yml` is citation-tracked of the four
  files, and its correction was written line-count-neutral.

### Adversarial sweep of the state-restore surface — three findings, and a correction to round 1

The sweep concentrated where round 1 and round 2 had both been editing (session/preset restore),
on the principle that our own recent changes are the least-reviewed code in the tree.

**ER-STATE-08 — CONFIRMED, FIXED.** A v0.2 bare-APVTS session restored into a **reused** instance
left all six host-hidden Settings (Oversampling, UI Scale, Scope Persistence, Show Meters,
Tooltips, UI Animations) at the *previous project's* values. The `AnamorphRoot` branch of
`setStateInformation` migrates them (`internal.migrateFromLegacyApvts`) for every pre-0.8.4
session; the v0.2 branch one `else if` below never touched `internal` at all — and a v0.2 session
is older still, so it is the same vintage the migration exists for. Fixed with the same call.
Confirmed by measurement, not reading: State test 4's InternalState assertion was **vacuous** (it
checked a value nobody had moved), so it was made non-vacuous first — Oversampling set to 4x and
UI Scale to a non-default — and it then failed twice before the fix and passes after. Filed as
ER-TST-05 as well, since the vacuity is a test defect in its own right (same class as round 1's
ER-TST-01).

**ER-STATE-06 — CONFIRMED, FIXED.** `PresetManager::applySoundTree` asked `child.isValid()` when
the question is whether the file carries a *value*. A `<PARAM id="width"/>` that lost its value to
a truncated write read back as `var()` → `(double) 0.0` → `convertTo0to1(0.0)`, i.e. the range
**minimum**. Width runs 0–2 with default 1.0, so the observed result is a silent full mono
collapse. Measured: width normalised **0.000** before the fix, 0.500 (the default) after; State
test 18 drives it through the real `loadFile` on a real file. The gate is now
`child.hasProperty("value")`. No compatibility cost — the writer is
`apvts.copyState().createXml()`, which always emits `value`. Note the asymmetry this closes: the
*session* path never had the bug, because JUCE's own `setNewState` reads
`getProperty("value", denormalisedDefault)`.

**ER-STATE-07 — REFUTED by measurement, and it took round 1's ER-STATE-01 with it.** The
hypothesis: `reassertParameters`' absent-node default branch applies values with `setValue()` plus
a direct atomic store, neither of which notifies — so a restore that moves Drive or Algorithm
through that branch would leave the host holding a stale reported latency (a hard-stop category,
hence filed as gated rather than fixed). Step 0 of the new `--latency-restore-probe` confirms the
*mechanism*: a bare `setValue(0)` on Drive leaves the reported latency at 4 samples. But the full
restore reports correctly, in both a first restore and a second one. Step 0b isolates why:
`apvts.replaceState()` **on its own**, with `reassertParameters` nowhere near it, takes Drive from
0.600 to its default and the latency from 4 to 0. The route is inside APVTS —
`updateParameterConnectionsToChildTrees` clears every adapter's tree, re-points those the new
state carries, then *appends* an empty `PARAM` node for each adapter left over; that `appendChild`
fires the APVTS's own `valueTreeChildAdded` → `setNewState` →
`setDenormalisedValue(getProperty("value", denormalisedDefault))` → `setValueNotifyingHost`.
Absent nodes are therefore reset to their defaults **with** full notification, which is exactly
what ER-STATE-01 claimed did not happen.

Consequences, all applied this round: ER-STATE-07 is closed as refuted (no gate item, no decision
needed); round 1's default branch is **kept but re-described** as a redundant idempotent backstop
in both code comments; `SERIALIZATION_REGISTRY.md`'s row now attributes the rule to
`replaceState`; the `[0.9.6]` CHANGELOG entry that claimed a user-visible parameter fix is
replaced by the one real instance of the leak class we actually fixed (ER-STATE-08's Settings); and
State test 4's assertion is left standing, with its comment corrected — it pins the contract
regardless of which layer satisfies it. Round 1's severity call was wrong in a specific way worth
remembering: it reasoned from `reassertParameters` alone and never ran `replaceState` in isolation.

### Round-2 investigation workflow — reconciled after the round closed

The parallel investigation sweep launched at the start of round 2 returned after the round's
manual work was already committed. Reconciling it against what shipped: it **independently
reproduced** ER-STATE-06, ER-STATE-07 and ER-STATE-08 with the same dispositions, and its
ER-DSP-06R reaches the same root cause this round did — the ordering contract in `prepareToPlay`,
not `snapSmoothers` "capturing stale defaults" — which is worth recording because two independent
derivations agreeing is the strongest evidence this round produced. Three items were genuinely new:

**ER-DSP-07 — raised as `likely`, now CONFIRMED by measurement, FIXED.** `AnamorphEngine::reset()`
flushes the duck state group — it adopts `pendingP`, then clears `pendingAlgoReset`, `switchState`,
`switchPhase`, `dryDuck` and `dryDuckLat` — but missed `pendingForced`, the sixth member. A FORCED
duck (A/B, preset, undo — `requestDuck`) still fading when the host re-prepares therefore left the
flag latched true underneath a `Normal` switchState. The sweep called the consequence "a masked,
inaudible extra reset at the next duck bottom"; the sharper one is at the END of `process()`, where
the defensive Level-Match consumer runs only `if (! pendingForced)` — so an injected trim is never
adopted at all. Measured, not argued: Test 47 injects −6 dB after a re-prepare mid-forced-duck and
reads the displayed match gain back. Before the fix **0.000 dB** (dropped); after, −5.868 dB
(adopted, then drifting — the injection is a seed, not a freeze, as Test 37 already documents,
which is why the assertion tests adoption rather than exactness). One line in `reset()`.

One correction to that finding as filed: it predicted the window would **broaden** under the
ER-DSP-06 reorder, because "a duckRequest pending at prepareToPlay would be consumed by the new
pre-prepare `setParameters` and then leak past reset()". That does not apply to what shipped. The
sweep was reasoning about the candidate fix it proposed — swapping the two statements — whereas the
implemented fix uses `primeParameters`, which assigns `p`/`pendingP` directly and never touches
`duckRequest`. A request posted before `prepareToPlay` is still consumed by the POST-prepare
`setParameters`, which begins a fresh, coherent forced duck. The underlying omission in `reset()`
was real on its own and predates both.

**ER-DOC-04 — CONFIRMED, corrected.** `PluginParameters.cpp`'s closing comment asserted that
"EngineParameters' member initialisers already hold those neutral defaults". True for the
advanced-gated fields it sits under, silently untrue for `dimMode`, which is assigned OUTSIDE the
gate and whose snapshot default (choice index 1 + 1 = 2) disagrees with the struct's 1. That is the
disagreement that made ER-DSP-06's duck universal rather than restore-only, and it was documented
nowhere. The comment now scopes its claim and names both disagreements (`dimMode` ungated and
consequential, `mbEnable` gated and therefore intended), with the explicit note that neither is
fixed by changing a default — the two sets of values mean different things.

**ER-RT-04 — informational, resolved as a side effect.** On first activation the host was told
`predictLatency(restored state)` while the engine still ran the default `osEngaged` latch, so
reported PDC and actual engine latency disagreed for the duration of the spurious duck. The
ER-DSP-06 fix closes the window that carried it; recorded so a later round does not re-raise it as
new. No separate action, and no reported-latency change: the fix removes a transient disagreement
rather than altering what is reported.

### Validation at the end of round 2

`preflight.sh` exit 0. DSP suite **46 tests / 242 checks** (Test 47, ER-DSP-07); state suite **18 tests / 941 checks**
(924 → 941: State tests 16, 17 and 18, plus the two de-vacuumed InternalState assertions in State
test 4). Two opt-in probes beside the suite, neither run by default: `--state-thread-probe`
(RISK-007, verdict from the sanitizer) and `--latency-restore-probe` (ER-STATE-07, measures and
reports rather than asserting — the reported latency is a hard-stop category, so the probe must
not encode any expectation). Citation self-test **145 cases**, gate green against all three
bases. `check-realtime` 93 self-test cases + clean scan; `check-gcc-warnings` self-test 17;
`check-docs`, `check-portability`, `check-linux-abi`, `setup-llvm-apt` all green. Pinned clang-22
warning gate: no NEW first-party warnings.

### Round-3 roadmap (revised by what round 2 learned)

1. **KI-028** — pick one of the two designs for the leaked value-box gesture and implement it.
   Highest-priority code item: it degrades undo, and it is a residual of our own fix.
2. **D-1 implementation**, if the maintainer approves the surviving design.
3. **R2-6 / twin-dump transition scenarios** — unchanged from round 1, still on request only.
4. **ER-CI-04 re-measurement** under `gcc:16`, to put the exclusion's empirical leg back on the
   compiler the lane actually runs.
5. **ER-STATE-04.5 (informational, mechanism now measured)** — after a restore that omits a PARAM
   node, the live tree keeps the node APVTS appended for it. Sharpened by the ER-STATE-07 probe:
   the node gets a `value` whenever the parameter actually moved (the flush follows
   `setValueNotifyingHost`), so this is confined to the case where the absent parameter was
   *already* at its default — `setDenormalisedValue` early-returns, `needsUpdate` stays false, and
   the next save persists `id` + `raw` only. Not a defect today (the `raw` path restores it);
   worth deciding deliberately.
6. Deferred, unchanged: ER-DSP-05 (chorus LFO phase beyond the tested envelope), ER-DEP-06 (silent
   preset-load failure UX — maintainer-owned copy).

### Checklist (round 2)

- [x] CI warning gates fixed at source, no baseline widened
- [x] First-activation defect root-caused, fixed, regression-tested (proven to fail without the fix)
- [x] R2-1 NaN ingress: mechanism corrected, both ingresses guarded, regression-tested
- [x] R2-2 TSan: RISK-007 measured, instrument committed
- [x] D-1 re-evaluated; two candidates refuted; no implementation
- [x] ER-STATE-04 / ER-GUI-02 / ER-CI-02..06 verified then corrected
- [x] KI-028 filed with both candidate designs
- [x] Adversarial sweep of the restore surface: ER-STATE-06 + ER-STATE-08 fixed with regression
      coverage, ER-STATE-07 refuted by measurement
- [x] Round 1's ER-STATE-01 premise corrected everywhere it was asserted (code ×2, registry,
      CHANGELOG, state-test comment, this worklog)
- [x] Investigation-workflow results reconciled after the round closed: ER-DSP-07 confirmed by
      measurement and fixed, ER-DOC-04 corrected, ER-RT-04 recorded as resolved-by-side-effect,
      and its "broadened by the reorder" clause refuted against the shipped fix
- [x] Worklog + dashboard updated and committed
- [ ] D-1 decided (surviving design)
- [ ] D-2 decided — the code half is now measured
- [ ] D-3 Level-5 audition for the shipping build
- [ ] Round 3 executed

---

## Round 1 — 2026-08-31 — baseline + broad sweep

**Tree at start:** `main` @ `e8f4422` (post-PR #133: toolchain identity work). Branch
`claude/anamorph-ci-workflow-8iu7yk` restarted from it. **Baseline validation:** preflight
exit 0 in 23.4 s — 42 DSP tests / 226 checks, 920 state checks, all eight checker self-tests
green, citation gate green on all three bases, ABI floor within bounds (GLIBC_2.38 /
GLIBCXX_3.4.31 / CXXABI_1.3.9). Build tree: GCC 13.3.0 local (the pinned-Clang gates run in
CI only), CMake 3.28.3/Ninja, JUCE 9.0.1 at the pinned commit.

**Sweep shape:** 8 lenses + 1 validation-baseline agent; 33 raised findings; 19 significant
ones adversarially verified (26 verifier verdicts) → **17 confirmed, 2 refuted**; ~75 areas
ruled out as sound. The supply-chain lens re-ran as a dependency-robustness lens after a
tooling false-positive; it added 6 findings (1 medium).

### Confirmed findings and dispositions

| ID | Title (short) | Sev | Disposition (round 1) |
|---|---|---|---|
| ER-DSP-01 | `process()` trusts `maxBlock` absolutely — oversized host block overruns every scratch buffer (release heap overflow; JUCE says defend) | High→Med (host-contract-violating trigger) | **FIXED**: depth-1 chunk guard in `AnamorphEngine::process`; Test 43 pins safety + bit-exactness vs a conforming-slice twin |
| ER-DSP-02 | `prepare()` re-arms continuous smoothers from neutral — first ~5–20 ms after every prepareToPlay glides wrong (Mix-0 session opens wet) | Med | **FIXED**: `snapSmoothers()` at end of `prepare()` (post-`updateDerived`); Test 44 asserts bit-null from sample 0 |
| ER-DSP-04 | CorrelationMeter has no NaN/Inf guard (ADR-0009 bullet 3 implemented only in LevelMeters); bypass crossfade re-injects raw non-finite input past the self-heal; meter latches NaN until re-prepare | Med | **FIXED**: `sanitize()` of the six accumulators in `publish()`; Test 45 |
| ER-RT-01 | Host automation of Drive/Algorithm re-reports latency from the audio thread: ≥3 locks; on a real latency change heap append + `write()` in the Linux wrapper; plus a priority-inversion variant | High | **FILED as KI-027** — the fix is a threading-model change (Architecture Review Gate hard stop) → maintainer decision **D-1**. Code comment corrected; LATENCY_MODEL/THREAD_MODEL drift recorded in the entry |
| ER-RT-02 | Enforcement-scope hole (narrowed by verification): `setParameters`' own body (and `toEngine`) outside every tier for lock/blocking/IO classes — RTSan never sees them, the lint never seeded them, Test 38 counts allocations only | Med | **FIXED**: `check-realtime.py` seeds `setParameters`+`toEngine` (+3 self-test cases, 90→93); docstring + REALTIME_AUDIO_POLICY scoping corrected. Verifier proved `updateDerived`/`snapSmoothers` were already covered by the same-file closure — the original claim over-reached there |
| ER-RT-03 / ER-STATE-05 | get/setStateInformation mutate message-thread state unguarded; real exposure = macOS AU off-main autosave (VST3 annotates `[UI-thread]`; JUCE hosting/pluginval structurally cannot produce the window) | Med (hyp→confirmed-narrowed) | **FILED as RISK-007** + THREADING_POLICY §Host state calls (assumption documented). TSan two-thread harness = round-2 investigation; any guard is gate-item → **D-2** |
| ER-STATE-01 | PARAM nodes absent from a restored session keep the previous project's values on a REUSED live instance (policy rule 2 held only vacuously, on fresh instances) | Med | **~~FIXED~~ → PREMISE REFUTED in round 2 (ER-STATE-07 probe step 0b).** `apvts.replaceState` already resets absent nodes to their defaults, with host notification. The shipped default branch in `reassertParameters` is a redundant, idempotent backstop — kept, but it is not what makes rule 2 hold. Code comments, SERIALIZATION_REGISTRY and the CHANGELOG entry corrected in round 2; the state-suite assertion stands (it pins the contract, whoever satisfies it). The genuine instance of this leak class was in the **host-hidden Settings**, found and fixed in round 2 as ER-STATE-08 |
| ER-STATE-02 | Parsable-but-wrong-typed A/B slot payload re-types the live APVTS on apply → every later save silently loses all 36 parameters for a fresh instance | Med | **FIXED**: `readSlot` accepts only `apvts.state.getType()` (wrong type = unparsable = slot re-seeded); end-to-end state regression (restore → `abSwitchTo` → re-save → fresh-instance restore); registry sentence extended |
| ER-GUI-01 | Value-box vertical drag is a third gesture-less edit path — no Undo step, no host change gesture; KI-010 claimed the list complete | Low | **FIXED**: `ScopedDragNotification` held across the ValueBox press (knob-drag parity); KI-010 dated correction |
| ER-TST-01 | Tests 2 & 38 ran the whole algorithm×OS matrix with `algoAmount` at its 0 identity default — the engaged wet synthesis of all four algorithms outside both the NaN/denormal and allocation invariants; Dimension-D engaged by NO assertion-bearing test | High | **FIXED**: both matrices at `algoAmount 0.7`; Test 2 sweeps dimMode 1–4 for Dimension-D. Result: all green — the engaged paths were clean, now they are *proven* clean per push |
| ER-TST-02 | The twin-dump/ADR-0032-gate bit-identity claim is steady-state-scoped; blind axes (duck/adopt, crossfades, solo, xover glide, NaN-heal) named nowhere | Med | **DOCUMENTED**: TESTING.md §Gaps coverage-boundary entry + KI-026 scope qualifier. Matrix extension = round-2 candidate (not committed) |
| ER-TST-04 | channelMode/swapLR/inputBalance/polarity + chorusRate/chorusDepth/dimMode: zero behavioural coverage anywhere (chorus family zero even at module level) | Med | **FIXED**: Test 46 — conditioning semantics pinned on the transparent chain (incl. exact polarity sign-flip) + discrimination checks (rate, depth, all six dimMode pairs) |
| ER-CI-01 | `run-pluginval.ps1` retried genuine Win32-exception crashes 3× per pass — the masking removed from macOS 2026-08-18 survived on Windows; its null-exit justification was retired by the KI-007 WaitForExit fix | Med | **FIXED**: real abnormal exit fails immediately; retry only for `$null` (launch failure). TESTING.md §retry, RISK-004, and the sh-side comment re-synced |
| ER-DOC-01 | v0.9.5 renumbering incomplete: 7 documents still named v0.9.4 as the release in preparation / first tag; HANDOVER asserted the Level-5 audition "against the build that ships" for a superseded build; stale open-KI enumeration | High | **FIXED** as part of the 0.9.6 bump: all forward-looking claims renumbered; precondition 7 restated **OPEN** (→ **D-3**); KI enumeration completed (KI-018–023, KI-026) |
| ER-DOC-02 / ER-DEP-02 | `NOTICE` (shipped attribution asset) declared JUCE 9.0.0/f8f8864 while the product ships 9.0.1/e18f7f5 | Med | **FIXED** + `NOTICE` added to the DEPENDENCY_POLICY JUCE-bump re-verification checklist so the next bump cannot miss it |
| ER-DOC-03 | CI_CD.md job inventory omitted `macos-crossslice` and the release-blocking `windows-avx2-ab`; "seven non-packaging jobs" stale (nine); REPOSITORY_MAP same | Med | **FIXED**: two table rows, count, release-blocking carve-out for `macos-crossslice`, REPOSITORY_MAP row |
| ER-DEP-01 | `NOTICE` omitted the AudioUnitSDK Apache-2.0 attribution for the macOS AU while carrying SheenBidi's under the same licence | Med | **FIXED**: AudioUnitSDK section added (© 2000-2021 Apple Inc., from the pinned tree's LICENSE.txt); THIRD_PARTY_LICENSES mandatory-notices updated |
| ER-DEP-05 | One action pin (`build.yml` crossslice checkout) lacked the trailing version comment the Dependabot review convention depends on | Low | **FIXED**: `# v7.0.1` appended |

### Refuted findings (recorded so they are not re-raised without new evidence)

- **ER-DSP-03** — "ordinary discrete duck holds silence for the rest of the block (~host-buffer
  dropout at large buffers)". Mechanism real, but **documented product behaviour** since
  [0.8.10]: the CHANGELOG entry describes exactly this bottom-dwell-until-block-boundary shape.
  Not a defect; not unrecorded.
- **ER-TST-03** — "the state fuzzer can't reach the XML space through zlib framing". The framing
  premise is false: `copyXmlToBinary` in the pinned JUCE (and in 8.0.8, the earliest ever used)
  writes magic + length + **plain UTF-8 XML**, no deflate anywhere. The fuzzer's byte mutations
  reach the parser directly.

### Ruled out as sound (the negative-results map — abbreviated; full lists in the round data)

DSP: NaN/Inf self-heal detector+reset completeness; Nyquist clamp ordering in all three
consumers; Velvet gather ring-collision proof; SoloMonitor/Multiband cold-re-entry snap
ordering; latency latch vs ring sizing; long-session accumulator boundedness; parked-path warm
history; LR4 allpass discipline; n=0/1 edges. RT: ScopeBuffer SPSC contract; engine path
alloc/lock-free end-to-end; meter publication atomics; destruction order both sides; FTZ/DAZ
scope; gate liveness proofs non-gameable. State: 36/36 registration/restore completeness;
A/B index clamp; slot reset-first overlay; preset identity round-trips; load-failure
atomicity; ±inf clamped by NormalisableRange (only NaN survives → ER-STATE-03, round 2);
save→load→save byte-stability; legacy read paths. GUI: GL lifecycle per ADR-0011; repaint
idle-gate economics; DPI layer keying; SafePointer discipline; no mutable statics;
FrameClock internals. Tests: run-tests/preflight exit plumbing; Windows self-test step;
liveness proofs; fixture regime; sanitizer plumbing; no tautologies in ~130 assertion sites.
Build/CI: ccache correctness boundary; no silent gate-skips; tested-bytes==shipped-bytes on
all three platforms; warning-baseline mechanism; windows-avx2-ab gate internals; fail-closed
discovery everywhere; release.yml validation; secrets hygiene. Docs: count claims in
README/HANDOVER/TESTING accurate pre-round (now re-synced); strictness single-authority
discipline; packaging matches documented behaviour; licensing story consistent (sole
inconsistencies = ER-DOC-02/ER-DEP-01, fixed); ADR_INDEX complete. Deps: Actions all
SHA-pinned (one comment gap, fixed); JUCE pin end-to-end in CI; CI log hygiene;
THIRD_PARTY_LICENSES symbol-level method; preset path construction fail-closed; fuzz harness
covers the real entry point.

### Already-known encountered (not re-filed)

KI-001, KI-012/ADR-0015, RISK-002, the A7-9 platform terminal states, the H4 Class-B
level-match trade, KI-016/023/026, KI-003/004/007 host-coverage gaps, RISK-004 (Linux),
RISK-005, KI-015/RISK-006 licensing, pluginval unpinned (RH-F6), the v0.2 abSlot staleness
note (worklog §11), metadata-only AnamorphRoot adoption (deliberate, worklog §13), KI-009,
KI-010 (typed+wheel halves), KI-013, KI-017–KI-022, macos-intel thin-build scope, Windows
staging self-check scope, `gcc:16` floating major, Renovate rejection.

### Version and validation at end of round

Version bumped **0.9.5 → 0.9.6** (CHANGELOG `[0.9.6] — 2026-08-31`, six Fixed entries; the
repo's established pattern — every change set with user-visible fixes gets a version, tags
have never been cut, RISK-003 open). End-state validation: DSP suite **45 tests + guard,
241 checks, 0 failures** (new Tests 43–46; engaged matrices); state suite **15 tests,
924 checks, 0 failures** (2 new regressions); `check-realtime` 93 self-test cases + clean
scan with the widened seed set; preflight + all checker self-tests green; citation gate
green after re-anchoring (see the round's commits).

### Maintainer decisions needed (full evidence in the dashboard §Decisions)

- **D-1 (KI-027 / ER-RT-01):** approve moving latency-notification delivery off the audio
  thread (candidate: atomic flag consumed by the editor's 24 Hz poll, keeping the synchronous
  path for message-thread calls; alternatives: AsyncUpdater, timer). Threading-model gate item.
  Until decided, the defect stands recorded; the latency VALUE is unaffected.
- **D-2 (RISK-007):** whether to add a narrow guard (mutex over the state-set members, or
  callAsync marshalling of the metadata tail) for off-main-thread state calls, or accept the
  AU exposure as documented. Also gated. Round 2 can run the TSan harness first (recommended).
- **D-3 (Level-5 audition):** **[CLOSED 2026-09-01 — PERFORMED BY THE MAINTAINER, PASSED; see round 6.]** As recorded at the time: precondition 7 is open for v0.9.6 — the 2026-08-15 audition
  covered v0.9.4; since then every x86-64 binary's machine code changed (ADR-0031/0032) and
  0.9.6 changed engine behaviour in the defective windows (prepare-settle, oversized blocks).
  Needs a human DAW session; cannot be automated (TESTING_POLICY Level 5).
- **D-4 (version):** 0.9.6 bump + renumbering executed per the repo's established pattern —
  override if you want the round folded differently.

### Roadmap after round 1 (evidence-ranked; the dashboard tracks live state)

1. **R2-1** ER-STATE-03/ER-DEP-03 — NaN parameter injection via session/preset (`raw="nan"`
   passes every clamp; JUCE parses `nan`). Guard (`std::isfinite` fallback to default) +
   state test + a fuzz seed. Small, closes the last non-finite ingress.
2. **R2-2** TSan two-thread state-call harness (feeds D-2 with measurements).
3. **R2-3** CI-file comment drift batch: build.yml header "ONLY LINUX validates the shipped
   bytes" (ER-CI-02), codeql.yml unpinned-compiler comment (ER-CI-03), check-gcc-warnings
   header pair (ER-CI-04) — batched deliberately: build.yml line shifts re-anchor many
   citations, so they land together, once.
3b. **R2-3b** release.yml annotated-tag check misreports a transient fetch failure (ER-CI-05).
4. **R2-4** `-DANAMORPH_JUCE_PATH` revision check (ER-DEP-04): configure-time rev-parse
   WARNING on mismatch (must stay a warning — the twin dump deliberately points at an old tree).
5. **R2-5** GUI low-priority pair: cancelInlineTextEdits scope (ER-GUI-02), KI-013 impact
   escalation note (ER-GUI-03); plus ER-STATE-04 (restore-path comment vs pinned JUCE) —
   verify then correct.
6. **R2-6** Twin-dump transition scenarios (extends the ER-TST-02 boundary) — only if the
   maintainer wants the gate's surface widened; costs hash-churn on every toolset move.
7. **Deferred, revisit-on-evidence:** ER-DSP-05 (chorus LFO float phase at 192 k×8 OS beyond
   the tested envelope), ER-DEP-06 (silent preset-load failure UX — UI copy is
   maintainer-owned, C8), Windows staging loadability probe, auval scope (RH-F3).

### Checklist (round 1)

- [x] Baseline validation recorded
- [x] 8-lens sweep + adversarial verification
- [x] 10 code/tooling fixes implemented with regression coverage
- [x] 2 registry filings (KI-027, RISK-007) for gate-blocked defects
- [x] Doc drift sweep (renumbering, NOTICE, CI inventory, counts, KI-010/026)
- [x] Version 0.9.6 + CHANGELOG
- [x] Worklog + live dashboard committed
- [x] Full validation green at end state
- [ ] D-1..D-4 decided (maintainer)
- [ ] Round 2 scheduled per roadmap
