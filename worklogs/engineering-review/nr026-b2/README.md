# B2 source-bound implementation evidence

The implementation source checkpoint is `f9d8cbd380cc01fc967ae0cae195092001ed00de`,
based on integrated #221 main `b9f94d83`. ADR-0072 B2 is owner-approved; this bundle
does not approve the production PR, integrate it or authorize release. Original
NR-026 and rejected B1 records are separate and unchanged.

Run `python3 worklogs/engineering-review/nr026-b2/verify.py --self-test` to verify
the retained payloads and three deliberate corruption controls. This checks the
bundle, not current runtime correctness. `execution.json` distinguishes the local
partial ASan build, full/focused ordinary suites, preliminary troubleshooting and
required final-head CI. `build.json` and the executed scripts retain exact absolute
paths/arguments; source copies and build paths must be re-aimed for reproduction.

The partial ASan build instruments all five reached JUCE gui_basics units plus
processor, editor, LookAndFeel and the actual-component probe. Other linked objects
are not instrumented; it is not full UBSan/Valgrind or a native host result. Seven
mutants use copied product source, the same executed test header and detector
options. Six fail gesture/history/anchor assertions with clean ASan return; removing
press staging causes a required ASan use-after-free. The unmodified source then
passes 2,115 checks. Patches, compiler/link commands, binary and stream identities
are retained. Never apply these intentionally wrong patches to shipping source.

For ordinary/focused reproduction, check out that immutable source with official
JUCE 9.0.5 and build the existing State target. The State executable at this base
predates integration of #219: use a disposable verified profile/temporary environment.
For example, inside the recorded Linux container after building:

```sh
b2_profile=$(mktemp -d)
mkdir -p "$b2_profile/home/.config" "$b2_profile/tmp"
env HOME="$b2_profile/home" XDG_CONFIG_HOME="$b2_profile/home/.config" \
  TMPDIR="$b2_profile/tmp" xvfb-run -a \
  /path/to/AnamorphStateTests --knob-lifetime-probe
```

This environment prescription is Linux-specific. HOME alone does not redirect
Windows or macOS's production preset path; their evidence comes from disposable
hosted CI. Do not weaken the original NR-026 runner's immutable source guard to run
B2. Each new execution needs its own source/build/result identity.

`bank-comparison.json` keeps distinct observations: original Wheel source preserves
the host-delivered 0.8 anchor but the observed Redo is 0.8; the first B2 staging
candidate resumes at 0.53; corrected B2 preserves the 0.8 anchor and the owned 0.53
Redo endpoint. Original and initial-candidate exit codes use the combined new oracle,
so they must not be relabeled as overall passes. No native DAW incidence is measured.
