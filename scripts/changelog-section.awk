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
# ---------------------------------------------------------------------------
# WHAT THIS SCRIPT PROMISES, AND WHAT IT DOES NOT
#
# It is NOT a Markdown parser and does not try to be. It answers exactly one
# question per line -- "is this a `## [` entry boundary at column 0, or is it
# inside a fenced example?" -- and it implements just enough of CommonMark to
# answer it the way `scripts/check-docs.py` answers it. The contract is external
# and testable, and `--self-test` executes it:
#
#     For every document `check-docs.py` accepts, this script finds the same
#     release boundaries: it never omits a real release, never merges two, and
#     never publishes a fenced example as one.
#
# What that requires it to model, and all it models:
#   * BLOCKQUOTE markers (`>` plus at most one space), counted as a depth;
#   * LIST markers (`-`, `*`, `+`, `1.`, `1)` ...), each contributing a CONTENT
#     COLUMN -- the marker plus its following whitespace, at most four columns of
#     it, because a fifth means the item begins with an indented code block and
#     the content column is the marker plus one (CommonMark 5.2);
#   * TABS, expanded to four-column stops, so every measurement is a column;
#   * FENCED blocks, with the container they were opened in.
#
# What it deliberately does NOT model, because no boundary decision needs it:
#   setext headings, ATX heading levels, link reference definitions, HTML blocks,
#   inline code spans, paragraph continuation, or loose/tight list semantics.
#   `check-docs.py` gates every push and rejects the structures those would
#   matter for -- a release heading behind a container marker is FORBIDDEN by
#   `CHANGELOG_POLICY.md`, not something this script has to publish.
# ---------------------------------------------------------------------------
#
# A FENCED BLOCK IS DATA, NOT STRUCTURE. Without the tracking below, a
# `## [x.y.z]` line inside a fenced sample -- an entry template in the preamble,
# a quoted diff, a worked example -- would start the extraction there, and a
# fenced `## [` line inside a real entry would cut that entry short.
#
# WHAT OPENS AND CLOSES A FENCE (CommonMark 4.5), each clause earning its place:
#   1. the SAME character. A `~~~` line inside a ``` block is data.
#   2. at least AS LONG as the opener. A ``` line inside a ```` block is data --
#      which is how a Markdown example that itself contains a fence is written.
#   3. NOTHING BUT TRAILING WHITESPACE after the run, a CARRIAGE RETURN included.
#      A line carrying an info string is an OPENING fence and can never be a
#      closer, so a nested ```cpp inside a ```markdown example does not end it.
#      `check-docs.py` reads the file with universal newlines and never sees a
#      CR, so on a CRLF CHANGELOG.md it called the file clean while no fence here
#      could close: the first block ran to EOF and every older entry was
#      published inside the newest one's notes.
#   4. A BACKTICK fence's info string may not contain a backtick. ```a`b is a
#      PARAGRAPH, so nothing after it is code; opening one here hid every
#      following line, the next release's heading included. A TILDE fence carries
#      no such restriction, and the difference is CommonMark's, not this script's.
#   5. THREE COLUMNS OF INDENT AT MOST, on either delimiter, measured from the
#      CONTAINER's content column -- not from column 0. Four or more is an
#      indented code block. Columns, not characters: one tab is four columns.
#   6. An OPENER may be preceded by CONTAINER MARKERS; a CLOSER may be preceded
#      by spaces and by nothing else, so `- ``` ` inside a fence is code text.
#
# AND WHAT ELSE ENDS ONE -- the half this script did not have, and the reason
# three releases could vanish from a document `check-docs.py` calls clean:
#   7. LEAVING THE CONTAINER THE FENCE WAS OPENED IN. A fence opened inside a
#      blockquote ends where the quote does; one opened inside a list item ends
#      at the first non-blank line indented less than that item's content column.
#      Without this the fence stayed open forever: an unclosed `> ```text` in the
#      preamble swallowed EVERY release below it, so `awk -v ver=...` printed
#      nothing for any version and a release tag could not be cut. The rule is
#      `check-docs.py`'s `fence_mask`, stated the same way and tested against it.
#   8. ...WHICH IS ONLY AS GOOD AS THE CHAIN OF ITEMS IT IS MEASURED AGAINST, and
#      that chain is carried by ONE rule: TRIM THEN EXTEND. A non-blank line
#      keeps the longest PREFIX of the carried chain it still reaches, then
#      appends the markers it states for itself; a blank changes nothing.
#      Replacing the chain with a line's own markers -- which is what this did --
#      dropped the still-open OUTER item at `- outer` over `  - nested`, and a
#      fence opened below that belonged to no item at all: the column-0 `## [`
#      that ends the list, the fence and the block in every renderer was
#      swallowed, so the newer entry's notes ran on through the older one and
#      asking for the older version printed nothing.
# ---------------------------------------------------------------------------

function expand(s,   out, col, k, ch, wid) {          # tabs to four-column stops
    out = ""; col = 0
    for (k = 1; k <= length (s); k++) {
        ch = substr (s, k, 1)
        if (ch == "\t") { wid = 4 - (col % 4); while (wid-- > 0) { out = out " "; col++ } }
        else            { out = out ch; col++ }
    }
    return out
}
function lead(s,   n) {                               # leading SPACES == columns
    n = 0
    while (substr (s, n + 1, 1) == " ") n++
    return n
}
function blank(s) { return s ~ /^[ \t\r]*$/ }
# `s` after exactly `k` blockquote markers, or SENT when it does not carry that
# many -- which is a dedent past every column, not a zero.
function qrest(s, k,   i) {
    for (i = 0; i < k; i++) {
        if (match (s, /^ *>( ?)/)) s = substr (s, RLENGTH + 1)
        else                       return SENT
    }
    return s
}

