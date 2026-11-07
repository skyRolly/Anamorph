#!/usr/bin/env python3
"""Independently check the owner-supplied ADR-0070 native measurement archive.

Usage: python3 worklogs/native-validation/verify-adr0070-archive.py <archive.zip>
No files are extracted or changed. Requires this repository's measured Git commit.
"""
import collections
import hashlib
import json
import math
import re
import statistics
import subprocess
import sys
import zipfile
from pathlib import Path, PurePosixPath

EXPECTED = "2468efadf36730029b135996908c696e829eccd9cbb57a95249920aca5ae7c9e"
SOURCE = "a0a48290479cac99f74c6a1391990263c82dbf29"
FIELDS = ("variant", "kind", "case", "rate", "block", "window", "reader", "paced")
ROOT = Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def verify_probe(row, request, index):
    """Check the probe's reported controls independently of its saved summary."""
    for key in ("kind", "case", "variant", "rate", "block", "window", "reader", "paced", "rep", "seconds", "smoke"):
        require(row.get(key) == request[key], f"row {index}: {key}")
    require(row.get("schema") == 2 and bool(row.get("cpu")), f"row {index}: schema/CPU")
    require(row.get("fp_denormals_disabled") is True and row.get("fp_round_to_nearest") is True,
            f"row {index}: FP setup")
    require(type(row.get("rss_available")) is bool, f"row {index}: RSS availability")
    require(not (row["variant"] == "original" and row["reader"] != "none"),
            f"row {index}: unsafe original reader")
    values = row.get("block_ns")
    require(isinstance(values, list) and all(type(x) is int and x >= 0 for x in values),
            f"row {index}: bad timing")
    count = (0 if row["kind"] == "memory" else request["iterations"]
             if row["kind"].startswith("scope-") else math.ceil(row["seconds"] * row["rate"] / row["block"]))
    denom = row["window"] if row["kind"] == "scope-read" else row["block"]
    require(len(values) == count and row.get("samples") == count * denom,
            f"row {index}: timing/sample count")
    require(not values or any(values), f"row {index}: zero timing control")
    if row["reader"] != "none":
        require(row.get("reader_accepted", 0) > 0, f"row {index}: inactive reader")
        if row["reader"] == "stalled":
            require(row.get("stall_frames", 0) > 0, f"row {index}: no producer progress")
    for key in ("audio_hash", "input_hash", "reader_hash"):
        value = row.get(key)
        require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{16}", value),
                f"row {index}: invalid {key}")
    if row["reader"] != "none":
        require(row["reader_hash"] != "cbf29ce484222325", f"row {index}: inert reader consumption")
    if row["kind"] == "engine" and row["case"] == "working":
        require(row["audio_hash"] != row["input_hash"], f"row {index}: inert working engine")
    if row["kind"].startswith("scope-"):
        expected_end = 16384 + (request["iterations"] * row["block"] if row["kind"] == "scope-push" else 0)
        require(row.get("scope_start") == 16384 and row.get("scope_end") == expected_end,
                f"row {index}: scope publication control")
        require(row["audio_hash"] != "cbf29ce484222325", f"row {index}: inert scope consumption")
    for key in ("engine_bytes", "scope_bytes", "rss_before", "rss_constructed", "rss_prepared",
                "reader_accepted", "reader_rejected", "stall_frames", "clock_min_ns", "clock_max_ns",
                "latency_samples", "scope_start", "scope_end"):
        value = row.get(key)
        require(type(value) is int and value >= 0, f"row {index}: invalid {key}")
    require(row["clock_min_ns"] <= row["clock_max_ns"], f"row {index}: reversed clock calibration")
    require(not row["rss_available"] or row["rss_prepared"] > 0, f"row {index}: missing RSS observation")


