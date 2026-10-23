# Installed macOS receipt validation — 2026-10-24

Starting main `e76a4a66e6ed50f3ade53be9717c2c002865e3c4`; #170/#171/#172 and owner
approval reconciliation #173 are merged. ADR-0063/0064 are Accepted. This extends
validation of their existing identities and packaging, with no new product dependency,
CMake/pin/format/state/identity decision or receipt migration. No new Architecture Review
Gate item identified. Human PR review and merge authorization remain separate.

## Choice and safety

Use the existing disposable GitHub-hosted macOS packaging VM and real native `installer`
and `pkgutil`; no mock installation, uncontrolled Mac, shared host, custom target-volume
package semantics or real user's folders. The strict environment guard requires this
repository, macOS and GitHub-hosted execution. Existing canonical product receipts and
system bundle paths cause refusal. No receipt forgetting, wildcard removal or global
plug-in cache clearing. Keep installed payload/receipts for inspection; VM disposal is
cleanup. Installer upload now requires this check; failed evidence is retained.

The product package defaults to all three components. Measure actual receipt IDs,
versions, installation volume/location, file lists, installed CFBundleIdentifier,
AU `aufx/Anmr/RTec`, and exact staged/installed file bytes. Assert exactly the three
expected receipt additions and preserve every previous receipt plus sibling metadata
and any existing sibling bundles. If no sibling receipts exist, record zero explicitly;
this is not both-products co-installation evidence. Selected-component permutations,
older-install upgrades, GUI caches and real DAWs remain outside this full-install smoke.

## Local parser evidence

```sh
python3 scripts/test-macos-receipts.py --self-test
python3 scripts/check-docs.py --self-test
python3 scripts/check-docs.py
python3 scripts/check-citations.py --check --base e76a4a66e6ed50f3ade53be9717c2c002865e3c4
```

Self-test: three valid components, 45 specific bad metadata/ownership/payload/separation/
guard probes. Three independently run mutants disabling receipt identity, named bundle
ownership or staged-payload equality fail their targeted negative fixture. The foreign
file probe adds an extra bundle to an otherwise complete valid receipt, so a missing
required-file rejection cannot stand in for the ownership gate.
Native installation is pending until current-head CI completes; no Linux runtime pass
is implied. The same job runs the self-test before installation. No audio/GUI/state,
existing tests, version, artwork, namespace, AppId or AU/VST3 identity changes.
No release/tag, merge, deployment or ADR acceptance is performed.

## Native result and citation correction

At `fd1941a4dd4e9e31af2c31aaf8c3505f97d24360`, macOS job 120707087674 in
run 40039366516 passed actual default-full installation. Artifact 12539936807,
ZIP SHA-256 `427ec934e9813a771e707b4c51af25a7ee325a69ad975d45e1d12b91ee62cc8c`,
records failure=null, exact tech.rolly.anamorph.vst3/au/app receipts, version 0.9.10,
root volume and correct relative pkgutil locations, installed bundle/AU identities
and payload equality. Existing receipts survive; sibling count is zero, so this
is not a both-products co-installation result.

The first implementation commit shifted four build-workflow citations; they were
corrected against main in the next commit. That push's citation job compared with
the intermediate uncorrected commit and correctly remained red. This follow-up
records the failure and actual native evidence; no checker/gate is weakened or
exception added. Local checks against both main and the immediate good predecessor
pass. The receipt script/workflow/native implementation is unchanged by this follow-up;
new combined-head CI must still complete before it is called green.
