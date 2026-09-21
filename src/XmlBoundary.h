#pragma once

#include <juce_core/juce_core.h>

// ============================================================================
//  XmlBoundary.h — the parser-safety scan shared by every XML document this
//  plug-in hands to `juce::parseXML`.
//
//  WHY THIS EXISTS AS ONE FUNCTION RATHER THAN TWO SCANNERS. ADR-0055 (0.9.9)
//  put a byte-level boundary in front of the preset loader after measuring
//  three shapes that never returned from the parser at all. ADR-0056 measured
//  the SAME three shapes on the two host-state paths -- the session chunk and
//  each A/B slot payload -- and found them reachable at identical thresholds.
//  A second scanner written to the same specification would be a second model
//  of what JUCE's parser does with a `<`, and the two models would drift. So
//  the walk is written once, here, and the two callers differ only in WHICH
//  QUESTIONS they ask of it.
//
//  WHAT IS SHARED AND WHAT IS NOT, established rather than assumed:
//
//    * SHARED (generic XML parser safety, and all this file contains): nesting
//      depth, because `XmlDocument::readNextElement` and `readChildElements`
//      are mutually recursive with no bound; and any `<!` construct that is
//      not a comment or a CDATA section, because a `DOCTYPE` is what reaches
//      `XmlDocument::expandExternalEntity`, whose `ent.indexOf (i + 1, ";")`
//      indexes by the DTD TOKEN index rather than by the ampersand it just
//      found and therefore walks instead of terminating.
//
//    * NOT SHARED, and deliberately left in `PresetManager.cpp`: the
//      BYTE-level rules. Those read a file's bytes the way
//      `String::createStringFromData` is about to -- a UTF-16 byte-order mark
//      selecting 16-bit code units, an embedded NUL, the UTF-8 validity the
//      Windows-1252 fallback turns on. A session chunk is not decoded by that
//      function at all (`AudioProcessor::getXmlFromBinary` uses
//      `String::fromUTF8`), and an A/B payload has no bytes of its own -- it
//      arrives already decoded, as an attribute value of the outer document.
//      Copying those rules across would have been guarding the wrong decoder.
//
//    * NOT SHARED, and specific to `.anamorph` files: everything that makes a
//      preset ONE WELL-FORMED DOCUMENT -- a single top-level element, nothing
//      but whitespace after it, the XML declaration only at offset zero. Those
//      are correctness rules about a file format, not parser-safety rules, and
//      ADR-0056's ruling narrowed host state on size, depth and `DOCTYPE`
//      alone. They stay available through `DocumentRule::oneWellFormedDocument`
//      so the preset path keeps EXACTLY the behaviour ADR-0055 shipped.
// ============================================================================

namespace anamorph::xmlBoundary
{
    // ADR-0055 chose these against a 1 525-byte, two-deep preset; ADR-0056 adopted
    // the same two numbers for host state rather than fitting new ones to the
    // sessions that happen to exist today. One definition, because two copies of
    // "256 KB" are two things that can disagree: `PresetManager::maxPresetBytes`
    // and `maxPresetDepth` now name these.
    inline constexpr juce::int64 maxDocumentBytes = 256 * 1024;
    inline constexpr int         maxDocumentDepth = 8;

    enum class DocumentRule
    {
        // Depth and `DOCTYPE` only. Everything else the text may be -- two
        // documents, a trailing sentence, a stray closing tag, an unterminated
        // comment -- is left to the parser, which answers it without recursing
        // or expanding anything. ADR-0056: the host session chunk and each A/B
        // slot payload.
        parserSafetyOnly,

        // ...and additionally: exactly one top-level element, only whitespace,
        // comments and processing instructions around it, and the XML
        // declaration only at offset zero. ADR-0055: `.anamorph` preset files.
        oneWellFormedDocument
    };

