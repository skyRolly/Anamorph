# Anamorph — Launch / Highlight Video Script (Keynote Style)

> **Status & class.** Session work product (worklog), prepared against the repository at
> **v0.9.0 (pre-1.0, internal-testing phase)**. This is a *marketing draft* for a future launch
> video — **derived content**: every claim in it restates facts established by the code, tests,
> ADRs and architecture docs, and it may never be cited as evidence for a technical claim
> (`docs/SOURCE_OF_TRUTH.md`). Nothing here changes product status: Anamorph is **not yet
> released or for sale** (`docs/COMMERCIAL_STATUS.md`). Availability lines are left as
> placeholders to be finalized at launch. No ™/® symbols are used anywhere, per
> `TRADEMARKS.md`.

---

## Part 1 — Product positioning analysis

**Category.** Anamorph sits at the intersection of three plugin categories: stereo wideners
(the "make it bigger" tools), M/S utility processors (the "channel plumbing" tools), and
stereo metering suites (the "am I safe?" tools). Most products pick one. Anamorph's identity
is that it is deliberately all three around a single question: *how wide can this be, and can
I trust it?*

**The wedge.** Every stereo widener on the market asks the user to accept a trade: width now,
mono-compatibility anxiety later. Anamorph's differentiation is that mono-safety is a
**construction property, not a checking chore**. The default algorithm (Velvet Noise)
synthesizes the Side from the Mid and leaves the Mid untouched — mono playback hears the
unprocessed center. Width is pure Side gain (`L+R = 2·Mid` regardless of Width). The multiband
recombines allpass-flat. The Mono Maker runs post-Mix. The correlation meter and diamond
vectorscope sit at the center of the UI, not in a submenu. The product's promise can be stated
in one line: **width you can verify**.

**The second wedge: radical engineering honesty as brand.** The repository documents a
verifiable-transparency discipline that most commercial plugins never attempt: Mix 0% is a
bit-exact null, Drive 0 dB is a true identity, bypass settles bit-exact, reported latency
equals measured latency by test, optimizations only shipped when proven output-identical, and
the one artifact that physics wouldn't remove (fast crossover-drag FM) was measured five ways
and parked at the threshold of audibility — then *published* as a known issue instead of
hidden. Add the privacy stance (no account, no activation, no network code, no telemetry, no
log files — "verify it yourself with a network monitor") and you have a trust narrative that
is genuinely rare in this market.

**Positioning statement.** *For mixing and mastering engineers, producers and sound designers
who need real stereo width without regrets, Anamorph is a precision stereo-field toolkit that
creates and controls width with mono-compatibility engineered in by construction — unlike
conventional wideners that make you choose between sounding big and staying safe.*

**Audience mapping.**

| Audience | What lands hardest |
|---|---|
| Mixing engineers | Mono-safety by construction; correlation/scope instrumentation; loudness-matched A/B; zero latency |
| Producers | One-knob simple mode; four distinct flavors; 10 presets; click-free everything; automatable everything |
| Sound designers | Velvet Noise mono→stereo synthesis; Dim-D voicings; Drive; extreme-automation robustness (self-healing engine) |
| Mastering engineers | Phase-coherent multiband width; allpass-flat recombination; Mono Maker; BS.1770 level match; bit-exact null |
| DSP engineers / plugin devs | Anti-phase Dim-D taps; phase-matched dry reconstruction; conditional latched oversampling; 914-check gate; sessions-never-break contract; the ADR/postmortem culture |

---

## Part 2 — Top 8 selling points (discovered from the repository)

1. **Mono-safety by construction.** The default Velvet Noise engine builds a decorrelated Side
   from the Mid and never touches the Mid — mono playback hears the unprocessed center. Width
   is pure Side gain, the multiband is per-band mono-compatible, and the Mono Maker keeps the
   low end mono at any Mix setting. *(src/dsp/VelvetNoise.h, MidSide.h, MonoMaker.h;
   DSP_POLICY invariant 6)*

2. **Four musically distinct widening engines.** Haas (precedence delay, 1–35 ms), Velvet
   Noise (sparse-FIR decorrelation, mono→stereo), Chorus (animated width), and Dim-D — two
   anti-phase-modulated taps per channel so pitch wobble cancels to first order: "width with
   no seasick vibrato," in four voicings. *(src/dsp/ChorusEngine.h/.cpp, HaasProcessor.cpp)*

