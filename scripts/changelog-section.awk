# changelog-section.awk -- extract one CHANGELOG.md release section, verbatim.
#
#   awk -v ver=0.9.7 -f scripts/changelog-section.awk CHANGELOG.md
#
# Prints from that version's own `## [` heading (heading included) to the next
# `## [` heading, or to EOF when it is the oldest entry. Prints NOTHING if the
# version has no entry -- which is what makes it usable as a fail-closed test as
# well as a producer: `release.yml` runs it twice, once in `validate` to reject a
# tag whose section will not extract, and once in `draft-release` to build the
# published notes. ONE implementation, so the check and the thing it checks
# cannot drift apart; `RELEASE_PROCESS.md` states that they are the same pass.
#
# `index(...) == 1` is an exact prefix compare: version dots are not treated as
# regex.
#
# A FENCED BLOCK IS DATA, NOT STRUCTURE, and that is what the fence tracking
# below adds. Without it, a `## [x.y.z]` line inside a fenced sample -- an entry
# template in the preamble, a quoted diff, a worked example -- would start the
# extraction there, and a fenced `## [` line inside a real entry would cut that
# entry short. Neither exists in CHANGELOG.md today (it currently carries no
# fenced blocks at all), so this changes nothing about the notes any current
# version produces; it stops the first fenced sample anyone adds from silently
# corrupting them. This is the other half of a contract `scripts/check-docs.py`
# gates: below the first entry, every `## ` heading must be a `## [` entry
# heading, or a section lands in the published notes of a release it does not
# belong to.
#
# WHAT CLOSES A FENCE (CommonMark section 4.5), and all three clauses earn their
# place -- an earlier version of this tracker had only the first and got nested
# blocks wrong:
#   1. the SAME character. A `~~~` line inside a ``` block is data.
#   2. at least AS LONG as the opener. A ``` line inside a ```` block is data --
#      which is exactly how a Markdown example that itself contains a fence is
#      written.
#   3. NOTHING BUT TRAILING WHITESPACE after the run. A line carrying an info
#      string is an OPENING fence and can never be a closer, so a nested ```cpp
#      inside a ```markdown example does not end it. With only clause 1 it did:
#      the example's body was then scanned as real structure and the real closer
#      re-opened a block, inverting the mask from there to EOF.
# Up to three leading spaces are allowed on either delimiter, matching
# `check-docs.py`'s own FENCE pattern; four or more is an indented code block,
# not a fence, so such a line falls through to the rules below as ordinary
# content.

/^[ \t]*(```|~~~)/ {
    fl = $0
    if (match (fl, /[^ \t]/) <= 4) {                 # <= 3 leading blanks
        sub (/^[ \t]+/, "", fl)
        fc = substr (fl, 1, 1)
        n  = 0
        while (substr (fl, n + 1, 1) == fc) n++
        rest = substr (fl, n + 1)
        if (! fence)                                            { fence = 1; f = fc; w = n }
        else if (fc == f && n >= w && rest ~ /^[ \t]*$/)        { fence = 0 }
        if (on) print
        next
    }
}
fence                          { if (on) print; next }
index($0, "## [" ver "]") == 1 { on = 1; print; next }
on && /^## \[/                 { exit }
on                             { print }