    namespace detail
    {
        // Advance past the next occurrence of `terminator`. False means the construct never
        // ended -- see `skipRanOff` at the call sites for what each rule makes of that.
        inline bool skipPast (juce::String::CharPointerType& p, const char* terminator, int length)
        {
            for (; ! p.isEmpty(); ++p)
                if (juce::CharacterFunctions::compareUpTo (p, juce::CharPointer_ASCII (terminator), length) == 0)
                {
                    p += length;
                    return true;
                }
            return false;
        }

        inline bool startsWith (juce::String::CharPointerType p, const char* lit, int length)
        {
            return juce::CharacterFunctions::compareUpTo (p, juce::CharPointer_ASCII (lit), length) == 0;
        }

        // True when `p` stands at the `<?` of a processing instruction whose TARGET is exactly
        // `xml` in any case -- which XML reserves for the DECLARATION and for nothing else. The
        // comparison stops at a terminator (`compareIgnoreCaseUpTo` breaks on a zero character),
        // so a truncated `<?xm` cannot be read past the end of the text; and because a match
        // proves five non-zero characters, `p[5]` is at worst the terminator itself. `<?xmlfoo?>`
        // is a DIFFERENT target and an ordinary instruction, which is why the character after the
        // name is examined at all.
        inline bool isXmlDeclaration (juce::String::CharPointerType p)
        {
            if (juce::CharacterFunctions::compareIgnoreCaseUpTo (p, juce::CharPointer_ASCII ("<?xml"), 5) != 0)
                return false;

            const auto after = p[5];
            return after == 0 || after == '?' || juce::CharacterFunctions::isWhitespace (after);
        }
    }

