#!/usr/bin/env python3
"""Validate retained Callgrind companion outputs and emit scoped comparison."""
import gzip, hashlib, json, re
from pathlib import Path

root = Path(__file__).resolve().parent
before = (root / "binary-sha-before.txt").read_text()
after = (root / "binary-sha-after.txt").read_text()
if before != after:
    raise SystemExit("binary hashes changed during Callgrind runs")
receipts = {m: json.loads((root / m / "identity.json").read_text()) for m in ("baseline", "candidate")}
for mode, receipt in receipts.items():
    if receipt["binary_sha256"] not in before:
        raise SystemExit(f"{mode} binary differs from its receipt")
rows = []
for bands, osfactor in ((1, 0), (4, 0), (1, 1), (4, 1)):
    values = {}
    for mode in ("baseline", "candidate"):
        label = f"{mode}-{bands}b-os{osfactor}"
        output = (root / (label + ".stdout")).read_bytes()
        error = (root / (label + ".stderr")).read_bytes()
        with gzip.open(root / (label + ".callgrind.gz"), "rb") as f:
            data = f.read()
        marker = f"256 measured blocks; wrapped_blocks={256 if osfactor else 0}; fade_blocks=0".encode()
        if marker not in output:
            raise SystemExit(f"missing actual workload marker: {label}")
        summary = re.search(rb"^summary: (\d+)$", data, re.M)
        totals = re.search(rb"^totals: (\d+)$", data, re.M)
        if not summary or not totals or summary.group(1) != totals.group(1):
            raise SystemExit(f"missing or inconsistent instruction count: {label}")
        values[mode] = {"instructions": int(summary.group(1)),
                        "callgrind_sha256": hashlib.sha256(data).hexdigest(),
                        "stdout_sha256": hashlib.sha256(output).hexdigest(),
                        "stderr_sha256": hashlib.sha256(error).hexdigest(),
                        "output": output.decode().strip()}
    delta = values["candidate"]["instructions"] - values["baseline"]["instructions"]
    rows.append({"bands": bands, "oversampling": "x4" if osfactor else "off",
                 **values, "additional_instructions_per_block": delta / 256,
                 "percent_increase": 100 * delta / values["baseline"]["instructions"]})
print(json.dumps({"scope": "retained side artifacts, workload markers and receipt-bound binary identities; not native timing", "rows": rows}, indent=2))