3. **Phase-coherent multiband width with a gesture-rich spectral editor.** Up to 4 bands over
   Linkwitz-Riley crossovers with allpass compensation — recombines flat (a measured
   −17.75 dB naive-summing dip, eliminated at zero added latency), edited by dragging splits
   directly on a live spectrum, with monitoring-only band solos that never alter the
   processing. *(src/dsp/MultibandWidth.cpp, src/gui/SpectrumImager.cpp, ADR-0014/0015)*

4. **Honest loudness: BS.1770 Level Match.** K-weighted Measure that holds on silence plus an
   absolute feed-forward Predict that can never ratchet — so "wider" can't masquerade as
   "louder" in an A/B. One click commits the correction. *(src/dsp/LoudnessMatch.cpp, ADR-0007)*

5. **Zero latency unless you actually need it.** Oversampling (2×/4×/8×, minimum-phase, no
   pre-ringing) wraps only the nonlinear stages and engages only when they have work to do;
   engagement is latched so reported latency never jumps mid-note. Linear chains report
   exactly 0 samples. *(AnamorphEngine.cpp, LATENCY_MODEL.md, ADR-0003)*

6. **Click-free everything, bit-exact when idle.** Preset loads, A/B switches and undo dip
   briefly to the *dry signal* — never to silence. Bypass is a crossfade with analyzers still
   running. Mix 0% is a bit-exact null; Drive 0 dB is a true identity; the engine self-heals
   from any non-finite sample within a block, and there is no hidden output clipper.
   *(ADR-0004/0005/0009)*

7. **A monitoring instrument at the center.** The diamond vectorscope (mono reads vertical,
   width reads horizontal, phosphor-style afterglow), a +1/−1 correlation meter that warns
   before mono playback does, balance and in/out peak/RMS metering — all display-rate-adaptive
   and idle-silent. *(src/gui/Vectorscope.cpp, CorrelationMeter.cpp, FrameClock.h)*

8. **Engineering credibility you can audit.** 914 automated checks (33 DSP self-tests plus a
   9-test state-compatibility suite) and pluginval at maximum strictness, both modes, three
   passes each, blocking on Linux, Windows and macOS. Parameter IDs are frozen contracts;
   three generations of legacy sessions load forever. Optimization waves shipped only when
   proven bit-identical. Zero telemetry, zero accounts, zero log files — stated with evidence
   and an invitation to verify. *(tests/, .github/workflows/build.yml, PRIVACY.md,
   SESSION_COMPATIBILITY_POLICY.md)*

---

## Part 3 — Recommended keynote storyline

**Narrative spine: "Width has always been a gamble. Anamorph ends the gamble."**

1. **Act I — The fear (hook).** Every mix eventually plays in mono: club PAs, phones,
   broadcast. The widener that sounded huge collapses. Establish the emotional stake — years
   of learned distrust of "wideners."
2. **Act II — The idea (reveal).** Anamorph's philosophy in one picture: Mid is what mono
   hears; Side is what makes it wide. Anamorph builds the Side and protects the Mid. Name the
   promise: *width you can trust.*
3. **Act III — The instrument (tour).** Four engines, each with a personality; Width and
   Drive; then Advanced mode: multiband on a live spectrum, the scope, the meters.
4. **Act IV — The proof (credibility).** The workflow of honest decisions (level-matched A/B,
   per-slot undo), then the engineering: the test gate, bit-exactness, zero latency, sessions
   that never break, zero telemetry. This is where trust is earned, in plain language.
5. **Act V — The music (scenarios).** Four fast vignettes: vocals, a mono synth, a drum bus,
   a mastering pass. Show, don't tell.
6. **Act VI — The vision (close).** The stereo field as a *place*, not a trick. Tagline:
   **"Anamorph. Width you can verify."**

Runtime target ≈ 7 minutes English / 7 minutes Chinese. Narration pacing assumed at
~140 wpm (EN) / ~240 chars-per-minute (ZH).

---
---

# Part 4 — Version 1: English Product Launch Video Script

**Working title:** *Anamorph — Width You Can Verify*
**Runtime:** ~7:15 · **Tone:** confident, calm, cinematic; Apple-keynote restraint with
FabFilter-grade technical credibility. Music: dark minimal pulse that *opens into wide stereo*
exactly when the product is revealed (the mix of the video is itself a demo).

---

### [00:00–00:45] — Cold open: the fear