    // True when `text` is safe to hand to `juce::parseXML`: nested no deeper than `maxDepth` and
    // carrying no `DOCTYPE`. Under `oneWellFormedDocument` it additionally answers whether the
    // text is ONE document; under `parserSafetyOnly` it answers nothing else at all, and in
    // particular a text this returns true for may still be malformed -- the parser will say so,
    // safely, which is the whole point of the distinction.
    //
    // The walk steps over every XML shape that may legally contain a `<` or a `>` -- a comment, a
    // CDATA section, a processing instruction (which is also how the `<?xml ... ?>` declaration
    // arrives), and a quoted attribute value -- because miscounting any of them would refuse a
    // legitimate document, and a guard that rejects real files is worse than the hole it closes.
    inline bool textIsAdmissible (const juce::String& text, int maxDepth, DocumentRule rule)
    {
        const bool oneDocument = (rule == DocumentRule::oneWellFormedDocument);

        // AN UNTERMINATED CONSTRUCT IS NOT A PARSER-SAFETY PROBLEM, and under
        // `parserSafetyOnly` it must not be treated as one. A `<!--`, a `<![CDATA[` or a `<?` that
        // never ends swallows the rest of the text for this scan AND for `XmlDocument`, which runs
        // out of data and reports an error without recursing: there is no depth hiding behind it.
        // So the scan stops and says yes, and the parser refuses the document on its own. Under
        // `oneWellFormedDocument` the same shape is a malformed file and ADR-0055 refuses it.
        //
        // AN OPENING TAG THAT NEVER ENDS IS NOT IN THAT LIST ANY MORE, and it never belonged
        // there -- see the tag walk below and ADR-0056 §"Correction, 2026-09-21". This sentence
        // used to name it, which is how a 12 KB chunk reached the recursion this file exists to
        // bound. The three constructs above are still exactly as described: measured on the pinned
        // JUCE, each returns null rather than recursing.
        const bool skipRanOff = ! oneDocument;

        const auto begin = text.getCharPointer();
        auto p = begin;
        int  depth = 0;
        bool sawRoot = false, rootClosed = false;

        while (! p.isEmpty())
        {
            const auto c = *p;

            if (c != '<')
            {
                // Character data. Inside an element it is merely content the preset format does
                // not use (and which `presetDocumentIsWellFormed` refuses by name); OUTSIDE every
                // element only whitespace may appear, and that single rule is what refuses a
                // second document's prose, a trailing sentence and a trailing run of binary alike.
                if (oneDocument && depth == 0 && ! juce::CharacterFunctions::isWhitespace (c))
                    return false;
                ++p;
                continue;
            }

            if (detail::startsWith (p, "<!--", 4))
            {
                p += 4;
                if (! detail::skipPast (p, "-->", 3)) return skipRanOff;
                continue;
            }
            if (detail::startsWith (p, "<![CDATA[", 9))
            {
                if (oneDocument && depth == 0) return false;   // a CDATA section outside every element
                p += 9;
                if (! detail::skipPast (p, "]]>", 3)) return skipRanOff;
                continue;
            }
            if (p[1] == '?')                           // a processing instruction -- or the declaration
            {
                // THE XML DECLARATION IS NOT AN ORDINARY INSTRUCTION, and treating it as one was a
                // hole in the preset boundary: `<ANAMORPH/>` followed by `<?xml version="1.0"?>`
                // was accepted, because the skip below steps over any `<?...?>` wherever it sits
                // and the `rootClosed` refusal guards only an opening TAG. `juce::parseXML` then
                // returns the first element and ignores the tail, so a malformed document loaded
                // as a preset.
                //
                // XML allows the declaration in exactly one place: the very first thing in the
                // document, once. So it is admitted only at offset zero and refused everywhere
                // else -- after the root, after a comment, after another instruction, after
                // whitespace, or after another declaration. NOTHING may precede it, not even
                // whitespace, which is the spec's own rule and is what the writer produces:
                // `XmlElement::toString` emits the header first, at offset zero, and a byte-order
                // mark is removed by the decode before this scan ever sees the text.
                //
                // Ordinary instructions are untouched, before the root and after it alike, which
                // is the tolerance ADR-0055 states and State test 114 leg E pins. Under
                // `parserSafetyOnly` a misplaced declaration is simply a malformed document, which
                // the parser refuses by itself: an A/B slot payload carries its own `<?xml ... ?>`
                // header (`ValueTree::toXmlString` emits one), so this branch is walked by every
                // real payload and must stay a skip rather than a rule.
                if (oneDocument && p.getAddress() != begin.getAddress() && detail::isXmlDeclaration (p))
                    return false;

                p += 2;
                if (! detail::skipPast (p, "?>", 2)) return skipRanOff;
                continue;
            }
            if (p[1] == '!')                           // `<!DOCTYPE ...`: the hang and the crash
                return false;

            if (p[1] == '/')                           // a closing tag
            {
                if (depth == 0)
                {
                    // A closing tag with nothing open. Malformed, and the parser says so; it
                    // cannot deepen anything, so parser safety has no opinion. The scan must not
                    // let `depth` go negative, or a run of them would buy back real depth.
                    if (oneDocument) return false;
                    p += 2;
                    if (! detail::skipPast (p, ">", 1)) return skipRanOff;
                    continue;
                }
                p += 2;
                if (! detail::skipPast (p, ">", 1)) return skipRanOff;
                if (--depth == 0) rootClosed = true;
                continue;
            }

            // An opening tag. It is walked to its own `>` with quotes honoured, so a `>` inside an
            // attribute value cannot end it early and a `<` inside one cannot open a phantom
            // element.
            if (oneDocument && rootClosed) return false;   // a SECOND top-level element
            ++p;
            juce::juce_wchar quote = 0;
            bool selfClosing = false, closed = false;
            for (; ! p.isEmpty(); ++p)
            {
                const auto t = *p;
                if (quote != 0)              { if (t == quote) quote = 0; continue; }
                if (t == '"' || t == '\'')   { quote = t; continue; }
                if (t == '/' && p[1] == '>') { selfClosing = closed = true; p += 2; break; }
                if (t == '>')                { closed = true; ++p; break; }
            }
            // A TAG THAT NEVER CLOSED, AND THE ONE CASE WHERE THAT HIDES DEPTH. The walk above
            // reaches the end of the text for exactly two reasons, and they are not equivalent.
            //
            //   * `quote == 0`: the text simply ran out inside the tag (`<r><a b c`). Everything
            //     that remained WAS scanned -- there is nothing after it -- so nothing is hidden,
            //     and `XmlDocument` runs out of data at the same place. Admit, as before.
            //
            //   * `quote != 0`: the walk consumed from an opening quote to the end of the text as
            //     though it were all one attribute value. THE PARSER DOES NOT AGREE, and the
            //     disagreement is positional: `juce_XmlDocument.cpp:491-497` treats a quote as a
            //     string delimiter only after `name =`. A quote where an attribute NAME is
            //     expected falls to `:508-510` (`setLastError ("illegal character found in ...",
            //     false)` then `break`), and a name followed by a quote with no `=` to `:500-503`
            //     (`setLastError ("expected '=' after attribute ...", false); return node;`).
            //     Both RETURN the element, and `errorOccurred` is consulted once, at `:233`,
            //     AFTER parsing -- so the parent's `readChildElements` (`:577-580`) carries on and
            //     recurses into every element that followed the quote. Those elements are the text
            //     this scan skipped, and their nesting is unbounded.
            //
            // Measured on this tree before the refusal existed: `<r><a "` followed by 4 000 `<x>`
            // is 12 007 bytes, was ADMITTED here, and SIGSEGV'd in `juce::parseXML` on a 1 MB
            // stack -- and on an 8 MB one at 40 000. The same through the real framing at 12 016
            // bytes, and inside a well-formed `<AnamorphRoot>` prefix at 12 134.
            //
            // REFUSING COSTS NO COMPATIBILITY, which is why this is an implementation repair of
            // SESSION_COMPATIBILITY_POLICY rule 7 rather than a new narrowing of it: every shape
            // that reaches here with a quote open is one `juce::parseXML` answers with null or
            // does not answer at all -- measured across the name-position, no-`=` and
            // value-position forms, long and short. The set of chunks that RESTORE is unchanged,
            // and a refusal is the outcome each path already had.
            if (! closed) return quote == 0 && skipRanOff;

            // A SELF-CLOSING ELEMENT IS AN ELEMENT, AND IT OCCUPIES A LEVEL. It just vacates it
            // again immediately, so it raises the DEEPEST level reached without raising the
            // running depth. Skipping it entirely -- which this scan did until 2026-09-19 -- put
            // enforcement one level below the definition every other part of the repository uses,
            // and the gap was reachable: `<a>..<h><leaf/></h>..</a>` is NINE elements deep and
            // passed a cap of eight, because `<leaf/>` contributed nothing.
            //
            // THE DEFINITION IS NOT INVENTED HERE and there is only one of it. ADR-0055 records a
            // preset -- `<ANAMORPH><PARAM id=.. value=../></ANAMORPH>`, whose deepest element is
            // the SELF-CLOSING `PARAM` -- as "nested exactly two deep", ADR-0056 records a session
            // as three, and the compatibility census measures both with a walk that returns
            // `d + 1` at every element including childless ones (State test 116 leg F). All three
            // count the self-closing leaf. This line is what makes the scan agree with them.
            if (depth == 0) sawRoot = true;
            if (selfClosing)
            {
                if (depth + 1 > maxDepth) return false;
                if (depth == 0) rootClosed = true;     // `<ANAMORPH/>`: opened and closed at once
            }
            else if (++depth > maxDepth)
            {
                return false;
            }
        }

        // Under `parserSafetyOnly` the walk reaching the end IS the answer: nothing exceeded the
        // depth cap and no `DOCTYPE` appeared. Whether the document is balanced, or is one
        // document, or is a document at all, is the parser's to decide.
        if (! oneDocument) return true;

        return sawRoot && rootClosed && depth == 0;
    }
}
