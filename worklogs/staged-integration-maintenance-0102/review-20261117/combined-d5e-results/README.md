# Disposable combined source: fresh Linux execution

Fresh Linux/Ninja Release execution used combined commit
`d5e38a534cb2252476b0569ae6fda9ae66074dbb`, tree
`65938e2ca5f0068f2ed404870a88a5d4da5673f4`, pinned JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd` and specification commit
`a66dae8b3188e9e38f9ec6f9d7f42500aac9a71e`. The compiler was Debian GCC
14.2.0 in the recorded container. The outer runner exited 0. All seven
executable invocations exited 0 and met their declared markers; an additional
[companion check](companion-validation.json) rehashed every captured stdout and
stderr and checked each recorded exit and marker.

| Selection | Checks | Failures |
| --- | ---: | ---: |
| N2-04 | 125 | 0 |
| N2-06 | 16 | 0 |
| N2-07 | 20 | 0 |
| N2-03 | 392 | 0 |
| N2-02 engine | 720 | 0 |
| Full DSP | 5,775 | 0 |
| Full State | 16,555 | 0 |

The DSP executable SHA-256 is
`f662a96383728f925bfea07a1e7eb0c8259eaa4a892f37e0272f6504e6361330` and the
State executable is
`cbcdb214e6adaac57eaa505675463cdea94e914682e6c3612493e33a68d6d0fa`.
This is a local execution on a disposable integration source, not actual
merged main. Hosted Build & Validate, CodeQL and Microsoft analysis on this
commit are separate checks and were pending when this file was written.
Earlier failed/cancelled attempts remain in the adjacent records.