BEGIN { SENT = "\001no-such-container\001" }

{
    xl = expand($0)

    # ---- THIS LINE'S CONTAINERS: quote depth, and the chain of list items ----
    rest = xl; d = 0; base = 0; n = 0
    split ("", lid); split ("", lic)
    while (1) {
        if (match (rest, /^ *>( ?)/)) {
            rest = substr (rest, RLENGTH + 1); d++; base = 0   # content restarts inside the quote
            continue
        }
        if (match (rest, /^ *([-*+]|[0-9]+[.)]) +/)) {
            pre = substr (rest, 1, RLENGTH)
            sp = 0                                             # the marker's following whitespace
            while (substr (pre, length (pre) - sp, 1) == " ") sp++
            mk = RLENGTH - sp                                  # indent + the marker itself
            tail = substr (rest, RLENGTH + 1)
            # 5.2: four columns of padding at most. A fifth -- or nothing but
            # whitespace to end of line -- leaves the content one column past the
            # marker, and the rest of that run is the item's own indentation. Read
            # greedily instead, `-     ```text` (a code block to every renderer)
            # opened a fence here and every release below it was swallowed.
            if (sp >= 5 || tail == "") wid = 1; else wid = sp
            base += mk + wid
            n++; lid[n] = d; lic[n] = base
            rest = substr (rest, mk + wid + 1)
            continue
        }
        break
    }

    # ---- THE ITEMS THIS LINE SITS IN, carried across lines -------------------
    # A fence may be opened on a CONTINUATION line -- `- an item`, and indented
    # under it the delimiter -- and then the chain is not on that line at all.
    #
    # ONE RULE, TRIM THEN EXTEND: keep the longest PREFIX of the carried chain
    # this line still reaches (each item read in its own quote frame; containers
    # nest, so leaving an outer item leaves every inner one with it), then append
    # the markers the line states for itself. A blank changes nothing, because a
    # blank does not end an item.
    #
    # A MARKER LINE IS NOT A FRESH START. It used to REPLACE the chain, so
    # `- outer` over `  - nested` left only the nested item and the still-open
    # outer one was gone; the next continuation line fell below the nested column
    # with nothing to fall back to and read as top level. A fence opened there
    # belonged to NO item, so only a closing delimiter could end it and the
    # column-0 `## [` that ends the list, the fence and the block in every
    # renderer was swallowed -- two releases published as one note.
    if (! blank(xl)) {
        keep = 0
        for (i = 1; i <= cn; i++) {
            r = qrest(xl, cd[i])
            if (r == SENT || lead(r) < cc[i]) break
            keep = i
        }
        cn = keep
        # The indentation compared above is the one BEFORE this line's own
        # markers, which is the test CommonMark applies: an outer item matches by
        # indentation first, and only what is left may open an item inside it.
        for (i = 1; i <= n; i++) { cn++; cd[cn] = lid[i]; cc[cn] = lic[i] }
    }

    # ---- INSIDE A FENCE ------------------------------------------------------
    if (fence) {
        ended = (d < fdepth)                       # left the blockquote
        if (! ended) {
            for (i = 1; i <= fn; i++) {            # left an enclosing list item
                r = qrest(xl, fid[i])
                if (r == SENT)     { ended = 1; break }
                if (blank(r))     continue        # blank in that frame is not a dedent
                if (lead(r) < fic[i]) { ended = 1; break }
            }
        }
        if (! ended) {
            cr = qrest(xl, fdepth)                # a closer sits behind SPACES only
            if (cr != SENT) {
                ci = lead(cr); ct = substr (cr, ci + 1)
                if (ct ~ /^(```|~~~)/) {
                    fc = substr (ct, 1, 1); nn = 0
                    while (substr (ct, nn + 1, 1) == fc) nn++
                    info = substr (ct, nn + 1)
                    if (ci - fbase <= 3 && fc == f && nn >= w && blank(info)) fence = 0
                }
            }
            if (on) print
            next
        }
        # The fence ended HERE by leaving its container, so this line is outside
        # it: not content, not a closer, and read from scratch below -- opener
        # test included, because a line that ends one fence may open the next.
        fence = 0
    }

    # ---- NO FENCE ACTIVE: may this line open one? ----------------------------
    if (n > 0) { ind = lead(rest); body = substr (rest, ind + 1) }
    else {
        r = qrest(xl, d)
        if (r == SENT) { ind = 4; body = "" }
        else {
            # The allowance is counted from the item's content column, which for a
            # continuation line is on an earlier line: four columns into a `10. `
            # item is an ordinary nested sample, and counting from column 0 opened
            # nothing there.
            ind = lead(r) - ((cn > 0 && cd[cn] == d) ? cc[cn] : 0)
            body = substr (r, lead(r) + 1)
        }
    }
    if (ind <= 3 && body ~ /^(```|~~~)/) {
        fc = substr (body, 1, 1); nn = 0
        while (substr (body, nn + 1, 1) == fc) nn++
        info = substr (body, nn + 1)
        if (! (fc == "`" && index (info, "`"))) {
            fence = 1; f = fc; w = nn; fdepth = d
            fbase = (cn > 0 && cd[cn] == d) ? cc[cn] : 0
            fn = cn
            for (i = 1; i <= cn; i++) { fid[i] = cd[i]; fic[i] = cc[i] }
            if (on) print
            next
        }
    }
}
index($0, "## [" ver "]") == 1 { on = 1; print; next }
on && /^## \[/                 { exit }
on                             { print }