**Visual:**
Black. A single thin white waveform, dead center of frame. We hear a gorgeous, wide synth
chord — then hard cut: the same chord through a phone speaker on a kitchen table. Cut: a club
PA at night, house lights off. Cut: a mall ceiling speaker. Each cut, the on-screen stereo
meter folds from two bars into one. On the final fold, the wide chord audibly thins and
hollows out — classic comb-filter collapse.

**Voice:**
"Every mix you ever make... will one day be played in mono. Maybe not by you. By a club
system. A phone. A speaker in a ceiling. And that is the moment every stereo widener fears.
The width that sounded enormous in your studio — thins, phases, disappears. For decades,
width has been a gamble. Sound bigger now. Pay for it later. ...What if width didn't have to
be a gamble?"

**Key message:** The creative problem: width vs. mono truth. Establish stakes and distrust
before naming the product.

---

### [00:45–01:35] — Reveal: what Anamorph is

**Visual:**
From the single center line, the screen blooms: the line splits into a diamond — the Anamorph
vectorscope trace — as the soundtrack opens into true stereo. Logo resolve: **ANAMORPH** — *by
RollyTech*. The full interface fades in: near-black glass, a breathing diamond scope, one
clean WIDEN panel. Slow dolly across the UI. On "Mid," the scope shows a vertical trace; on
"Side," it spreads horizontal.

**Voice:**
"This is Anamorph. A stereo-field toolkit built on one principle: the center of your mix is
sacred. Anamorph thinks the way mono playback listens — in Mid and Side. The Mid is
everything mono will ever hear. The Side is everything that makes it wide. Anamorph creates
width by *building the Side* — while the Mid stays untouched. Turn mono into stereo. Shape
width across the whole mix, or band by band. And verify every move, on a vectorscope built
like a precision instrument. This is width you can trust."

**Key message:** Product identity + core philosophy (M/S; protect the Mid; verify visually).

---

### [01:35–02:55] — The four engines

**Visual:**
Macro shots of the WIDEN panel. The Algorithm selector cycles; each engine gets a short
visual signature and a two-bar audio demo on a different source:
- *Haas* — a dry guitar; a subtle diagram of two arrival times; the image leans left, then right with the FOCUS switch.
- *Velvet Noise* — a mono synth line; sparse impulse dots scatter across a 45 ms window; the vertical scope line blooms wide. A big caption: **MONO IN. WIDE OUT. CENTER UNTOUCHED.** Then the mono-check button: the sum collapses back — and sounds *identical* to the original mono synth.
- *Chorus* — a clean electric piano begins to shimmer and move.
- *Dim-D* — pads; two counter-rotating LFO graphics cancel each other; caption: **MOTION, WITHOUT THE WOBBLE.** STYLE steps through Subtle / Classic / Wide / Lush.
End on the Width knob sweeping 0 → 200% and Drive adding density with no level jump.

**Voice:**
"Four widening engines. Four personalities. *Haas* uses the oldest trick in psychoacoustics —
a few milliseconds of precedence — to lean your image exactly where you want it. *Velvet
Noise* is Anamorph's signature: it synthesizes a brand-new Side channel from your Mid, using
sparse velvet-noise decorrelation. True mono in — wide, natural stereo out. And when the
world folds your mix back to mono... it hears the untouched center. No combing. No
surprises. *Chorus* gives you classic, animated width. And *Dim-D* — inspired by the legendary
studio dimension expander — runs two modulated taps in anti-phase, so the pitch wobble
cancels itself. Space, without the seasickness. Under all of it: Width, from mono to double
wide. And Drive — up to twenty-four dB of saturation that adds density, not volume tricks."

**Key message:** Distinct engines, each musically motivated; the default one is the mono-safe
hero.

---

### [02:55–03:55] — Surgical width: multiband + the scope

**Visual:**
The window extends into Advanced mode. The MULTIBAND editor: a live spectrum, a crossover
handle dragged — neighbors push aside and spring back; a band-pass preview curve appears on
hold. Click a gap: a new split. Per-band width lines drag up and down. A headphone icon:
hold-to-audition one band (caption: **SOLO TOUCHES NOTHING**). The low band's width line pulls
to mono; the vectorscope narrows at the bottom while the top stays wide. The correlation
meter rides near +1 with a brief dip flagged in warm color, then recovers.

