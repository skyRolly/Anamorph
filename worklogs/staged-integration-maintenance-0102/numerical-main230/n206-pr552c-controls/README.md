# N2-06 PR 552c6fe5 isolated mutation controls

These eight serial receipt-controlled relinks used the exact committed test
header bytes from PR #244 head `552c6fe5e7d4c6f92da0ea200adddec85e1358ae`,
actual-main `230da0e2`, fresh baseline receipt `31bf22e2` and control spec
`ca997aeddcad85386d27f8b56678b45947a96cde`. SHA-256 comparison matched
both committed test headers to that PR head. The candidate and mutations
change only the engine translation unit against that base; this is not a
fresh build of the complete PR tree. Receipt identities, compiler inputs,
patches, commands and outputs are retained under the documented trust model.
Each outer runner exited 0.

| Control | Executable exit | Checks / failures |
| --- | ---: | ---: |
| Unmodified main | 1 expected | 14 / 3 |
| PR engine | 0 | 14 / 0 |
| Restore settled Mix call sites only | 1 expected | 14 / 3 |
| Restore smoothing Mix call sites only | 1 expected | 14 / 1 |
| Select Wet even when dry is non-finite | 1 expected | 14 / 1 |
| Alter interior Mix arithmetic | 1 expected | 14 / 1 |
| PR engine, N2-03 oracle | 0 | 392 / 0 |
| Restore only N2-03 Wet call sites with N2-06 preserved | 1 expected | 392 / 44 |

The settled mutation also affects later settled blocks in the ramp test; its
three failures are not three independent production mechanisms. Expected
negative exits are not product test failures. The MSVC error subsequently
found in the test access helper applies to this historical PR head, not the
production engine; a typed helper correction requires new-head validation
and fresh controls with the new header bytes.
