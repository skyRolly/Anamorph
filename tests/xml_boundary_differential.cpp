// ============================================================================
//  xml_boundary_differential.cpp — the contract between `anamorph::xmlBoundary`
//  and the parser it stands in front of, asserted instead of assumed.
//
//  THE CONTRACT, in one line:
//
//      textIsAdmissible (t, cap, parserSafetyOnly) == true
//          IMPLIES  juce::parseXML (t) RETURNS
//
//  "Returns" means returns — a document or a null, either is a pass. What is
//  forbidden is not returning at all. ADR-0055 and ADR-0056 guard the recursive
//  parsing/entity expansion paths that were unbounded in the earlier dependency.
//  JUCE 9.0.5 bounds those paths; the product boundaries remain unchanged.
//
//  WHY THIS FILE EXISTS. `XmlBoundary.h` is a hand-written partial XML scanner
//  standing in front of a parser whose behaviour it MODELS. Round 52 shipped one
//  such model — "an unterminated construct swallows the rest of the text for this
//  scan AND for `XmlDocument`" — and it was true for a comment, a CDATA section
//  and a processing instruction, and false for an opening tag: the parser treats
//  a quote as a string delimiter only after `name =`
//  (`juce_XmlDocument.cpp:491-497`), so a quote in NAME position errors at
//  `:508-510`, RETURNS the element, and `readChildElements` carries on through
//  everything that followed. A 12 KB chunk walked through both host-state
//  boundaries and took a SIGSEGV. Nothing in the tree compared the two, so
//  nothing could have caught it; State test 116 asserts the shapes someone
//  thought of, which is a different thing.
//
//  THE ORACLE HAS TO BE ABLE TO FAIL. Controlled child failures independently
//  prove return/rejection, signal, timeout and setup-error classification. A
//  well-formed document one level above the product cap is accepted by JUCE but
//  refused by the boundary, so upstream hardening cannot hide a disabled guard.
//  JUCE 9.0.5 bounds its own recursion: the historical deep stress inputs must
//  now be REJECTED, not required to crash (ADR-0071). The earlier dependency's
//  crash-depth search remains in its version branch for historical comparisons.
//
//  ISOLATION, AND WHY THIS IS NOT A STATE TEST. A false negative is a stack
//  overflow: it kills the process that meets it. So every parse runs in a FORKED
//  CHILD on a `pthread` with an explicit 1 MB stack — the Windows main-thread
//  default the two ADRs measured against — under an alarm that catches the
//  non-returning loop. `fork` and `pthread_attr_setstacksize` are POSIX, so this
//  harness is Linux-only by construction and is wired into the `linux` job. It is
//  deliberately NOT folded into `AnamorphStateTests`: that suite must not contain
//  a case whose failure mode is taking the whole run down with it. State test 116
//  leg E2 carries the same shapes as ordinary assertions on the boundary's
//  VERDICT, which is what every platform can run; this file is the half that
//  contains failures that must not end the test runner.
//
//  Build (the `linux` job does exactly this):
//    c++ -std=c++23 -O1 -I src -I <juce>/modules \
//        -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 \
//        -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 \
//        tests/xml_boundary_differential.cpp \
//        <juce>/modules/juce_core/juce_core.cpp \
//        <juce>/modules/juce_core/juce_core_CompilationTime.cpp \
//        -o xml-boundary-differential -lpthread -ldl -lz
//
//  Usage: xml-boundary-differential [cases] [seed]   (default 1500, 20260921)
//  Exit:  0 clean · 1 a false negative, a dead oracle, or a refused legitimate
//         document.
// ============================================================================

#include <juce_core/juce_core.h>