**Voice:**
"Real mixes don't need one width — they need the *right* width at every frequency. Anamorph
gives you up to four bands of phase-coherent multiband width, on Linkwitz-Riley crossovers
with allpass compensation — so the bands recombine perfectly flat, with zero added latency.
Drag a crossover on the live spectrum. Widen the air. Tighten the mids. Anchor the bass in
mono, where it belongs. Hold a band's solo to audition it — soloing is pure monitoring; it
never changes your processing. And through all of it, the diamond scope tells the truth:
vertical is mono. Horizontal is width. The correlation meter warns you — before mono
playback does."

**Key message:** Per-band control that stays phase-coherent + the instrumentation to see it.

---

### [03:55–04:45] — The honest workflow

**Visual:**
Top bar close-ups. The A/B pill flips — audio continues seamlessly (visible waveform, no
gap). LEVEL MATCH engages: the readout settles; APPLY commits the gain in one click. Undo
steps back through a whole knob gesture at once. The preset stepper walks through *Vocal
Air*, *Drum Spread*, *Wide Master*. Kinetic type: **LOUDER ISN'T BETTER. IT'S JUST LOUDER.**

**Voice:**
"Anamorph is built for honest decisions. Level Match uses broadcast-grade loudness
measurement to hold A and B at the same perceived level — so 'wider' can never fool you by
being louder. Hold it, trust your ears, commit the gain in one click. A and B aren't just
snapshots — they're two complete workspaces, each with its own preset, its own level match,
its own hundred-and-twenty-eight-step undo. And every switch — preset, A/B, undo, bypass — is
click-free by design. The audio dips to *dry*, never to silence. Your session never pops.
Your ears never flinch."

**Key message:** Workflow features exist to protect judgment; polish is audible.

---

### [04:45–05:55] — Under the hood (technical excellence)

**Visual:**
Style shift: dark engineering montage. Slow pans over abstract circuit-like traces of the
signal chain (no code close-ups). Three platform silhouettes (Linux / Windows / macOS) light
up. Kinetic type beats, one at a time:
**914 AUTOMATED CHECKS** · **PLUGINVAL: MAXIMUM STRICTNESS — EVERY BUILD, THREE PLATFORMS** ·
**MIX 0% = BIT-EXACT NULL** · **0 SAMPLES LATENCY (UNTIL YOU NEED MORE)** ·
**SESSIONS NEVER BREAK** · **0 ACCOUNTS. 0 CONNECTIONS. 0 LOG FILES.**
End the sequence on a quiet screen: a network activity monitor showing a flat line while
Anamorph plays.

**Voice:**
"Under the glass, Anamorph is engineered like an instrument you'd stake a career on. The
audio path is audited allocation-free and lock-free — real-time discipline as written policy,
not a vibe. Latency is zero unless oversampling actually has nonlinear work to do — and when
it engages, it's minimum-phase, fully reported, and locked so it never jumps mid-note. Every
single build must survive nine hundred and fourteen automated checks, and the industry's
plugin validator at maximum strictness — both modes, three passes each — on Linux, Windows
and macOS. When we optimized the engine, changes shipped only when the output was proven
bit-for-bit identical. Parameter identities are frozen contracts — sessions and presets from
every previous version load, forever. And Anamorph collects nothing. Sends nothing. No
account. No activation server. No telemetry. Not even a log file. Don't take our word for
it — put a network monitor on it. Verify it yourself."

**Key message:** Credibility through verifiable discipline — spoken plainly, no jargon walls.

---

### [05:55–06:55] — In the real world (scenarios)

**Visual:**
Four fast vignettes, each 12–15 seconds, each ending on the scope + correlation meter:
1. *Vocal session* — a lead vocal; Advanced mode; high band widened gently (*Vocal Air*); the voice gains air around a rock-solid center; mono check — intact.
2. *Sound design* — a mono synth stab; *Mono To Stereo* preset; Velvet density swept; Dim-D layered for motion; the stab becomes a landscape.
3. *Drum bus* — *Drum Spread*; overheads breathe wider; then *Bass Guard*: the low band pinned mono; kick stays dead center on the scope.
4. *Mastering* — a finished mix; *Wide Master*; a whisper of amount; Level Match on; A/B at matched loudness; the correlation meter stays healthy; the engineer nods.

