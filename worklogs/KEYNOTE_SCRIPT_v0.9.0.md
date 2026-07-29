# Anamorph — Product Video Script: "Four Ways to Shape Stereo Space"

> **Status & class.** Session work product (worklog), prepared against the repository at
> **v0.9.0 (pre-1.0, internal-testing phase)**. This is a *marketing draft* — **derived
> content**: it restates facts established by the code, tests, ADRs and architecture docs, and
> may never be cited as evidence for a technical claim (`docs/SOURCE_OF_TRUTH.md`). It does not
> change product status: Anamorph is **not yet released or for sale**
> (`docs/COMMERCIAL_STATUS.md`). No ™/® symbols are used, per `TRADEMARKS.md`.
>
> **Revision note.** This version supersedes the earlier keynote-style draft in this file. The
> positioning was reworked: the video is no longer a launch advertisement built around mono
> compatibility, but a developer walkthrough built around method selection ("width is a
> method"). A Chinese adaptation is deferred until the English script is approved.

---

## Positioning lock (v1.0)

**Thesis (spoken form):** "Width is not just a number. It's a method."

**Statement.** Anamorph is a stereo width instrument that puts the method in your hands. It
offers several ways of creating space, each with its own character, and keeps the process
visible and controllable while you work. It does not decide what a stereo image should sound
like — the engineer does. It shortens the path from the space the engineer hears to the space
that comes out of the monitors.

**Hierarchy (non-negotiable in every line).** The engineer holds the artistic intention and
makes the aesthetic decision. The instrument provides a more direct path from intention to
result. Nothing in the script implies the viewer misjudges, needs protection, or is being
corrected.

**Competitive tone.** Design philosophy, never criticism. Character processors are designed
around a recognizable sound; M/S utilities around controlling an existing image; imager tools
around visualization and workflow. All valid. Anamorph's distinction: the stereo creation
method itself becomes a creative choice. "Most tools are designed around a particular idea of
space. Anamorph makes that choice explicit."

**Emotional register.** Agency. "The instrument responds exactly to what I intended." A
well-made tool disappears between intention and result.

**Brand words** (Precise / Essential / Unbound) are never spoken or written in the video —
each exists only as shots: method selection and per-band control (Precise); Simple mode and
the absence of accounts/activation/network (Essential); four methods switching in place with
the same session and recall (Unbound).

**Mono compatibility:** one observational line in §4, as monitoring. Never a thesis, never
"safe," never "solved." Per-method summing behavior appears only inside each method's
description in §3, as character.

**Locked spoken lines** (each used once, where marked):
- "Every widening method has a different idea of space. Anamorph lets you choose which idea you want." (§1)
- "Width is not just a number. It's a method." (§2)
- "Most tools are designed around a particular idea of space. Anamorph makes that choice explicit." (§2)
- "Most tools let you choose the result. Few let you choose the process behind it." (§2)
- "Meters reveal. They don't judge. You decide." (§4)
- "It isolates the character difference." (§6)
- "Create the space you hear." (§8 — tagline, once, on screen and spoken)

**Title:** *Anamorph — Four Ways to Shape Stereo Space*
**Hook variant (thumbnails/social):** *Width Is Not a Knob | Anamorph*

---

## Format

Developer at a desk in a real room, mid-session. Screen capture carries the picture; the
developer appears occasionally, lit like a room. No score under technical sections — silence
and the material being discussed. Music, if any, under the first and last ~20 seconds only.
Every frame captured from a real build (the repo contains no media assets).

Demo sources: one mono synth line (§1, recurring), doubled rhythm guitars (§3 Haas), a mono
pad (§3 Velvet Noise), electric piano (§3 Chorus), a string/synth bus (§3 Dim-D), a full mix
(§5).

Narration ≈ 850 words total. Pacing assumes ~145–155 wpm spoken, with listening gaps where
marked. Section word counts noted for the editor.

---

## The script

### §1 · 0:00–0:55 — "I know the space I want" *(~120 words + four short listens)*

**ON SCREEN:** A DAW session, mid-project, nothing staged. One mono synth line loops — the
scope (not yet explained) shows a single vertical trace. The developer's hands on the
controller, occasionally the developer at the desk. As each variation plays, we see only a
selector changing on a plugin window — no names legible yet, no logo. On the final line, cut
to black, then the title card: **ANAMORPH**, small, no music swell.

**VOICE:**
"This is a part from a track I was working on last spring. One synth, one pattern — mono. And
I knew exactly what I wanted from it: wider, but still. It should sit in one place and just...
take up more of it.

So — same part, four ways.

*(listen)* Wider — but now it leans. One side arrives first.
*(listen)* Wider — and moving. Which is lovely. It's not what I asked for.
*(listen)* Wider — and diffuse. The line turned into a texture.
*(listen)* Wider... and still.

None of these is wrong. They're four different kinds of space. Every widening method has a
different idea of space. Anamorph lets you choose which idea you want.

And that's the plugin I want to show you — because I just did all four of those inside it."

**NOTE:** The four variations are performed inside Anamorph by switching methods. No other
plugin appears in the video. The methods are deliberately unnamed here; they get their names
in §3.

---

### §2 · 0:55–1:40 — "Width is made, not turned up" *(~115 words)*

**ON SCREEN:** First proper look at the interface: Simple view — the diamond scope, one WIDEN
panel. Camera settles on the Algorithm selector. A minimal M/S diagram overlays for one
sentence only — two labeled arrows, gone in four seconds. No tutorial graphics beyond that.

**VOICE:**
"Here's the thing I kept running into, long before I wrote any code. Width is not just a
number. It's a method. Every widener gives you a knob for *how much*. Behind that knob there's
a process — a delay, a modulation, a decorrelation — and the process is what you're actually
hearing. Most tools are designed around a particular idea of space. And that's a legitimate
way to design — someone commits to one idea and refines it. Anamorph makes that choice
explicit. Most tools let you choose the result. Few let you choose the process behind it.

Under the hood it's all Mid/Side. The Mid stays where it is. Each of the four methods is a
different way of making Side content. That's the whole architecture. Here they are."

---

### §3 · 1:40–3:35 — The four methods *(centerpiece; ~250 words + four demos of ~6–8 s)*

**ON SCREEN:** One section per method. The selector changes; the method's own controls slide
in (Delay + Focus for Haas; Density for Velvet Noise; Rate + Depth for Chorus; Style for
Dim-D). Each demo plays on its own source, and the scope is visible in every shot — its
behavior quietly differs per method, which §4 will pick up. No comparison charts, no
checklists.

**VOICE:**

"Haas first — the oldest idea here: arrival time. One channel leads the other, anywhere from
one to thirty-five milliseconds, and Focus picks the side. *(doubled guitars)* That's
separation. The part doesn't just widen — it takes a side. The trade comes with the physics:
the two channels stop matching, so a mono sum turns that delay into comb filtering. You'll see
it on the meter — more on that in a minute.

Velvet Noise is the reason I started building this at all. No delay, no modulation. It
manufactures Side content out of the Mid, by sparse decorrelation. *(mono pad)* Diffuse. Not
left or right — around. And nothing moves. The Mid passes straight through, so a mono sum
hears the untouched center — what folds away is the width itself. You're spending focus to
buy air.

Chorus, you already know. A modulated delay, with the two sides moving out of step with each
other. *(electric piano)* The character is motion — and motion is the point. When a part
should shimmer, nothing else does this. When a part has to hold still... it won't. That's its
whole personality.

And Dim-D — the answer to the opening request. It's my take on an idea from a legendary piece
of studio rack hardware: run two modulated taps against each other in each channel, so most of
the pitch movement cancels itself. What's left is size. *(string bus)* Four voicings, Subtle
through Lush — and even Lush doesn't wobble. It's not an effect you point at. The room just
got bigger.

Whichever method you pick, the same two controls sit underneath. Amount — how much of the
process. Width — scaling the Side, from mono up to double. Amount at zero is designed to pass
the input through untouched. And there's a Drive stage in front, for when a part needs density
before it needs space."

**NOTE:** No method is framed as superior; each beat is process → character → the request it
answers → what it costs. Keep internal figures (tap counts, modulation offsets, window
lengths) out of narration — wrong altitude. The named hardware behind Dim-D is never spoken.

---

### §4 · 3:35–4:15 — What the process looks like *(~95 words)*

**ON SCREEN:** The diamond scope, full frame. Quick intercuts: the §3 sources replayed for
two seconds each — the trace leans (Haas), becomes a cloud (Velvet), breathes (Chorus),
widens in place (Dim-D). Then the correlation meter below, moving with the material.

**VOICE:**
"Now — the diamond. The scope is drawn so mono is a vertical line, and Side content spreads it
sideways. Which means each method draws its own picture. Haas leans the trace. Velvet turns it
into a cloud. Chorus breathes. Dim-D just gets wider. You can watch the process you chose.

Under it, correlation — plus one to minus one. Width is Side energy, and Side behaves
differently when something downstream sums it. The meter lets you keep an eye on that while
you work. That's all it's for. Meters reveal. They don't judge. You decide."

**NOTE:** The only mono line in the video is the one above. Verify the four scope
characterizations against the real build during capture; adjust wording to what the trace
actually does.

---

### §5 · 4:15–5:05 — When the space changes with frequency *(~125 words, narration rides over the demo)*

**ON SCREEN:** A full mix. Global width first — the whole image opens, including the lows.
Then the Advanced view: the live spectrum, a crossover dragged into place (smoothly), a second
one added with a click. Per-band width lines set: low band pulled to center, mids near
unchanged, top opened. Hold the band-solo headphone icon; release. One shot of a crossover
automation lane in the DAW.

**VOICE:**
"Most of the time I don't want one width across the whole spectrum anyway. Here's a full mix.
If I open everything up, the low end spreads with it — and I want the low end planted.

So: the full view. The spectrum is live. I can drop in up to three crossovers and drag them
where I want them. Still one method — but every band gets its own width. Lows stay put. Mids
barely move. The top opens.

The headphone icon solos a band. That's a monitor tap at the end of the chain — it changes
nothing about the processing. There's a mono maker too, if you want everything below a chosen
frequency brought to center. A shaping decision, like any other. And all of it takes
automation — crossovers included."

---

### §6 · 5:05–5:55 — Getting back to it *(~115 words)*

**ON SCREEN:** The A/B pill in the top bar. A holds Velvet Noise, B holds Dim-D, same part.
Switching between them; each slot's preset name visible. Level Match engaged — the readout
settles; the two states now sit at the same loudness. APPLY clicked once. Undo stepped back
through a knob gesture. A preset file shown in a file browser, then the same preset open in
the standalone app.

**VOICE:**
"The rest is about coming back to a decision. A and B are two complete working states — each
keeps its own settings, its own preset, its own undo history, a hundred and twenty-eight steps
deep. I use them to hold two methods on the same part.

One thing matters when you compare: different methods don't land at the same loudness, and
louder always makes a strong first impression. Level Match measures both on a K-weighted curve
and holds them level. It isolates the character difference. Once you've chosen, Apply writes
the gain into the output and switches itself off.

Presets are plain files. They move between machines — and between the plugin and the
standalone."

---

### §7 · 5:55–6:30 — The instrument itself *(~90 words; hard cap 35 s)*

**ON SCREEN:** Flat and quick: the plugin running on Linux, Windows, macOS — three short
clips, no logos animation. The standalone app. A network monitor beside the running plugin:
flat line. Plain text, small: *VST3 · AU (macOS) · Standalone. No AAX. Output is always
stereo.*

**VOICE:**
"The practical part, quickly. VST3 on Linux, Windows and macOS. AU on the Mac. A standalone
app. The output is always stereo, and there's no AAX version — I'd rather say that plainly.

It runs with no account, no activation, and no network connection. It sends nothing, to
anyone.

It reports zero latency unless you enable oversampling and something nonlinear is actually
running — then it reports what it uses, and holds it steady.

And right now, Anamorph is in testing. A small group of engineers is working with it before I
call it finished."

---

### §8 · 6:30–7:05 — Close *(~65 words)*

**ON SCREEN:** Back to the opening session. The mono synth from §1, now sitting wide and
still — the Dim-D setting from the cold open. The scope holds. Cut to black. Tagline, plain
text, no music swell: **Create the space you hear.** Then a plain availability card
(finalize at release).

**VOICE:**
"So — no. Anamorph won't tell you how wide anything should be. That was never the job. You
already hear the space you want. You've probably been hearing it all along.

What I built is the shortest path I could make between that — and what comes out of your
monitors.

Four ideas of space. Yours to choose.

Create the space you hear."

---

## Production guardrails

**Claims discipline**
- No absolute claims in narration: "designed to," "lets you," "reports" — never "eliminates,"
  "guarantees," "solves."
- Mono compatibility is never a promise; the §4 line and the per-method character sentences in
  §3 are the only mentions.
- Velvet Noise phrasing is fixed: "a mono sum hears the untouched center" — not "bit-identical."
- Latency is phrased as *reported*: zero unless oversampling is engaged with nonlinear work.
- "K-weighted curve" describes the Level Match measurement; no standards-compliance claim.
- Dim-D: "an idea from a legendary piece of studio rack hardware" — the original hardware and
  its manufacturer are never named on screen or in narration (open trademark review item).
- No ™/® anywhere. Banned vocabulary: revolutionary, ultimate, next-generation, perfect,
  intelligent, powerful, innovative, professional-grade, comprehensive; avoid "precision,"
  "transparent," "advanced" (except the UI's own labels), "optimized."
- Product status: the §7 testing line is the only status mention; the availability card is
  plain text, finalized at release. Nothing implies the product is on sale.

**Capture guardrails**
- All four §1/§3 demonstrations are performed inside Anamorph; no competitor UI appears.
- Drag crossovers smoothly on camera; no violent flicks (KI-012: fast drags carry a small
  bounded FM by design).
- One visual theme only (no skins exist). Window sizes are five fixed steps — no free-resize
  shots. Tooltips are off by default; enable in Settings before any shot that needs them.
- No typed-value-then-undo demonstrations (typed entries create no undo step, KI-010).
- Windows capture: use the VST3 in an ASIO host (the Windows standalone has no ASIO).
- No named-DAW compatibility claims; the session shown is whatever host captures cleanly.
- Verify the §4 scope characterizations (lean/cloud/breathe/widen) on the real build before
  final edit; adjust narration to match observed behavior.

**Fact-source map (every narration claim → repository evidence)**
- Four methods & their controls → `src/dsp/EngineParameters.h`, `src/dsp/HaasProcessor.cpp`,
  `src/dsp/VelvetNoise.{h,cpp}`, `src/dsp/ChorusEngine.{h,cpp}`; user manual §3.3–4
- Haas 1–35 ms + Focus; mono-sum combing caveat → `EngineParameters.h`, user manual §4
- Velvet: Side built from Mid; center untouched in a sum → `VelvetNoise.h`, DSP_POLICY inv. 6
- Dim-D: two anti-phase taps, pitch movement cancels; four voicings → `ChorusEngine.{h,cpp}`
- Amount 0 designed as identity; Width 0–200%; Drive ahead of the method →
  DSP_POLICY invariant 8, `docs/architecture/SIGNAL_FLOW.md`, user manual §3.3
- Scope orientation (mono vertical / Side horizontal); correlation ±1 →
  `src/gui/Vectorscope.h`, user manual §3.2
- Multiband: ≤3 crossovers / 4 bands, per-band width, live spectrum; solo is monitoring-only →
  `src/dsp/MultibandWidth.h`, `src/dsp/SoloMonitor.h`, `src/gui/SpectrumImager.h`, ADR-0006/0014
- Mono maker 20–500 Hz → `src/dsp/MonoMaker.h`, user manual §3.5
- Automation incl. crossovers/band widths → user manual §9, ADR-0014
- A/B slots with per-slot preset, undo (128), matched gain → ADR-0008, user manual §3.1/§7.4
- Level Match K-weighted; Apply writes Output Gain and disengages → `src/dsp/LoudnessMatch.cpp`, ADR-0007
- Presets as portable plain files, shared with standalone → user manual §7.2
- Formats/platforms; no AAX; output always stereo → README, `docs/architecture/COMPATIBILITY_MATRIX.md`
- No account/activation/network/telemetry/logs → `PRIVACY.md`, CMakeLists (JUCE_WEB_BROWSER=0, JUCE_USE_CURL=0)
- Latency reported only with engaged oversampling; held steady → `docs/architecture/LATENCY_MODEL.md`, ADR-0003
- Internal-testing status → README, `docs/COMMERCIAL_STATUS.md`
