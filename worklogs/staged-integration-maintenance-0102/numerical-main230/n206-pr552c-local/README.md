# N2-06 proposed PR — local development execution

[Draft PR #244](https://github.com/skyRolly/Anamorph/pull/244) has head
`552c6fe5e7d4c6f92da0ea200adddec85e1358ae`, tree
`c6d8b3cf5710415fb31b67ddf3e36dc9e106891e`, on main `230da0e2`.
A Linux/Ninja Release development build ran the same production and test
file bytes before the commit was created; documentation and citation edits
were in progress during the build. This is not an exact-clean-HEAD or hosted
execution and is not a replacement for those checks. Results: 14 focused,
392 Multiband Enable, 720 ordinary-duck and 5628 full DSP checks passed.
The updated Enable oracle observed old Wet differences both with and without
Match; it is not a whole Level-Match algorithm oracle.

Current local documentation and citation checks, real-time and portability
lints and self-tests, and State-coverage lint and self-tests passed. The host
provenance guard attempt failed two of 22 cases because `cmake` was absent
(`n206-provenance.log`); the same 22 guards passed in the supported
Linux/Ninja container (`n206-provenance-container.log`). These outcomes are
preserved separately. Full State, hosted cross-platform/sanitizer gates and
exact-head isolated mutations remain outstanding. The new PR is not merged.