**Voice:**
"On a vocal — open the air above the consonants, and leave the center bulletproof. On a mono
synth — one preset, and a single line becomes a landscape... that still folds back to mono
perfectly. On a drum bus — spread the room, and lock the kick to the center of the earth.
And on a master — a whisper of width, level-matched, correlation in the green... the kind of
wider you only notice when you turn it off."

**Key message:** Concrete value for each audience; every vignette ends in *proof* (scope/mono
check), reinforcing the trust theme.

---

### [06:55–07:20] — Close: the vision

**Visual:**
Everything falls away except the scope. The trace narrows to the single vertical mono line
from the cold open — then blooms one last time into a wide, stable diamond and holds. Logo:
**ANAMORPH** — *by RollyTech*. Tagline sets beneath it. Final card: availability placeholder.

**Voice:**
"The stereo field was never a trick. It's a place — where your music lives. Make it as wide
as you dare... and know, not hope, that it holds. Anamorph. Width you can verify."
*(Final card, no VO or one short line: availability/date — to be finalized at launch.)*

**Key message:** Vision + memorable tagline. The video's last image is the product proving
itself on its own instrument.

---
---

# Part 5 — Version 2: 产品发布视频脚本(中文)

**片名:**《Anamorph——可以验证的宽度》
**时长:** 约 7 分钟 · **基调:** 克制、专业、有信念感;不喊口号,用事实与画面建立信任。
**旁白风格:** 面向华语专业音频人群(混音师、制作人、声音设计师)的沉稳男声/女声皆可,
语速约每分钟 240 字。配乐本身就是演示:开场刻意单声道,产品亮相一刻打开为宽阔立体声。

> 注:Anamorph、RollyTech、Mid/Side、Haas、Velvet Noise、Dim-D 等名称保留英文;
> 中文行业惯用语:立体声声场、单声道兼容、中置/侧信号(Mid/Side)、梳状滤波、
> 相关表、矢量示波器、分频段、响度匹配。

---

### [00:00–00:45] — 开场:每个混音师都懂的恐惧

**画面:**
黑场。画面正中一条纤细的白色波形线。先听到一段宽阔华丽的合成器和弦——硬切:同一段声音
从餐桌上的手机外放出来;再切:深夜俱乐部的音响系统;再切:商场天花板的吸顶喇叭。每切换
一次,屏幕上的立体声电平表就折叠一分,最终并成一条。最后一次折叠时,宽阔的和弦明显变薄、
发空——典型的梳状滤波塌陷。

**旁白:**
"你做的每一个混音,总有一天,会被单声道播放。也许不是在你的监听室——而是在夜店的音响里,
在一部手机的外放里,在商场天花板的喇叭里。那一刻,是所有立体声扩展插件的噩梦:棚里听起来
无比宽阔的声场,瞬间变薄、相位打架、消失不见。多年以来,'加宽'始终是一场赌博——现在听起来
更大,将来可能付出代价。那么……如果宽度,可以不再是赌博呢?"

**关键信息:** 先立"痛点"——宽度与单声道兼容的天然矛盾,唤起职业听众的共鸣与警惕。

---

### [00:45–01:35] — 产品亮相:Anamorph 是什么

**画面:**
中央那条单声道直线"绽放"开来,化作钻石形的矢量示波器轨迹——与此同时,配乐从单声道打开为
真正的立体声。标志浮现:**ANAMORPH** —— *by RollyTech*。完整界面淡入:近乎纯黑的玻璃质感
面板,一块缓缓呼吸的钻石示波器,一排干净的 WIDEN 控件。镜头缓慢横移。提到"Mid"时,示波器
呈现垂直线;提到"Side"时,轨迹向水平方向展开。

**旁白:**
"这就是 Anamorph——来自 RollyTech 的立体声声场工具箱。它建立在一个原则之上:混音的中央,
不可侵犯。Anamorph 用单声道世界的方式聆听——Mid 与 Side。Mid,是单声道所能听到的一切;
Side,是宽度的全部来源。Anamorph 创造宽度的方式,是构建 Side——而让 Mid 原封不动。把单声道
变成立体声;对整个混音、或者逐个频段,精确雕刻宽度;而每一步操作,都能在一台仪器级的
矢量示波器上得到验证。这,是可以信赖的宽度。"

**关键信息:** 产品定义 + 核心哲学(M/S 思维、保护中置、可视化验证)。

---

### [01:35–02:55] — 四种加宽引擎

