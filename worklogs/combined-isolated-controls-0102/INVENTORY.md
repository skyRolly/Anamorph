# Replay and evidence entry-point inventory

This is the 2026-11-14 source review of the 0.10.2 evidence stack. Historical
logs and inputs remain unchanged. A data comparator can verify numerical inputs
and formulas, but cannot identify the executable that generated those inputs.

| Entry points | Classification and present limitation |
| --- | --- |
| `dsp-numerical-audit-0102/rebuild_probe.py`, `replay_provenance.py` | Maintained historical baseline replay, with raw-tree and fresh-build guards. It does not cover every other experiment. |
| `dsp-numerical-audit-0102/replay_experiment.py` | Shared fresh-build runner for a committed experiment specification. A reviewed purpose and trusted local tools remain premises. |
| `dsp-numerical-audit-0102/relink_verified.py` | Controlled relink against a committed fresh-build receipt; object, source, dependency, rule and command checks are required. Its supported one-TU replacement does not authorize arbitrary object reuse. |
| `combined-isolated-controls-0102/run_controls.py` | Shared-guard N2 control runner with a fresh base build and engine-only reviewed patches; 15 recorded selector outcomes are in `results-f268/`. The header clamp needs a separate fresh build. |
| `n2-controls-0102/run_controls.py` | Existing portable execution helper; unchecked source, build and objects. Historical comparison replacing the entire engine also reverts unrelated changes on later combined sources. Not yet migrated. |
| `n2-endpoints-0102/build_probe.py`, `make_controls.py` | Existing execution/relink helpers; unchecked objects, source, headers and probe inputs. Not yet migrated. |
| `h4-dry-state-0102/build_probe.py`, `make_recovery_trace.py`, `relink-final-endpoint-controls.py` | Existing H4 diagnostic builders and relinker; dirty-source and reused-object identities are incomplete. Not yet migrated. |
| `h4-dry-state-0102/combined-controls.py`, `combined-final-controls.py` | Historical, hard-coded one-off relink records; do not establish object provenance or portable replay. |
| `h4-dry-state-0102/run-counts*.py`, `run-glide-counts.py`, `run-guard-bench.py`, `run-thread-bench.py` | Historical, hard-coded workload/measurement command records. They invoke pre-existing binaries and do not verify their source or object provenance. Their instruction, wall and thread-time results remain distinct. |
| `h4-dry-state-0102/verify_oracle.py`, audit `original/compare*.py`, `test25-oracle.py` | Data comparison/oracle programs. They validate their supplied data under their own premises, not the producing executable. |
| Audit `original/*.sh`, `original/make-variants.py`, archived `.cpp`, reports, manifests and compressed raw logs | Preserved original source and historical command records; not maintained provenance guards. Do not rewrite them to match new replays. |
| `h4-review-followup-0102` witness C++ and candidate patches | Nonshipping diagnostic inputs. Earlier relinks depend on their recorded unchecked-object premises; new committed specifications are necessary but not proof of execution. Probe-local headers/stubs are executable inputs when used and must be selected from a committed packet. |

R-237-01 remains open until each maintained entry point is migrated or
explicitly superseded by a usable verified procedure, including real candidate,
combined and isolated mutation runs. A historical command's status does not
become a current provenance guarantee merely because it is executable.
