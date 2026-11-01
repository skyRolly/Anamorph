# NR-017 — Static diagnostic baseline and dispositions

## Current integrated-main delta — 2026-11-02 (Asia/Shanghai)

The historical 257-record CSV below remains unchanged and source-scoped to`4c13688`.
Actual integrated main`4255393` has **80 raw diagnostic records** in digest-verified
current-head artifacts: CodeQL C++65 (all exactly match the prior rule/URI/line/
message), Actions0, PREfast15. Primary locations split14 first-party tests /
66 dependencies /0 first-party runtime source. This is a row inventory, not a bug,
severity or open-alert count. [Current artifacts, hashes and PREfast rows](nr014-rotary-accessibility/current-analyzer-delta.json).

PREfast retains three C6262 test stack estimates, four constexpr suggestions and
eight initialization observations (seven test, one dependency). Existing scoped
heap fixtures eliminate most prior test stack reports; this does not establish177
fixed product defects. One remaining inline `authoring` processor in State36
accounts for its 275284-byte estimate; other estimates are19136/20528. All are below
the unchanged 1 MB runtime stack cap, and native full-suite gates pass. Keep these
bounded harness observations unless a gate/recurrence demonstrates a need; no
assertion, diagnostic, baseline or detector is suppressed. Existing test aggregate/
explicit field assignments and the prior dependency caveat remain the disposition
basis for initializer/constexpr reports; no first-party runtime defect is asserted.

The connector fetch route does not support the alert endpoint (HTTP400 validation,
not a GitHub alert result); direct authorized GitHub CLI refresh returns **HTTP403**,
“Resource not accessible by integration.” Alert state remains unavailable. Analyzer
success and the smaller row count do not imply zero open alerts or security clearance.
Subsequent PRs require their own actual-head analyzers. Dated baseline/review details
below preserve their original scope.

**Recorded:** 2026-10-31 (Asia/Shanghai). **Status:** Completed inventory; review PR
pending integration. This is an engineering evidence inventory, not a zero-defect
claim or a vulnerability/severity assessment. Canonical progress is in
[NEXT_RELEASE_DEVELOPMENT_ROADMAP.md](NEXT_RELEASE_DEVELOPMENT_ROADMAP.md).

## 1. Source and provenance

Latest available main-branch analyzer runs at collection both target
`4c136881f245470577d66a7603f15b177ddcf3dc`. Main at `6ae102c` adds only merged
PR #194 documentation; source/tests/CMake/workflow inputs are identical. NR-022's
unmerged development branch is a different candidate and needs its own CI results.

