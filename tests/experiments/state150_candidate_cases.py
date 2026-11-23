import difflib
import hashlib
import json
import subprocess
import sys
from pathlib import Path


FIXTURE_START = "    constexpr std::array<std::uint32_t, 8> bits"
FIXTURE_END = "    // Prepare/reset must leave the display ring's lifetime/count intact."
SCOPE = "src/dsp/ScopeBuffer.h"
TEST = "tests/scope_snapshot_tests.h"
SEED_START = "    for (std::size_t i = 0; i < bits.size(); ++i)"
UNPACK = "            r[i] = std::bit_cast<float> (static_cast<std::uint32_t> (stereo >> 32));"
MUTATED_UNPACK = """            const auto right = static_cast<std::uint32_t> (stereo >> 32);
            r[i] = std::bit_cast<float> (right ^ (right == 0xff812345u ? 0x00400000u : 0u));"""


def git(*args):
    return subprocess.check_output(["git", *args])


def get_fixture(commit):
    source = git("show", f"{commit}:{TEST}").decode("utf-8")
    start = source.index(FIXTURE_START)
    return source[start:source.index(FIXTURE_END, start)].rstrip()


def build_source(fixture):
    return (
        """#include "dsp/ScopeBuffer.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

int main()
{
    using anamorph::ScopeBuffer;
    constexpr int cap = ScopeBuffer::capacity;
    auto ring = std::make_unique<ScopeBuffer>();
    std::vector<float> l (cap + 17), r (l.size()), outL (cap), outR (cap);
    ScopeBuffer::ReadResult read;
    int checks = 0;
    int failures = 0;
    const auto check = [&] (bool success, const char* name)
    {
        ++checks;
        if (! success) ++failures;
        std::printf ("[%s] %s\\n", success ? "PASS" : "FAIL", name);
    };
"""
        + fixture
        + """
    std::printf ("%d checks, %d failure(s)\\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
"""
    )


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    commit, target = sys.argv[1:]
    out = Path(target)
    base = git("rev-parse", f"{commit}^").decode("ascii").strip()
    original = get_fixture(base)
    corrected = get_fixture(commit)
    old_seed = original[original.index(SEED_START):original.index("    ring->pushBlock")]
    seed = corrected.index(SEED_START)
    replaced = corrected[:seed] + old_seed + corrected[corrected.index("    const auto bitsMatch", seed):]
    scope = git("show", f"{commit}:{SCOPE}")
    scope_text = scope.decode("ascii")
    if scope != git("show", f"{base}:{SCOPE}") or scope_text.count(UNPACK) != 1:
        raise ValueError("Unexpected ScopeBuffer baseline")
    changed_scope = scope_text.replace(UNPACK, MUTATED_UNPACK).encode("ascii")
    data = {
        "fixture_commit": commit,
        "fixture_tree": git("rev-parse", f"{commit}^{{tree}}").decode("ascii").strip(),
        "fixture_blob": git("rev-parse", f"{commit}:{TEST}").decode("ascii").strip(),
        "scope_blob": git("rev-parse", f"{commit}:{SCOPE}").decode("ascii").strip(),
        "legacy_seed_commit": base,
        "variants": {},
    }
    for name, fixture, header in (
        ("candidate", corrected, scope),
        ("legacy-seed", replaced, scope),
        ("right-unpack", corrected, changed_scope),
    ):
        case = out / name
        (case / "dsp").mkdir(parents=True)
        code = build_source(fixture).encode("ascii")
        (case / "probe.cpp").write_bytes(code)
        (case / "dsp/ScopeBuffer.h").write_bytes(header)
        data["variants"][name] = {"cpp_sha256": sha(code), "scope_sha256": sha(header)}
    for name, before, after in (
        ("legacy-seed", corrected, replaced),
        ("right-unpack", scope_text, changed_scope.decode("ascii")),
    ):
        diff = difflib.unified_diff(before.splitlines(), after.splitlines(), lineterm="")
        data["variants"][name]["mutation_diff"] = list(diff)
    (out / "identity.json").write_text(json.dumps(data, indent=2) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