**画面:**
WIDEN 面板微距特写。算法选择器逐一切换,每种引擎配一段两小节的音频演示与专属视觉符号:
- *Haas* —— 干声吉他;两个到达时间的示意图;拨动 FOCUS,声像向左、向右倾斜。
- *Velvet Noise* —— 单声道合成器旋律;稀疏的脉冲点散布在 45 毫秒的窗口内;示波器上的垂直线
  绽放成宽阔的钻石。大字幕:**单声道进,宽声场出,中央分毫未动。** 随后按下单声道检听:
  声场收回一条直线——听感与原始单声道完全一致。
- *Chorus* —— 干净的电钢琴开始摇曳、流动。
- *Dim-D* —— 铺底音色;两个反向旋转的 LFO 图形互相抵消;字幕:**有空间感,没有晕船感。**
  STYLE 依次切换 Subtle / Classic / Wide / Lush。
结尾:Width 旋钮从 0 扫到 200%,Drive 增加密度而电平不跳。

**旁白:**
"四种加宽引擎,四种性格。*Haas*,用心理声学最古老的把戏——几毫秒的先行声——把声像稳稳
推向你想要的方向。*Velvet Noise*,是 Anamorph 的招牌:它用稀疏的丝绒噪声解相关,从你的
Mid 信号中,合成出一条全新的 Side。真正的单声道进来,自然宽阔的立体声出去。而当全世界把
你的混音折回单声道时——它听到的,是那条分毫未动的中央信号。没有梳状滤波,没有意外。
*Chorus*,经典的、流动的宽度。*Dim-D*——灵感来自录音棚里传奇的维度合唱效果——每个声道用两个
反相调制的延迟抽头,让音高抖动自我抵消。有空间,没有晕船。在这一切之下,是 Width——从
单声道到两倍宽;还有 Drive——最多二十四分贝的饱和,增加的是密度,而不是响度的把戏。"

**关键信息:** 四种引擎各有音乐动机;默认引擎即"单声道安全"的主角。

---

### [02:55–03:55] — 外科手术级的宽度:分频段 + 示波器

**画面:**
窗口向下展开进入 Advanced 模式。MULTIBAND 编辑器:实时频谱之上,拖动一个分频点——相邻
分频点被推开、松手后弹回;按住时浮现带通预览曲线。在空隙处一点:新增一个分频。逐段拖动
宽度线。按住耳机图标:单独检听一个频段(字幕:**检听,不改变任何处理**)。把低频段的宽度线
拉到单声道——示波器下部收窄,上部依然宽阔。相关表在 +1 附近游走,短暂下探时以暖色提示,
随即恢复。

**旁白:**
"真实的混音,不需要'一个宽度'——它需要每个频段都有*正确的*宽度。Anamorph 提供最多四个
相位相干的分频段宽度控制,建立在 Linkwitz-Riley 分频与全通补偿之上——各频段重新合成时
完全平直,零附加延迟。在实时频谱上直接拖动分频点。打开高频的空气感,收紧中频,把低频
牢牢锚定在单声道——那才是它该在的地方。按住耳机检听任何一个频段——检听是纯粹的监听,
永远不会改变你的处理。而这一切进行时,钻石示波器始终说真话:垂直,是单声道;水平,
是宽度。相关表会在单声道播放暴露问题之前——先提醒你。"

**关键信息:** 分频段精确控制且保持相位相干;仪表让"安全"看得见。

---

### [03:55–04:45] — 诚实的工作流

**画面:**
顶栏特写。A/B 胶囊按钮切换——音频无缝延续(波形可见,无断点)。LEVEL MATCH 亮起:读数
稳定;一键 APPLY 写入增益。撤销一步回退整个旋钮手势。预设步进器走过 *Vocal Air*、
*Drum Spread*、*Wide Master*。动态大字:**更响,不等于更好。只是更响。**

**旁白:**
"Anamorph 为诚实的判断而设计。Level Match 采用广播级响度测量,把 A 与 B 保持在同一感知
响度——'更宽'休想借着'更响'来骗过你的耳朵。按住比较,相信你的耳朵,再一键写入增益。
A 与 B 不只是两组参数快照——它们是两个完整的工作区,各自记住自己的预设、自己的响度匹配、
自己的一百二十八步撤销历史。而每一次切换——预设、A/B、撤销、旁通——都天生无爆音:声音
短暂回落到*干声*,而不是静音。你的工程不会'啪'的一声,你的耳朵不会被吓一跳。"