| Analyzer | Run | Artifact | Independently downloaded ZIP SHA-256 |
|---|---|---|---|
| CodeQL C++ | [41167465482](https://github.com/skyRolly/Anamorph/actions/runs/41167465482) | [13030303164](https://github.com/skyRolly/Anamorph/actions/runs/41167465482/artifacts/13030303164), `cpp.sarif` | `a2921477cb673675df1f4606c973b48d8d69d361b0ec26a98b047792c4db30d0` |
| CodeQL Actions | same run | [13029512075](https://github.com/skyRolly/Anamorph/actions/runs/41167465482/artifacts/13029512075), `actions.sarif` | `9b42f4a7c3050f905f9749044eb79289b00cc10957f70c522a1c02c61f68987c` |
| MSVC PREfast | [41167465497](https://github.com/skyRolly/Anamorph/actions/runs/41167465497) | [13029797600](https://github.com/skyRolly/Anamorph/actions/runs/41167465497/artifacts/13029797600), `results.sarif` | `2afc89b5bd5b53105b81bb2886bd5c71c52cdc0d92edb8f3b0c92191e692d541` |

All three archive hashes match their GitHub API digests. The complete result
inventory is [NR017_BASELINE_DIAGNOSTICS.csv](NR017_BASELINE_DIAGNOSTICS.csv): **257
rows with unique snapshot IDs**, tool, artifact/source identity, rule, reported level,
exact URI/line/column, original message and explicit disposition. Duplicate sites
reporting different members remain distinct rows. No source positions are rewritten
to pretend this immutable snapshot came from the later NR-022 branch.

Each CSV row records the result's **primary** physical location, not every related
location, end span or code-flow element. Those remain in the original SARIF artifacts.
An empty `reported_level` preserves an absent result-level field; it does not infer
a rule default or security severity. The CSV is a primary-result inventory, not a
lossless replacement for the SARIF.

The authenticated code-scanning alerts endpoint still returns **HTTP 403,
“Resource not accessible by integration”**. Therefore open/fixed/dismissed alert
state is **unknown**. SARIF rows are analyzer observations, not a count of open bugs.
No alert is dismissed, fixed or suppressed by this document.

## 2. Counts and coverage boundaries

Evidence [Verified]: downloaded SARIF and baseline workflow configuration.

| Tool / rule | Count | Reported ownership / disposition |
|---|---:|---|
| CodeQL `cpp/comparison-with-wider-type` | 1 | JUCE dependency; retain report, reachability/severity unassessed |
| CodeQL `cpp/unsafe-use-of-this` | 1 | JUCE dependency; same disposition |
| CodeQL `cpp/alloca-in-loop` | 7 | JUCE ALSA/MIDI dependency paths; same disposition |
| CodeQL `cpp/integer-multiplication-cast-to-long` | 56 | JUCE/vendored codec/graphics dependencies; same disposition |
| CodeQL Actions | 0 | Zero results in this artifact, not a universal workflow-safety claim |
| PREfast `C6262` | 180 | Test functions: 127 state / 53 DSP; retain as stack estimates requiring concrete frame/exposure evidence |
| PREfast `C26495` | 7 + 1 | Seven test construction-style reports and one JUCE VST3 wrapper report; source context inspected below |
| PREfast `C26498` | 4 | Two DSP / two state test const-versus-constexpr style reports; retain |

Totals: CodeQL C++ **65**, Actions **0**, PREfast **192**. Across nonzero results,
**191 locations are first-party tests, 66 are dependencies, zero are first-party
`src/` runtime locations**. This describes the artifacts, not program correctness.
Counts happen to match the prior P1 record; they were remeasured, not copied as
today's alert state.

CodeQL manually builds/extracts the VST3 and DSP suite, not the state suite.
Its `paths-ignore` does not remove dependencies from that compiled extraction.
PREfast's CMake-codemodel analysis reaches the state target too, including source
it did not compile in its ordinary Build step. Its ignored include/target paths
cannot suppress the JUCE VST3 wrapper translation unit inside a first-party target.
See `.github/workflows/codeql.yml`, `msvc.yml` and TESTING's raw-SARIF section.
The scanner coverage difference must not be interpreted as contradictory results.

## 3. Source-context review and selected work

### Test stack estimates — C6262

All 180 reports name tests, not a shipping processing function. The original
messages/positions remain in the CSV; the largest-looking source number is not
accepted as a measured stack frame. Existing TESTING/CI_CD records already explain
PREfast's estimates and the separate 1 MB-stack/native/sanitizer gates. Those gates
passed on the baseline; that does not prove every unexecuted branch fits a stack.

**Disposition: retain / investigate on a concrete failure or new diagnostic delta.**
Do not mechanically heap-convert every test, widen a warning baseline, or mark the
reports false solely because CI passes. A selected cleanup needs target-toolchain
frame/call-depth evidence and a stack-limit regression or equivalent discriminating
control. NR-022's new test uses heap-owned processors and needs its own fresh CI.

### Test construction reports — C26495

Reviewed the exact baseline definitions and their uses in `tests/dsp_tests.cpp`:

- `testForcedSwapNoDropout::Scenario` (line 1610): both array elements receive
  `name` and `minRatio` assignments before the later iteration. Engine parameter
  members have their declared defaults. The report does not establish an actual
  read-before-initialization in this sequence.
- `testRapidForcedSwapDryFill::Case` (line 1732): `Case cases[4] {}` value-initializes
  scalar members; each scenario then supplies its name/overrides. Missing explicit
  assignments to false-valued flags are not evidence of indeterminate values.
- `runForcedSwapAuditProbe::Case` (line 14152): the builder assigns `name`, `from`
  and `to` before pushing the object into the vector used by the audit.

These seven construction/design diagnostics do not demonstrate a behavioral defect
on the reviewed paths. **Retain without suppression or unrelated initialization
churn.** Reopen if a changed path actually reads a member before setting it.

The eighth C26495 is in pinned JUCE 9.0.4's
`modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp`, `Submenu::enabled`
(line 1826). The local construction uses `vector<Submenu>(1)` and explicit aggregate
members on group pushes. This is dependency code; no first-party wrapper patch or
JUCE pin upgrade is selected. The observation is not a blanket exoneration of host
menu lifetime or of other dependency reports.

### Constexpr recommendations — C26498

The four messages concern `numeric_limits<float>::min`, `quiet_NaN` or `infinity`
assigned to const test values (DSP lines 3770/3930; state lines 19300/19301).
The recorded issue is a constexpr-style recommendation; no wrong value, state
transition or audio consequence is established. **Retain as-is.** A larger semantic
test change may naturally revisit the declarations, but no standalone fix is justified.

### Dependency CodeQL reports

All 65 retain their exact rule/location/message in the CSV. **Keep visible; do not
filter, dismiss, relabel as fixed or upgrade the dependency merely to lower a count.**
Reachability and security severity have not been validated here. A credible affected
host/input path or a new report with demonstrated product impact should become a
separately scoped investigation. A pin/toolchain/build-topology change follows its
normal ADR and validation requirements.

No currently validated new first-party runtime defect is produced by this inventory.
This is a disposition of available engineering evidence, not a claim that the source
or dependencies contain no vulnerabilities.

## 4. Adjacent validation facts and remaining limits

- Clang/GCC warning baselines, query configuration, ignored paths, annotations and
  compiler gates remain unchanged. This CSV is not a new automated allowlist.
- The existing Python invalid-escape SyntaxWarnings in check-docs.py are unchanged;
  both checker invocations succeed. They are not new SARIF findings.
- NR-022's local GCC 14 VST3/Standalone require CXXABI_1.3.15 above the shipping
  1.3.14 floor. That rejected local artifact is not evidence that the pinned-Clang
  release floor should rise. Its source-head pinned-Clang Linux job in run
  41216103432 passes the existing ABI check for both artifacts (CXXABI_1.3.9,
  GLIBC_2.38, GLIBCXX_3.4.31); the other required platform/analyzer jobs remain
  separate. See the roadmap's dated source-CI checkpoint.
- Stacked PR #196 targets a feature branch, so its CodeQL/MSVC workflows are not
  selected automatically by their main-base filter. Their existing manual workflows
  were invoked against the exact `fix/nr022-ab-pending-undo` ref (`4a431ca`) without
  changing configuration. Native Build jobs run separately. This baseline inventory
  does not substitute for those new-code validation results.
- Obtaining GitHub alert state still needs suitable owner/integration access. No
  bypass or permission change was attempted. The explicit access limit does not
  prevent completing this raw-output inventory, as NR-017's acceptance allows.

## 5. Completion and next execution

NR-017's inventory acceptance is met: provenance-bearing artifacts/digests, all
reported locations, ownership/rule groups, source-context dispositions and explicit
limits are recorded. No bug, suppression or numerical threshold was modified.
Human review can select further work on demonstrated impact; there is no justified
blanket warning-cleanup PR.

After explicitly authorized integration and repository reassessment, the recommended
first engineering batch is NR-011's bounded differential transition-test increment,
with liveness/old-new comparison controls; NR-010 measurements need a
quiet, identified machine before numeric budgets are asserted. NR-006's new
handoff contract remains a separate architecture review. NR-003 stays Deferred.

## 6. Independent review and reproduction — 2026-11-01 (Asia/Shanghai)

**Disposition: retain the inventory and existing diagnostic dispositions; clarify
the export boundary and make identity verification reproducible.** Independently
downloaded all three original archives again and matched every primary result tuple,
including multiplicity, against all 257 CSV rows. Rechecked the 127 state/53 DSP
stack reports, seven first-party construction reports, four constexpr recommendations
and JUCE wrapper context against the exact baseline/pinned dependency. No new
behavioral defect or blanket cleanup is justified by that evidence. Keep dependency
reachability/severity unassessed and reopen on demonstrated impact or a new delta.

The alerts API was retried through the authenticated CLI because the connector's
general fetch tool excludes that endpoint: HTTP 403, “Resource not accessible by
integration”. Open/fixed/dismissed state remains unknown. The connector successfully
downloaded the artifacts; this environment's direct CLI artifact redirect returned
HTTP 403, a transport limitation rather than absent SARIF evidence. No permission
or scanner configuration was changed. Historical pending CI text above is a dated
checkpoint; current stack-head outcomes are in the roadmap and PR handoff ledger.

To reproduce, download the three linked **ZIP archives** into one directory as
`13030303164.zip`, `13029512075.zip`, `13029797600.zip` using authenticated Actions
artifact access. From the repository root, run the following stdlib-only check with
that directory as its argument. Archive hashes also match the GitHub artifact API's
digests. This check intentionally preserves primary locations/messages exactly,
including blank result levels and duplicate result tuples; classification and
disposition still require the source review described above.

```sh
python3 - /path/to/downloaded-zips <<'PY'
import collections, csv, hashlib, json, pathlib, sys, zipfile

archives = {
    '13030303164': ('a2921477cb673675df1f4606c973b48d8d69d361b0ec26a98b047792c4db30d0', 'CodeQL C++'),
    '13029512075': ('9b42f4a7c3050f905f9749044eb79289b00cc10957f70c522a1c02c61f68987c', 'CodeQL Actions'),
    '13029797600': ('2afc89b5bd5b53105b81bb2886bd5c71c52cdc0d92edb8f3b0c92191e692d541', 'MSVC PREfast'),
}
rows = list(csv.DictReader(open('worklogs/NR017_BASELINE_DIAGNOSTICS.csv', newline='')))
assert len(rows) == len({r['id'] for r in rows}) == 257
assert {r['artifact_id'] for r in rows} <= archives.keys()
fields = ('rule', 'reported_level', 'source_uri', 'start_line', 'start_column', 'message')
for aid, (digest, tool) in archives.items():
    path = pathlib.Path(sys.argv[1]) / (aid + '.zip')
    assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
    expected = []
    with zipfile.ZipFile(path) as archive:
        names = [n for n in archive.namelist() if n.endswith('.sarif')]
        assert len(names) == 1
        for run in json.loads(archive.read(names[0]))['runs']:
            for result in run.get('results', []):
                assert len(result['locations']) == 1
                loc = result['locations'][0]['physicalLocation']
                region = loc.get('region', {})
                expected.append((result.get('ruleId', ''), result.get('level', ''),
                    loc['artifactLocation']['uri'], str(region.get('startLine', '')),
                    str(region.get('startColumn', '')), result['message']['text']))
    actual = [r for r in rows if r['artifact_id'] == aid]
    assert all(r['tool'] == tool and r['source_commit'] ==
        '4c136881f245470577d66a7603f15b177ddcf3dc' and r['review_disposition'] for r in actual)
    assert collections.Counter(expected) == collections.Counter(tuple(r[f] for f in fields) for r in actual)
    print(aid, len(actual), 'primary results verified')
assert collections.Counter(r['ownership_scope'] for r in rows) == {
    'first-party tests': 191, 'dependency': 66}
PY
```
