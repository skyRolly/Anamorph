# Combined rehearsal: stale local PR ref and failed attempts

The first rehearsal `0ea2299b4f9f0ea66fffefbe27e317aad89af7de` included #243
`381c89c2` and #245 `a6b66c25`, but used stale local tracking ref #244
`cca0121b` after fetching its branch only to `FETCH_HEAD`. The actual #244 head
was `e7824d8c`; its latest test/documentation changes were missing. No PR was
merged on this basis. The 16-check expected-output marker in the independently
specified combined replay detected the mistake: the 0ea test returned **14/0**.
The preceding N2-04 focused test returned **125/0**. The fresh runner exited
1 at the missing marker; its partial receipt and raw streams are retained.
It did not run the remaining focused or full suites and is not final validation.
A separate preliminary combined-engine relink also returned 125/0 and 14/0,
then rejected the expected 16-check marker. Its production engine bytes matched
the later rehearsal, but its old test header did not.

The first fresh attempt was rejected before building because an ignored Python
cache existed in the checkout. A second was stopped after another help command
created cache files while it was building, violating the no-concurrent-writers
premise; its outer exit was 137 and it is not a valid provenance result. Their
logs are retained. No protection was relaxed.

Explicit refspec fetches established the actual remote heads, and merge commit
`b182cc0a6d68b105a0226cfcffd9fe6cfe767003` (tree
`d91c4b01c8fda95d11f09cdc4b6627d16a7a84b5`) brings #244 `e7824d8c` into the
rehearsal. Its changed files relative to 0ea are the Mix test header and two
testing/coverage documents; `src/` is byte-identical. Hosted runs for 0ea were
superseded/cancelled, and new exact-source checks were requested for b182.
The new focused/full specification and mutation packets use b182; they are
not results merely because they exist. Main remains `230da0e2` at this entry.
