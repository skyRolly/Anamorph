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
#      A CARRIAGE RETURN COUNTS AS TRAILING WHITESPACE. `check-docs.py` reads the
#      file with Python's universal newlines and never sees one, so on a CRLF
#      CHANGELOG.md it reported the file clean while no fence here could ever
#      close: the first fenced block ran to EOF and every older entry was
#      published inside the newest one's notes.
# Up to three leading COLUMNS are allowed on either delimiter; four or more is an
# indented code block, not a fence, so such a line falls through to the rules
# below as ordinary content. Columns, not characters: CommonMark advances a tab
# to the next four-column tab stop, so ONE TAB is four columns and a tab-indented
# ``` is a code block. Measuring characters instead let it open a fence here, and
# the mask then ran on until the next delimiter -- merging the following entry
# into the release notes above it. `check-docs.py`'s `indent_columns` counts the
# same way, and its docstring records the same defect from the other direction.

/^[ \t]*(```|~~~)/ {
    fl = $0
    ind = 0                                          # indentation in COLUMNS
    for (k = 1; k <= length (fl); k++) {
        ch = substr (fl, k, 1)
        if      (ch == " ")  ind++
        else if (ch == "\t") ind += 4 - (ind % 4)
        else break
    }
    if (ind <= 3) {
        sub (/^[ \t]+/, "", fl)
        fc = substr (fl, 1, 1)
        n  = 0
        while (substr (fl, n + 1, 1) == fc) n++
        rest = substr (fl, n + 1)
        if (! fence)                                            { fence = 1; f = fc; w = n }
        else if (fc == f && n >= w && rest ~ /^[ \t\r]*$/)      { fence = 0 }
        if (on) print
        next
    }
}
fence                          { if (on) print; next }
index($0, "## [" ver "]") == 1 { on = 1; print; next }
on && /^## \[/                 { exit }
on                             { print }