def main(path):
    require(hashlib.sha256(path.read_bytes()).hexdigest() == EXPECTED, "archive digest differs")
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        require(len(names) == len(set(names)), "duplicate archive entry")
        require(sum(item.file_size for item in z.infolist()) < 100_000_000, "archive too large")
        require(all(not PurePosixPath(n).is_absolute() and ".." not in PurePosixPath(n).parts
                    and "\\" not in n for n in names), "unsafe archive path")
        read = lambda name: json.loads(z.read(name))
        manifest = read("SHA256.json")
        require(len(manifest) == 1228 and set(names) == set(manifest) | {"SHA256.json"},
                "archive manifest inventory differs")
        for name, digest in manifest.items():
            require(hashlib.sha256(z.read(name)).hexdigest() == digest, f"digest mismatch: {name}")
        source, status, plan = read("source.json"), read("run-status.json"), read("plan.json")
        require(source["commit"] == status["source_commit"] == SOURCE, "source identity differs")
        require(source["status"] == "", "measurement source was not recorded clean")
        require(len(source["files_sha256"]) == 65, "source hash inventory differs")
        for name, digest in source["files_sha256"].items():
            require(not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts
                    and re.fullmatch(r"[A-Za-z0-9_./-]+", name), "unsafe source path")
            require(hashlib.sha256(git("show", f"{SOURCE}:{name}")).hexdigest() == digest,
                    f"source hash differs: {name}")
        fixture = git("rev-parse", "00de8c7796f37d08a338256e01830bb312ba105c:src/dsp/ScopeBuffer.h").decode().strip()
        require(fixture == "ed24ceab5ced2784d9fe96effdc46f8aeb03f472", "historical fixture differs")
        require(len(plan) == 400 and status["completed"] == list(range(400)) and status["current"] is None,
                "incomplete plan or journal")
        groups = collections.defaultdict(list)
        for i, expected in enumerate(plan):
            request = read(f"raw/{i:04}.request.json")
            lines = z.read(f"raw/{i:04}.stdout.jsonl").splitlines()
            require(request == expected and len(lines) == 1, f"request/output mismatch: {i}")
            require(not z.read(f"raw/{i:04}.stderr.log").strip(), f"nonempty stderr: {i}")
            row = json.loads(lines[0])
            verify_probe(row, request, i)
            groups[tuple(row[k] for k in FIELDS)].append(row)
        summary = read("summary.json")
        require(summary == read("analysis.json"), "analysis and summary differ")
        metrics = {tuple(m[k] for k in FIELDS): m for m in summary["metrics"]}
        require(len(groups) == len(metrics) == 80 and set(groups) == set(metrics), "metric groups differ")
        for key, rows in groups.items():
            require(sorted(x["rep"] for x in rows) == list(range(5)), f"missing repeat: {key}")
            means = [sum(x["block_ns"]) / x["samples"] for x in rows if x["samples"]]
            med = statistics.median(means) if means else None
            check = {"median_ns_per_frame": med, "min_ns_per_frame": min(means) if means else None,
                     "max_ns_per_frame": max(means) if means else None,
                     "spread_percent": (max(means) - min(means)) / med * 100 if med else None,
                     "worst_observed_call_ns": max((max(x["block_ns"]) for x in rows if x["block_ns"]), default=None),
                     "rss_prepare_deltas": [x["rss_prepared"] - x["rss_constructed"] if x["rss_available"] else None for x in rows],
                     "scope_bytes": rows[0]["scope_bytes"], "engine_bytes": rows[0]["engine_bytes"], "repetitions": 5}
            for field, calculated in check.items():
                recorded = metrics[key][field]
                equal = (math.isclose(calculated, recorded, rel_tol=1e-12, abs_tol=1e-12)
                         if isinstance(calculated, float) and recorded is not None else calculated == recorded)
                require(equal, f"{key}: {field} differs")
        pairs = collections.defaultdict(dict)
        for key, rows in groups.items():
            if key[-2] == "none":
                pairs[key[1:]][key[0]] = {row["rep"]: row for row in rows}
        require(len(pairs) == len(summary["comparisons"]) == 38, "comparison count differs")
        for key, variants in pairs.items():
            require(set(variants) == {"original", "corrected"}, f"unpaired variants: {key}")
            for rep in range(5):
                for field in ("input_hash", "audio_hash", "latency_samples"):
                    require(variants["original"][rep][field] == variants["corrected"][rep][field],
                            f"paired {field} differs: {key}, rep {rep}")
        seen = set()
        for comparison in summary["comparisons"]:
            old, new = comparison["original"], comparison["corrected"]
            key = tuple(old[k] for k in FIELDS[1:])
            require(old["variant"] == "original" and new["variant"] == "corrected"
                    and key == tuple(new[k] for k in FIELDS[1:]) and key in pairs
                    and key not in seen, "duplicate or mismatched comparison")
            seen.add(key)
            require(old == metrics[tuple(old[k] for k in FIELDS)] and new == metrics[tuple(new[k] for k in FIELDS)],
                    "comparison group differs")
            a, b = old["median_ns_per_frame"], new["median_ns_per_frame"]
            delta = (b - a) / a * 100 if a and b else None
            got = comparison["median_delta_percent"]
            require((delta is None and got is None) or (delta is not None and got is not None
                    and math.isclose(delta, got, rel_tol=1e-12, abs_tol=1e-12)), "comparison delta differs")
        require(seen == set(pairs), "missing comparison")
        print(json.dumps({"archive_sha256": EXPECTED, "manifest_entries": len(manifest),
                          "source_files_verified": len(source["files_sha256"]), "records": len(plan),
                          "metric_groups": len(groups), "comparisons": len(pairs), "status": "PASS"}, indent=2))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("provide the supplied ZIP path")
    try:
        main(Path(sys.argv[1]))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as exc:
        sys.exit(f"verification failed: {exc}")
