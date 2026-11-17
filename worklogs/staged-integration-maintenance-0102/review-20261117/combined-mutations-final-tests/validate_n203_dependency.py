#!/usr/bin/env python3
"""Check that a Mix mutation changes N2-03's downstream oracle only.

This consumes the unmodified N2-03 selector's raw stdout. It does not turn an
end-to-end failure into a pass; it checks its precise cause and the remaining
independent kernel, aligned-dry, reference, Match and partial-Mix assertions.
"""

import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3 or sys.argv[1] not in ("settled", "interior"):
        raise SystemExit("usage: validate_n203_dependency.py settled|interior STDOUT")
    mode, path = sys.argv[1], Path(sys.argv[2])
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^\s*\d+/\d+ bands\d Mix(1\.0|0\.4) Match([01]): .* "
        r"bad wet/dry/ref/output/interior (\d+)/(\d+)/(\d+)/(\d+)/(\d+) peak ",
        re.MULTILINE,
    )
    rows = pattern.findall(text)
    expected_failure = "FAIL N2-03 engine: actual output matches the independently reconstructed boundary plus downstream rounding"
    failures = [line for line in text.splitlines() if line.startswith("FAIL ")]
    if len(rows) != 48 or len(failures) != 24 or any(line != expected_failure for line in failures):
        raise SystemExit(f"unexpected selector output: {len(rows)} lanes and {len(failures)} failures")
    affected = "1.0" if mode == "settled" else "0.4"
    for mix, _match, wet, dry, reference, output, interior in rows:
        if any(int(value) != 0 for value in (wet, dry, reference, interior)):
            raise SystemExit("N2-03 kernel, dry, reference or interior assertion failed")
        if (int(output) > 0) != (mix == affected):
            raise SystemExit(f"unexpected downstream output mismatch at Mix={mix}")
    if "selector=n203 392 checks, 24 failures" not in text:
        raise SystemExit("missing expected full selector summary")
    print(f"N2-03 dependency: {mode} mutation: 48 lanes, 24 expected downstream-only failures; other stage checks pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