**关键信息:** 工作流的意义是保护判断力;"顺滑"是听得见的品质。

---

### [04:45–05:55] — 玻璃之下:工程实力

**画面:**
风格一转:深色工程蒙太奇。抽象的信号链路图缓慢掠过(不出现代码特写)。Linux / Windows /
macOS 三个平台剪影依次点亮。动态大字逐条出现:
**914 项自动化检查** · **pluginval 最高严格度——每一次构建,三大平台** ·
**Mix 0% = 逐位精确的零差** · **零延迟(直到你真正需要)** ·
**工程文件,永不失效** · **0 账号,0 联网,0 日志。**
序列结尾归于安静:一个网络监视器的画面,Anamorph 正在播放,流量曲线是一条平线。

**旁白:**
"在玻璃面板之下,Anamorph 以'值得托付职业生涯'的标准来打造。音频线程经过逐个模块的审计:
不分配内存、不加锁——实时安全是一份成文的铁律,不是一种感觉。延迟为零——除非过采样真的有
非线性运算要做;而一旦启用,它是最小相位、完整上报、并且锁定的——绝不会在音符中途跳变。
每一次构建,都必须通过九百一十四项自动化检查,以及行业插件验证工具的最高严格度测试——
两种模式、各三轮——在 Linux、Windows 与 macOS 上全部通过。我们优化引擎时,只有当输出被
证明逐位一致,改动才被允许合入。参数标识是被冻结的契约——历史版本的工程与预设,永远可以
打开。还有:Anamorph 不收集任何数据,不发送任何数据。没有账号,没有激活服务器,没有
遥测,甚至没有日志文件。不必相信我们的说法——接上网络监视器,亲自验证。"

**关键信息:** 用平实的语言讲清"可验证的严谨"——中文专业听众最吃"实测、可查证"这一套。

---

### [05:55–06:55] — 真实场景

**画面:**
四段快节奏小片段,每段 12–15 秒,均以示波器 + 相关表的"验证镜头"收尾:
1. *人声* —— 主唱人声;Advanced 模式;仅高频段轻微加宽(*Vocal Air*);人声获得空气感,
   中央依旧坚如磐石;单声道检听——完好无损。
2. *声音设计* —— 单声道合成器戳音;*Mono To Stereo* 预设;扫动 Velvet 密度;再叠一层
   Dim-D 的流动;一个音符变成一片风景。
3. *鼓组总线* —— *Drum Spread*;吊镲的房间感向两侧呼吸展开;随后 *Bass Guard*:低频段
   钉在单声道;示波器上,底鼓稳居正中央。
4. *母带* —— 一首完成的混音;*Wide Master*;极轻的用量;Level Match 开启;等响度 A/B;
   相关表始终健康;工程师满意地点头。

**旁白:**
"在人声上——打开辅音之上的空气,而中央固若金汤。在单声道合成器上——一个预设,一条旋律线
变成一片风景……折回单声道,依然完美。在鼓组总线上——把房间感铺开,把底鼓锁在大地的中心。
在母带上——一丝若有若无的宽度,等响度对比,相关表全程健康……这种'变宽',只有在你关掉它的
那一刻,才会被察觉。"

**关键信息:** 四类用户各取所需;每段都以"验证"收尾,持续强化信任主题。

---

### [06:55–07:20] — 结尾:愿景

**画面:**
一切隐去,只剩示波器。轨迹收窄成开场那条垂直的单声道直线——随后最后一次绽放,化作宽阔而
稳定的钻石,并定格。标志:**ANAMORPH** —— *by RollyTech*。主题句浮现。最终画面:发售信息
占位卡。

**旁白:**
"立体声声场,从来不是一种特效。它是一个空间——你的音乐栖身的地方。尽管把它做到你敢想的
宽度……然后*确知*——而不是祈祷——它经得起考验。Anamorph。可以验证的宽度。"
*(结尾卡:发售时间与渠道待定,上线前替换。)*

**关键信息:** 愿景 + 记忆点。全片最后一个画面,是产品用自己的仪器为自己作证。

---
---

# Part 6 — Production notes (compliance & demo guardrails)

Binding constraints discovered in the repository. The finished video must respect all of them.

**Status / availability**
- v0.9.0 is pre-1.0, internal-testing; no release tag cut, nothing for sale, no license in
  force. All availability/pricing lines are placeholders. Never say "available now."