#include "XmlBoundary.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
    using anamorph::xmlBoundary::DocumentRule;
    using anamorph::xmlBoundary::textIsAdmissible;

    constexpr int kCap = anamorph::xmlBoundary::maxDocumentDepth;

    enum Outcome { kReturned, kRejected, kCrashed, kHung, kHarnessError };
    enum class Fault { none, signal, timeout, setup };

    const char* outcomeName (Outcome o)
    {
        switch (o)
        {
            case kReturned: return "returned";
            case kRejected: return "rejected";
            case kCrashed:  return "CRASHED";
            case kHung:     return "HUNG";
            default:        return "harness-error";
        }
    }

    bool parserAnswered (Outcome out)
    {
        return out == kReturned || out == kRejected;
    }

    struct ParseArg { const std::string* text; bool accepted = false; };

    void* parseOnThread (void* raw)
    {
        auto* a = static_cast<ParseArg*> (raw);
        auto x = juce::parseXML (juce::String::fromUTF8 (a->text->data(), (int) a->text->size()));
        a->accepted = x != nullptr; // a null still means the parser answered
        return nullptr;
    }

    // The parse, contained. A child so a stack overflow is an exit status; a thread
    // inside it because `setrlimit (RLIMIT_STACK)` after start cannot shrink the
    // already-mapped main stack -- an earlier version of this harness used it, and
    // its liveness check correctly refused to trust the result.
    Outcome parseContained (const std::string& text, size_t stackKB, unsigned timeoutSec,
                            Fault fault = Fault::none)
    {
        fflush (nullptr);
        const pid_t pid = fork();
        if (pid < 0) return kHarnessError;

        if (pid == 0)
        {
            alarm (timeoutSec);
            if (fault == Fault::signal) kill (getpid(), SIGKILL); // controlled, no core dump
            if (fault == Fault::timeout) for (;;) pause();       // alarm is the only exit
            if (fault == Fault::setup) _exit (9);
            pthread_attr_t attr;
            if (pthread_attr_init (&attr) != 0) _exit (9);
            if (pthread_attr_setstacksize (&attr, stackKB * 1024u) != 0) _exit (9);
            ParseArg arg { &text };
            pthread_t t;
            if (pthread_create (&t, &attr, parseOnThread, &arg) != 0) _exit (9);
            pthread_attr_destroy (&attr);
            if (pthread_join (t, nullptr) != 0) _exit (9);
            _exit (arg.accepted ? 0 : 10);
        }

        int status = 0;
        if (waitpid (pid, &status, 0) != pid) return kHarnessError;
        if (WIFSIGNALED (status)) return WTERMSIG (status) == SIGALRM ? kHung : kCrashed;
        if (WIFEXITED (status) && WEXITSTATUS (status) == 0) return kReturned;
        if (WIFEXITED (status) && WEXITSTATUS (status) == 10) return kRejected;
        return kHarnessError;
    }

    std::string repeat (const char* unit, int n)
    {
        std::string s;
        s.reserve (std::strlen (unit) * (size_t) n);
        for (int i = 0; i < n; ++i) s += unit;
        return s;
    }

    bool admits (const std::string& doc)
    {
        return textIsAdmissible (juce::String::fromUTF8 (doc.data(), (int) doc.size()),
                                 kCap, DocumentRule::parserSafetyOnly);
    }

    // ---- the deterministic PRNG, so a finding reproduces from its seed ----------
    struct Rng
    {
        juce::uint64 s;
        explicit Rng (juce::uint64 seed) : s (seed ? seed : 0x9E3779B97F4A7C15ULL) {}
        juce::uint64 next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
        int  below (int n) { return n <= 1 ? 0 : (int) (next() % (juce::uint64) n); }
        bool chance (int pct) { return below (100) < pct; }
    };

    // Every fragment here is a branch the scan makes a decision on. The quote
    // entries are split by POSITION on purpose: value position is the one the
    // round-52 model got right, name position is the one it got wrong, and a
    // generator carrying only the first cannot find the second -- which is exactly
    // how an earlier 12 000-document sweep reported "no false negative".
    const char* const kAttrs[] = {
        "",
        " id=\"w\"",
        " a=\"x>y\"",               // '>' inside a quoted value
        " a=\"x<y\"",               // '<' inside a quoted value
        " a='x>y' b=\"p<q\"",
        " a=\"?>\"",                // a PI terminator inside a value
        " a=\"-->\"",               // a comment terminator inside a value
        " a=\"]]>\"",               // a CDATA terminator inside a value
        " a=\"<!DOCTYPE x>\"",      // a DOCTYPE spelling inside a value
        " a=\"unterminated",        // quote never closed, VALUE position
        " \"",                      // quote never closed, NAME position  <-- F1
        " '",                       // ...single-quoted
        " b\"",                     // a name, then a quote with no '='   <-- F1
    };

    const char* const kInterstitial[] = {
        "",
        "<!-- c -->",
        "<![CDATA[ <a><b> ]]>",
        "<?pi data?>",
        "text",
        "<!-- unterminated",
        "<![CDATA[ unterminated",
        "<?pi unterminated",
        "</stray>",
        "&ent;",
        "&#38;",
    };

    std::string makeDoc (Rng& r, int depth)
    {
        std::string out;
        if (r.chance (30)) out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

        std::vector<std::string> open;
        for (int d = 0; d < depth; ++d)
        {
            const std::string tag = "e" + std::to_string (d);
            out += "<" + tag + kAttrs[r.below ((int) (sizeof kAttrs / sizeof *kAttrs))];
            if (r.chance (12)) { out += "/>"; continue; }   // self-closing: a level, vacated
            out += ">";
            open.push_back (tag);
            out += kInterstitial[r.below ((int) (sizeof kInterstitial / sizeof *kInterstitial))];
        }
        for (auto it = open.rbegin(); it != open.rend(); ++it)
            out += "</" + *it + ">";

        if (r.chance (8)) out += "<!DOCTYPE d [<!ENTITY a \"bbbb\">]>";
        if (r.chance (8)) out += "trailing prose";
        if (r.chance (6)) out.push_back ('\0');            // String::fromUTF8 stops here
        if (r.chance (6)) out = out.substr (0, out.size() * (size_t) (40 + r.below (55)) / 100);
        return out;
    }

    int failures = 0;

    // The whole check, for one document: if the boundary admits it, the parser must return.
    void oracle (const char* label, const std::string& doc, size_t stackKB = 1024)
    {
        if (! admits (doc)) return;                        // refused: the parser never sees it
        const auto out = parseContained (doc, stackKB, 3);
        if (parserAnswered (out)) return;
        ++failures;
        std::printf ("  !! FALSE NEGATIVE (%s): admitted, parser %s -- %zu B: %.90s\n",
                     label, outcomeName (out), doc.size(), doc.c_str());
    }
}

