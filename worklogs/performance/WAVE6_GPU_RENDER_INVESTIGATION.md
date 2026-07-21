# Performance Wave 6 — GPU / GUI rendering-efficiency investigation (v0.8.12)

> Goal: reduce **unnecessary GPU/rendering** workload (idle GPU, redundant redraws, CPU→GPU
> uploads, thermal/power) while keeping the editor **pixel-identical**, GUI behaviour identical,
> and DSP untouched. Not a DSP task; not a rendering-architecture redesign. Numbers are
> session-local. **Outcome: one behaviour-neutral fix** — the per-band solo-headphone glyph in
> the SpectrumImager stops allocating a **plot-sized offscreen framebuffer** every frame (it now
> clips the transparency layer to the glyph, and skips it entirely at full opacity). Everything
> else was investigated and **rejected with evidence**: the idle and idle-simple pipelines are at
> their optimization frontier, and the one structural asymmetry (the spectrum being the only
> non-opaque visualizer) **cannot** be removed pixel-identically.

- **Date:** 2026-07-21
- **Branch:** `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `c6f3226` (PR #78 merged;
  fast-forward, fresh follow-up → **PR #79**). Target version **0.8.12**.
- **Environment / method:** same container, gcc 13.3.0, CMake/Ninja Release. Static analysis of the
  full repaint→GPU pipeline plus a 5-lens adversarial Workflow (14 agents) tasked to *find* a win or
  prove none survives. **Direct GPU measurement is unavailable** here (headless container, no GPU/
  display; the GL compositor path exists only on macOS/Windows) — see §Measurement.

## 1. GPU/render baseline — how the editor reaches the GPU

- **Compositor:** one `juce::OpenGLContext` attached to the whole editor, **macOS/Windows only**
  (`src/PluginEditor.cpp:278`, gated `#if ! (JUCE_LINUX || JUCE_BSD)`); **Linux/BSD render CPU-side**
  (ADR-0011 — GL there use-after-frees in the host's X11 embedding). So *all* "GPU cost" reasoning is
  **platform-specific to macOS/Windows**. The context is `setContinuousRepainting(false)`
  (`:264`) and uses every JUCE default (no MSAA, default pixel format, default 8-image texture cache).
  **GPU present rate == repaint rate** — there is no free-running compositor.
- **Two per-frame drivers:** (a) one 24 Hz `juce::Timer` for housekeeping (`timerCallback`,
  `:949-1055`) that emits **zero** repaints when nothing changed (every path is change-gated); (b) a
  per-visualizer `gui::FrameClock` (VBlankAttachment, adaptive, capped ~125 Hz; `src/gui/FrameClock.h`)
  driving the four visualizers.
- **Four visualizers, all already gated:** Vectorscope, LevelMeter, StereoMeter×2, SpectrumImager
  each (i) early-return their tick on `!isShowing()`, (ii) gate `repaint()` on a real data change, and
  (iii) `stop()` their FrameClock when hidden. Static geometry is cached in **persistent** member
  Images (H2/H13/H17/N2), regenerated only on size/scale change, so the GL texture cache keeps them
  resident (no per-frame re-upload).

**Repaint frequency per state (measured by tracing the gates):**

| State | Per-frame GPU present? |
|---|---|
| Editor hidden | **0** — FrameClocks stopped; timer emits no repaints |
| Idle-simple (silent, settled) | **0** — scope freezes on window-silence; meters settle to floor; micro-anims idle-gated |
| Idle-advanced (silent, settled) | **0** — additionally the spectrum's `magsSettled`/`redLevel` snap to floor |
| Active meters | meter repaints only on a value change (bitwise S3 gate) |
| Active vectorscope | repaints only while the visible window content changes |
| Active spectrum (Advanced + audio) | full `SpectrumImager::paint` every vblank (inherent — the picture changes every frame) |

The idle path is airtight for the common case (true digital zero / stopped transport / frozen ring).
The GPU only works when a visualizer's data is genuinely changing — as designed.

## 2. Hotspot findings

The active-spectrum state is the only one with meaningful per-frame GPU cost, and almost all of it is
**inherent** dynamic drawing (the spectrum path, clip quads, band tints, ruler). One item is **not**
inherent and was avoidable:

**H-GPU-1 (implemented) — per-band solo-headphone transparency layer.** `SpectrumImager::paint`'s
`paintHeadphone` lambda (`~1329`) wraps each headphone glyph in `g.beginTransparencyLayer(alpha)` /
`endTransparencyLayer()`. JUCE sizes the offscreen image/FBO that a transparency layer allocates to the
**current clip bounds** — and at that point the clip is the whole **plot rounded-rect** (set at
`:1104`), *not* the ~18×15 px glyph. The per-band loop (`~1348`) is **not** interaction-gated: any band
wider than 30 px always shows a headphone, so on macOS/Windows this allocated a **plot-sized offscreen
FBO + full-plot alpha composite, up to ~4×/frame, every frame the spectrum repaints while Advanced is
open and audio plays** — pure overhead behind the tiny glyph.

## 3. Optimization implemented

`src/gui/SpectrumImager.cpp`, `paintHeadphone` — two behaviour-neutral trims, both confirmed by
independent adversarial verification:

1. **Clip the layer to the glyph before `beginTransparencyLayer`.** `g.reduceClipRegion(bx.expanded(4)
   .toNearestInt())` bounds the offscreen JUCE allocates to ~26×23 px instead of the whole plot. The
   **+4 px margin** is what keeps it **pixel-identical**: the earcups (`fillRoundedRectangle`) reach
   ~1 px past `bx` and add ~1 px of AA, so every drawn pixel lies strictly inside the clip and the
   composited result is byte-identical — only the offscreen shrinks. (A naive clip to the tight `bx`
   would shave the earcups; that is the pixel-diff the verifier flagged, and the margin is the fix.)
2. **Skip the layer at full opacity.** The layer exists only so the headband + earcups can't
   double-blend into a bright seam at *partial* opacity (0.6.11 #2). At `alpha == 1.0` (the soloed/on
   band) opaque same-colour draws overwrite rather than accumulate, so there is no seam and the layer
   buys nothing — draw direct. Only the soloed/on band reaches 1.0; every other band eases in
   `[0.4, 0.9]` and still takes the (now glyph-clipped) layer path.

No visual change, no geometry change, no DSP, no threading/parameter/serialization/latency change.
Diff is one lambda in one file.

## 4. GPU impact

Per non-soloed wide band, per frame, in Advanced mode with audio playing (macOS/Windows GL):

- **Offscreen allocation + clear:** plot-sized ARGB (≈ 900×144 logical → ~520 KB at scale 1, ~2.1 MB
  at Retina 2×) → **~26×23** (~2.4 KB / ~9.6 KB) — a **~200×** reduction in the offscreen JUCE creates
  and clears.
- **Composite:** a full-plot textured-quad blend → a ~600-px blend.
- **GL batch breaks:** `beginTransparencyLayer` forces two render-batch flushes (defeating triangle
  batching); still incurred, but now around a trivially-small target.
- Up to **~4 bands × up to ~120 Hz** in the worst case → on the order of a few MB/frame of offscreen
  alloc+clear+composite removed from the advanced-mode present.
- **Soloed band:** the offscreen is removed **entirely** (direct draw).
- **Idle / simple / hidden:** unchanged (already 0).
- **Linux (CPU render):** same reduction in heap allocation + fill area; no GPU, but lower CPU paint.

## 5. Rejected candidates (investigated, not taken)

| Candidate | Why rejected |
|---|---|
| **Make SpectrumImager opaque + RGB** (the N2 trick used on the other 3 visualizers, to drop the per-frame parent-behind repaint + ARGB blend) | **Not pixel-identical.** The imager nests inside the translucent rounded `multiPanel` and is *bottom-flush* with it (imager.bottom == multiPanel.bottom, inset 2 px). Its two **bottom** corners fall inside multiPanel's 10 px transparent rounded cutout, so what shows through the imager's own 6 px corners there is an **arc boundary between two colours** (flat `colours::bg` outside the arc, `bgPanel.withAlpha(0.5)`-over-`bg` inside). No flat corner pre-fill can reproduce a two-colour curved split. This is exactly why N2 skipped the spectrum. (Adversarially re-checked: refutation failed.) |
| **Idle gate on a level threshold instead of bit-exact zero** (a sub-audible non-zero floor — denormals, DC, a −138 dBFS IIR tail — currently keeps the scope/spectrum repainting) | **Behaviour change, not behaviour-neutral.** An ε-threshold would freeze the scope on a signal that currently (sub-visibly) animates it — a change to *when* the view idles. Risk medium, benefit low; the common true-silence case is already handled. Fails "identical visual output". |
| **bottomLayer re-rasterize/upload while easing** (a width drag / panel-hover rebuilds the full-panel ARGB layer each frame) | **Transient interaction only**, not steady-state; the band tints are baked into the layer by design (splitting them out is a rendering-architecture change). Steady state provably never rebuilds (exact ease-snap). |
| **DropShadow on width lines / split curves** (`~1233`, `~1315`) | **Interaction-gated** (fires only while a width/split is being touched, `act/pr/pressA > 0.01`); not steady-state. Changing it risks the deliberate look. |
| **Sub-region `repaint(rect)` for meters** instead of full-component | No stable dirty sub-rectangle (bars/pointer/numbers move across the component); real stale-pixel risk for a marginal saving on small components. Fails low-risk/high-value. |
| **GL-context config** (swap interval, texture magnification, image-cache size) | Every default is already optimal for this use; changing any is a behaviour/pixel change, not a free win. Re-enabling GL on Linux is explicitly forbidden (ADR-0011). |
| **Cap visualizer FPS below the display rate** on high-refresh panels | Changes visible smoothness → not "identical visual output"; the FrameClock's ~125 Hz cap is a deliberate design point. |

## 6. Independent adversarial verification

A Workflow (`wave6-gpu-render-hunt`, 5 hunt lenses → per-finding adversarial verify, 14 agents,
0 errors) surfaced 25 findings. Only the headphone item survived verification as
implement-worthy (F2 `CONFIRM_IMPLEMENT` pixel-identical + F1/FE-2 the glyph-clip, the medium-value
part); every structural alternative — spectrum opacity (`pixel=no, risk=high`), idle ε-gate
(`behaviour change`), ease-rebuild, drop-shadows, meter sub-region, GL-config — was independently
**REJECTED**. The cross-platform lens confirmed the cost is **macOS/Windows-only** and that there is
**no accidental continuous repaint or whole-editor per-frame invalidation**.

## 7. Measurement limitation

Direct GPU profiling is **unavailable** in this environment: the container is headless (no GPU, no
display server) and the GL compositor path is compiled only for macOS/Windows. The impact in §4 is
therefore an **analytical estimate derived from the code geometry** (offscreen size = clip bounds;
plot rounded-rect vs glyph box) rather than a captured GPU-counter delta. Because the change is
**pixel-identical**, the correctness gate is the build + DSP suite + the pixel-identity argument
above; the efficiency gain is bounded below by the offscreen-size reduction, which is exact.

## 8. Validation

- **Build:** `Anamorph_VST3` + `AnamorphTests` build and link **clean** (Release/LTO; the only
  emitted line is the benign `lto-wrapper: using serial compilation of 88 LTRANS jobs` note — a
  build-parallelism message, not a code warning). `SpectrumImager.cpp` recompiled with **no warnings**.
- **DSP suite:** `AnamorphTests` — **140 checks, 0 failures**, identical to the pre-change baseline (no
  product DSP code was touched). No GUI pixel unit-test exists; editor rendering is validated by
  pluginval per ADR-0011 and by the pixel-identity argument below.
- **Pixel-identity:** argued per-pixel in §3 — the `bx.expanded(4)` clip contains every drawn pixel
  (earcup overflow ~1 px + AA ~1 px ≪ 4 px margin, then intersected with the same plot rounded-rect the
  glyph was already under), and the `alpha == 1.0` direct-draw path is opaque and seam-free. The
  composited output is unchanged; only the offscreen JUCE allocates behind it shrinks.
