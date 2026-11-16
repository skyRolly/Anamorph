# N2-04 final v6 fresh replay

Fresh Linux/Ninja Release execution on exact draft PR #243 head
`6a65b1d18a992c738b3f004f7690ea87d510bd40`, tree
`380f1a039eeb9deb1f1c7c28736a0c726dde30fa`, with effective pinned JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd` and specification commit
`34af3f05122d08e2dffed403167f14c5caf05a69`. The committed receipt binds
the checkout, dependency, build inputs, fresh objects, executables, commands
and outputs within the documented Linux/Ninja trust boundary. The outer
runner exited 0; all three executable exits were 0 as specified.

- Focused v6 conditioned-history selector: **125 checks, 0 failures**.
- Full DSP suite: **5,739 checks, 0 failures**.
- Full State suite under Xvfb: **16,555 checks, 0 failures**. The expected
  internal fixture-negative controls in the State log are not suite failures.

This is exact-head local execution, separate from hosted CI and from earlier
v5 controls. Full outputs and the build log are retained compressed; stdout
content hashes in `identity.json` refer to the original uncompressed bytes.
The v6 receipt-controlled positive and isolated mutations must execute serially
against this build in separate output directories; they are not implied here.
The PR remains subject to owner review and independent merge authorization.