- Do not show or imply install flows as frictionless: builds are not code-signed (Windows
  SmartScreen) or notarized (macOS Gatekeeper) yet.

**Claims discipline**
- Never print ™ or ® next to Anamorph or RollyTech (`TRADEMARKS.md`).
- Do not name Roland in narration; "inspired by the legendary studio dimension expander" and
  the product's own control name "Dim-D" are the approved framings (open naming-review item).
- Velvet Noise mono claim: say "mono playback hears the untouched center" — not
  "bit-identical mono sum" (the suite asserts RMS-level equivalence, not bitwise).
- Never blanket-claim all four algorithms are mono-safe: Haas is precedence delay and combs
  in mono by nature (the manual itself warns this). Scenario demos use Velvet/Dim-D/Chorus
  for mono-fold moments.
- No CPU %, RAM, or instance-count claims of any kind — no committed benchmark exists and
  inventing numbers is prohibited (PERFORMANCE_BUDGET.md constraint C2). The optimization-wave
  figures (−39% engine floor, −92% decay tick, etc.) are session-local dev measurements: usable
  in a "development journey" context if labeled as such, but they are **not** product specs —
  this script deliberately keeps them out of the voice-over.
- pluginval phrasing: "maximum strictness, both modes, three passes each, on all three
  platforms" (Windows CI skips pluginval's GUI tests on the GPU-less runner — never say
  "every pluginval test on every platform").
- DAW support: no host is formally verified (KI-004); show a generic/neutral session
  environment, don't claim "tested in Logic/Live/Cubase." AU exists but is not
  auval-validated (KI-014) — don't spotlight "certified AU."
- Formats: VST3 (Linux/Windows/macOS), AU (macOS), Standalone. **No AAX / Pro Tools.**
  Output is always stereo; mono→mono unsupported. No MIDI.

**Demo capture guardrails**
- There are zero media assets in the repo (no logo, screenshots, or demo audio) — every frame
  must be captured from a real build of the product.
- Drag crossovers smoothly on camera; do not showcase violent flicks (KI-012: fast drags
  carry a small bounded FM by design; normal drags track 1:1).
- Do not demo: switchable skins (PR #88 closed unmerged — one skin exists), free-form window
  resizing (five stepped sizes only), typed-value-then-undo (typed entries create no undo
  step, KI-010), tooltips without first enabling them in Settings (off by default).
- Windows Standalone has no ASIO — use the VST3 in an ASIO host for any Windows capture.
- The "network monitor flat line" shot is authentic and encouraged — PRIVACY.md explicitly
  invites this verification.

**Fact sources for every VO claim (spot-check map)**
- Mid/Side & mono-safety by construction → `src/dsp/MidSide.h`, `src/dsp/VelvetNoise.h`, `docs/policies/DSP_POLICY.md`
- Four algorithms & Dim-D anti-phase taps → `src/dsp/ChorusEngine.{h,cpp}`, `src/dsp/HaasProcessor.cpp`, user manual §4
- Multiband flat recombination, zero added latency → `src/dsp/MultibandWidth.{h,cpp}`, `docs/architecture/DSP_ALGORITHMS.md`, CHANGELOG [0.8.10]
- Solo touches nothing → `src/dsp/SoloMonitor.h`, ADR-0006/0014, user manual §6
- Level Match (BS.1770, no ratchet) → `src/dsp/LoudnessMatch.{h,cpp}`, ADR-0007
- Zero/latched latency → `src/dsp/AnamorphEngine.cpp`, `docs/architecture/LATENCY_MODEL.md`, ADR-0003
- Click-free & dry-filled swaps; bit-exact null/bypass → ADR-0004/0005, `tests/dsp_tests.cpp`
- 914 checks; pluginval gate; 3 platforms → README, `docs/HANDOVER.md`, `.github/workflows/build.yml`, `tests/`
- Sessions never break → `docs/policies/SESSION_COMPATIBILITY_POLICY.md`, `tests/state_tests.cpp` + fixtures
- Bit-identical optimization discipline → `docs/architecture/PERFORMANCE_BUDGET.md`, worklogs/performance/
- Privacy (0/0/0, verify it yourself) → `PRIVACY.md`, CMakeLists JUCE_WEB_BROWSER=0/JUCE_USE_CURL=0
- 10 presets & their recipes → `src/PresetManager.cpp:18-38`
- 128-step per-slot undo; A/B workspaces → ADR-0008, user manual §3.1/§7.4