int main (int argc, char** argv)
{
    const int          cases = argc > 1 ? std::atoi (argv[1]) : 1500;
    const juce::uint64 seed  = argc > 2 ? (juce::uint64) std::strtoull (argv[2], nullptr, 10)
                                        : 20260921ULL;

    std::printf ("xml-boundary differential: admitted => the parser returns (depth cap %d)\n", kCap);

    // ---- 1. THE ORACLE AND THE PRODUCT BOUNDARY MUST BOTH BE LIVE ----------------
    struct Canary { const char* label; const char* text; Fault fault; Outcome expected; bool safe; };
    const Canary canaries[] = {
        { "valid return", "<check/>", Fault::none, kReturned, true },
        { "parser rejection", "<", Fault::none, kRejected, true },
        { "controlled signal", "<check/>", Fault::signal, kCrashed, false },
        { "controlled timeout", "<check/>", Fault::timeout, kHung, false },
        { "controlled setup error", "<check/>", Fault::setup, kHarnessError, false },
    };
    for (const auto& c : canaries)
    {
        const auto out = parseContained (c.text, 1024, 1, c.fault);
        if (out != c.expected || parserAnswered (out) != c.safe)
        {
            std::printf ("::error::oracle canary %s: expected %s, observed %s\n",
                         c.label, outcomeName (c.expected), outcomeName (out));
            return 1;
        }
    }
    std::printf ("  liveness: all five contained-outcome canaries observed\n");
    const auto overCap = repeat ("<a>", kCap + 1) + repeat ("</a>", kCap + 1);
    if (parseContained (overCap, 1024, 3) != kReturned || admits (overCap))
    {
        std::printf ("::error::product depth boundary is not independently live\n");
        return 1;
    }
    std::printf ("  liveness: JUCE accepts depth cap+1; the product boundary refuses it\n");

   #if JUCE_VERSION >= 0x090005
    // Retain the existing deep controls. The hardened parser must reject each;
    // return alone could conceal a missing upstream limit. Never require a crash
    // from a dependency that deliberately added a recursion guard.
    const int fixtureDepth = 3000;
    for (const int levels : { 3000, 5000, 10000, 20000, 40000 })
    {
        const auto shape = repeat ("<a>", levels) + repeat ("</a>", levels);
        const auto out = parseContained (shape, 1024, 5);
        if (out != kRejected || admits (shape))
        {
            std::printf ("::error::hardened parser control at %d levels: %s; boundary admits=%d\n",
                         levels, outcomeName (out), admits (shape));
            return 1;
        }
    }
    std::printf ("  liveness: all five historical deep controls rejected by JUCE and boundary\n");
   #else
    // Fed directly, past the boundary, a deep enough document must not return. The
    // depth at which that happens belongs to the toolchain, so search for it.
    int fixtureDepth = 0;
    for (const int levels : { 3000, 5000, 10000, 20000, 40000 })
    {
        const auto shape = repeat ("<a>", levels) + repeat ("</a>", levels);
        const auto out   = parseContained (shape, 1024, 5);
        std::printf ("  liveness: %6d levels (%zu B) -> %s\n",
                     levels, shape.size(), outcomeName (out));
        if (out == kCrashed || out == kHung) { fixtureDepth = levels; break; }
    }
    if (fixtureDepth == 0)
    {
        std::printf ("::error::no nesting depth in this harness's range exhausted a 1 MB stack, so the\n"
                     " oracle cannot observe the failure it exists to catch. Do not read the result\n"
                     " below as a pass.\n");
        return 1;
    }
    // ...and the boundary must refuse that very shape. If it did not, everything
    // else here is moot.
    {
        const auto shape = repeat ("<a>", fixtureDepth) + repeat ("</a>", fixtureDepth);
        if (admits (shape))
        {
            std::printf ("::error::the boundary ADMITS the %d-level shape that does not return\n", fixtureDepth);
            return 1;
        }
        std::printf ("  liveness: the boundary refuses it, and the oracle can see a crash at %d levels\n",
                     fixtureDepth);
    }

   #endif

    // ---- 2. THE NAMED SHAPES ------------------------------------------------------
    // Retain the original named corpus and generated sweep. New-parser fixture
    // depth stays at the original Clang witness; legacy pins measure their depth.
    const std::string tail = repeat ("<x>", fixtureDepth);

    struct Named { const char* label; std::string doc; };
    const Named named[] = {
        { "quote in attribute-NAME position",      "<r><a \"" + tail },
        { "...single-quoted",                      "<r><a '"  + tail },
        { "name, then a quote with no '='",        "<r><a b\"" + tail },
        { "quote in attribute-VALUE position",     "<r><a b=\"" + tail },
        { "...single-quoted",                      "<r><a b='"  + tail },
        { "realistic session, malformed tag inside the root",
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?><AnamorphRoot presetName=\"Gentle Width\">"
          "<ANAMORPH><PARAM id=\"drive\" value=\"0.9\"/></ANAMORPH><a \"" + tail },
        { "unterminated comment",                  "<r><!--"   + tail },
        { "unterminated CDATA",                    "<r><![CDATA[" + tail },
        { "unterminated processing instruction",   "<r><?"     + tail },
        { "tag runs out of text, no quote open",   "<r><a b c" },
        { "stray closing tags",                    "</a></b></c><r/>" },
        { "self-closing elements at the cap",      repeat ("<a>", kCap - 1) + "<leaf/>"
                                                   + repeat ("</a>", kCap - 1) },
        { "embedded NUL then depth",               std::string ("<r>\0", 4) + tail },
        { "'<' and '>' inside attribute values",   "<r a=\"x<y>z\"><b c='p>q<r'/></r>" },
        { "DOCTYPE",                               "<!DOCTYPE d [<!ENTITY a \"&a;\">]><r>&a;</r>" },
        { "truncated mid-attribute-value",         "<AnamorphRoot presetName=\"Gentle" },
        { "truncated mid-tag",                     "<AnamorphRoot presetNa" },
        { "truncated after a closing bracket",     "<AnamorphRoot><ANAMORPH>" },
    };
    for (const auto& n : named) oracle (n.label, n.doc);
    std::printf ("  named shapes: %d checked\n", (int) (sizeof named / sizeof *named));

    // ---- 3. LEGITIMATE DOCUMENTS MUST STILL BE ADMITTED ---------------------------
    // Without this the contract is satisfiable by refusing everything, which would
    // pass §2 and lose every session the product has ever written.
    const Named legit[] = {
        { "writer-shaped session",
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?><AnamorphRoot presetName=\"Gentle Width\">"
          "<ANAMORPH><PARAM id=\"width\" value=\"1.5\" raw=\"1.5\"/></ANAMORPH>"
          "<ANAMORPH_INTERNAL int_osIndex=\"0\"/>"
          "<AB active=\"0\" slotAParams=\"&lt;ANAMORPH/&gt;\"/></AnamorphRoot>" },
        { "apostrophe in a preset name",
          "<AnamorphRoot presetName=\"Bob&apos;s Width\"><ANAMORPH/></AnamorphRoot>" },
        { "escaped '<' and '>' in a value",
          "<AnamorphRoot presetName=\"a&lt;b&gt;c\"><ANAMORPH/></AnamorphRoot>" },
        { "single-quoted attribute values",
          "<AnamorphRoot presetName='Gentle'><ANAMORPH><PARAM id='w' value='1'/></ANAMORPH></AnamorphRoot>" },
        { "an A/B payload as its own document",
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?><ANAMORPH><PARAM id=\"width\" value=\"1.2\"/></ANAMORPH>" },
        { "a comment and a processing instruction around the root",
          "<?xml version=\"1.0\"?><!-- saved by Anamorph --><?pi x?><AnamorphRoot><ANAMORPH/></AnamorphRoot>" },
        { "exactly at the depth cap",
          repeat ("<a>", kCap) + repeat ("</a>", kCap) },
    };
    int refusedLegit = 0;
    for (const auto& l : legit)
        if (! admits (l.doc))
        {
            ++refusedLegit;
            std::printf ("  !! REFUSED A LEGITIMATE DOCUMENT (%s): %s\n", l.label, l.doc.c_str());
        }
    std::printf ("  legitimate shapes: %d checked, %d refused\n",
                 (int) (sizeof legit / sizeof *legit), refusedLegit);

    // ---- 4. THE SWEEP -------------------------------------------------------------
    Rng r { seed };
    int admitted = 0, refused = 0, admittedOverCap = 0;
    for (int i = 0; i < cases; ++i)
    {
        // Depths on both sides of the cap, plus deep ones the cap must catch.
        const int depth = r.chance (55) ? 1 + r.below (kCap)
                        : r.chance (60) ? kCap + 1 + r.below (4)
                                        : 200 + r.below (fixtureDepth);
        const auto doc = makeDoc (r, depth);

        if (! admits (doc)) { ++refused; continue; }
        ++admitted;
        // THE LOAD-BEARING CLASS: admitted although nested past the cap, which can only
        // happen through a branch that stopped scanning early. A run with none of these
        // has not tested the thing this file is for.
        if (depth > kCap) ++admittedOverCap;

        const auto out = parseContained (doc, 1024, 3);
        if (! parserAnswered (out))
        {
            ++failures;
            std::printf ("  !! FALSE NEGATIVE (sweep, seed %llu case %d): admitted, parser %s -- %.90s\n",
                         (unsigned long long) seed, i, outcomeName (out), doc.c_str());
        }
    }
    std::printf ("  sweep: %d cases (seed %llu) -- admitted %d, refused %d, admitted past the cap %d\n",
                 cases, (unsigned long long) seed, admitted, refused, admittedOverCap);

    if (admittedOverCap == 0)
    {
        std::printf ("::error::the sweep admitted no document nested past the cap, so the early-exit\n"
                     " branches were never exercised. Raise the case count or check the generator.\n");
        return 1;
    }

    const int rc = (failures == 0 && refusedLegit == 0) ? 0 : 1;
    std::printf ("%s\n", rc == 0 ? "xml-boundary differential: clean"
                                 : "xml-boundary differential: FAILED");
    return rc;
}
