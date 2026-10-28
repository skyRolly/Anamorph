#!/usr/bin/env python3
"""Validate current DSP dump records, then apply ADR-0032's Windows hash gate.

Only text is read. The DSP scenario generator remains tests/dsp_dump.cpp.
Latency is retained and compared as separate metadata; C is informational.
Run --self-test before use in each job (TESTING_POLICY rule 4).
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


# Explicit current record contract, matching tests/dsp_dump.cpp::scenarios.
# An intentional matrix change must update this contract and its text fixtures.
EXPECTED_SCENARIOS = frozenset(
    f"{algorithm}-{factor}-{mode}"
    for algorithm in ("haas", "velvet", "chorus", "dimensional")
    for factor in ("os1", "os2", "os4", "os8")
    for mode in ("lr", "ms")
)
HASH = re.compile(r"[0-9a-fA-F]{16}")
LATENCY = re.compile(r"[0-9]+")
# The harness emits latency with %d from a C++ int on the supported targets.
MAX_LATENCY = 2147483647
HEADERS = (
    re.compile(r"dsp-dump\s+self-check\s+passed:\s+32\s+scenarios,\s+"
               r"all\s+repeatable\s+and\s+all\s+distinct\."),
    re.compile(r"#\s+AnamorphDspDump\s+--\s+[0-9]+\s+Hz,\s+"
               r"[0-9]+-sample\s+blocks,\s+[0-9]+\s+noise\s+\+\s+"
               r"[0-9]+\s+silence\s+blocks"),
    re.compile(r"#\s+scenario\s+fnv1a-64\s+latency"),
)


class InvalidDump(ValueError):
    """An input cannot support any numerical verdict."""


def parse_dump(text: str) -> dict[str, dict]:
    records: dict[str, dict] = {}
    for number, raw in enumerate(text.replace("\r\n", "\n").replace("\r", "\n").split("\n"), 1):
        line = raw.strip()
        if not line or any(header.fullmatch(line) for header in HEADERS):
            continue
        fields = line.split()
        scenario = fields[0]
        location = f"line {number}, scenario {scenario!r}"
        if scenario not in EXPECTED_SCENARIOS:
            raise InvalidDump(f"{location}: unknown scenario or unsupported output: {raw!r}")
        if scenario in records:
            raise InvalidDump(f"{location}: duplicate identity (first at line "
                              f"{records[scenario]['line']}); duplicates are ambiguous")
        if len(fields) != 3:
            raise InvalidDump(f"{location}: expected exactly scenario, 16-hex hash and latency; "
                              f"got {len(fields)} fields: {raw!r}")
        if not HASH.fullmatch(fields[1]):
            raise InvalidDump(f"{location}: invalid 16-hex hash {fields[1]!r}")
        token = fields[2]
        if not LATENCY.fullmatch(token):
            raise InvalidDump(f"{location}: latency must be a nonnegative decimal integer, "
                              f"got {token!r}")
        digits = token.lstrip("0") or "0"
        # Bound before int(): very long invalid integers must also fail as input,
        # including on Python versions with a decimal conversion length limit.
        if len(digits) > 10 or (len(digits) == 10 and digits > str(MAX_LATENCY)):
            raise InvalidDump(f"{location}: latency overflows the harness int (max {MAX_LATENCY})")
        records[scenario] = {"hash": fields[1].lower(), "latency": int(digits), "line": number}
    missing = sorted(EXPECTED_SCENARIOS - records.keys())
    if missing:
        raise InvalidDump("missing scenarios: " + ", ".join(missing))
    return records


def read_dump(path: Path, label: str) -> tuple[dict, str | None]:
    evidence = {"path": str(path), "sha256": None, "valid": False,
                "record_count": None, "records": None}
    try:
        raw = path.read_bytes()
        evidence["sha256"] = hashlib.sha256(raw).hexdigest()
        records = parse_dump(raw.decode("utf-8-sig"))
    except (OSError, UnicodeError, InvalidDump) as error:
        return evidence, f"{label} ({path}): {error}"
    evidence.update(valid=True, record_count=len(records), records=records)
    return evidence, None


def assess(mode: str, left: Path, right: Path) -> dict:
    labels = ("A", "B") if mode == "ab" else ("B", "C")
    inputs, errors = {}, []
    for label, path in zip(labels, (left, right)):
        inputs[label], error = read_dump(path, label)
        if error:
            errors.append(error)
    result = {"schema_version": 1, "mode": mode, "inputs": inputs,
              "classification": "INFRASTRUCTURE" if mode == "ab" else "INFORMATIONAL_UNAVAILABLE",
              "exit_code": 2 if mode == "ab" else 0,
              "hashes": {"status": "NOT_COMPARED", "compared_scenarios": None, "differences": []},
              "latency": {"status": "NOT_COMPARED", "compared_scenarios": None, "differences": []},
              "errors": errors if mode == "ab" else [],
              "warnings": errors if mode == "c" else []}
    if errors:
        return result
    for field, output in (("hash", "hashes"), ("latency", "latency")):
        differences = [
            {"scenario": name, labels[0]: inputs[labels[0]]["records"][name][field],
             labels[1]: inputs[labels[1]]["records"][name][field]}
            for name in sorted(EXPECTED_SCENARIOS)
            if inputs[labels[0]]["records"][name][field] != inputs[labels[1]]["records"][name][field]
        ]
        result[output] = {"status": ("DIFFERENT" if differences else
                                     "IDENTICAL" if field == "hash" else "EQUAL"),
                          "compared_scenarios": len(EXPECTED_SCENARIOS), "differences": differences}
    mismatch = bool(result["hashes"]["differences"])
    result.update(classification=("NUMERICAL" if mismatch else "IDENTICAL") if mode == "ab"
                  else "INFORMATIONAL", exit_code=1 if mode == "ab" and mismatch else 0)
    for row in result["latency"]["differences"]:
        result["warnings"].append(
            f"METADATA: {row['scenario']} reported latency {labels[0]}={row[labels[0]]}, "
            f"{labels[1]}={row[labels[1]]}; separate from ADR-0032 hash equality")
    return result


def annotation(level: str, message: str) -> str:
    # Preserve diagnostics without letting raw malformed text inject annotations.
    return f"::{level}::" + message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def messages(result: dict) -> list[str]:
    lines = [annotation("error", "INFRASTRUCTURE: " + error) for error in result["errors"]]
    prefix = "informational C: " if result["mode"] == "c" else ""
    lines += [annotation("warning", prefix + warning) for warning in result["warnings"]]
    if result["hashes"]["status"] == "NOT_COMPARED":
        return lines
    labels = ("A", "B") if result["mode"] == "ab" else ("B", "C")
    for row in result["hashes"]["differences"]:
        message = (f"{row['scenario']}: hash {labels[0]}={row[labels[0]]}, "
                   f"{labels[1]}={row[labels[1]]}")
        lines.append(annotation("error", "NUMERICAL: " + message) if result["mode"] == "ab"
                     else "informational C: " + message)
    count = result["hashes"]["compared_scenarios"]
    lines.append(f"{prefix}{'/'.join(labels)} hashes {result['hashes']['status']}: "
                 f"{len(result['hashes']['differences'])}/{count} scenario hashes differ; "
                 f"latency metadata {result['latency']['status']}: "
                 f"{len(result['latency']['differences'])}/{count} scenarios differ.")
    return lines


def check_evidence_paths(left: Path, right: Path, report: Path, summary: Path | None) -> None:
    """Refuse input aliases before either evidence output can be opened."""
    for option, output in (("--report", report), ("--summary", summary)):
        if output is None:
            continue
        for source_option, source in (("--left", left), ("--right", right)):
            same = output.resolve() == source.resolve()
            if not same:
                try:
                    same = output.samefile(source)  # Includes existing hard links.
                except FileNotFoundError:
                    pass  # A new evidence path is allowed; input parsing handles missing dumps.
            if same:
                raise ValueError(f"{option} {str(output)!r} refers to {source_option} raw dump "
                                 f"{str(source)!r}; refusing all evidence writes")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    commands = parser.add_subparsers(dest="mode")
    for mode in ("ab", "c"):
        command = commands.add_parser(mode)
        command.add_argument("--left", type=Path, required=True)
        command.add_argument("--right", type=Path, required=True)
        command.add_argument("--report", type=Path, required=True)
        command.add_argument("--summary", type=Path)
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if args.mode is None:
        parser.error("choose ab or c, or --self-test")
    try:
        check_evidence_paths(args.left, args.right, args.report, args.summary)
    except (OSError, RuntimeError, ValueError) as error:
        # Neither output is safe to open until BOTH have been checked. In
        # particular, do not enter the evidence writer's report-retry path.
        level = "error" if args.mode == "ab" else "warning"
        prefix = "INFRASTRUCTURE" if args.mode == "ab" else "informational C"
        print(annotation(level, f"{prefix}: unsafe evidence paths: {error}"))
        return 2 if args.mode == "ab" else 0
    result = assess(args.mode, args.left, args.right)
    lines = messages(result)
    try:
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        if args.summary:
            with args.summary.open("a", encoding="utf-8") as summary:
                summary.write("\n" + "\n".join(lines) + "\n")
    except OSError as error:
        # C's evidence writer is optional too; never undo the completed A/B gate.
        level = "error" if args.mode == "ab" else "warning"
        prefix = "INFRASTRUCTURE" if args.mode == "ab" else "informational C"
        message = f"cannot retain comparison evidence: {error}"
        result.update(classification="INFRASTRUCTURE" if args.mode == "ab" else "INFORMATIONAL_UNAVAILABLE",
                      exit_code=2 if args.mode == "ab" else 0)
        result["errors" if args.mode == "ab" else "warnings"].append(message)
        # A summary write may fail after the JSON write. Keep any writable JSON
        # consistent with the returned disposition; an unwritable report is
        # diagnosed on stdout without claiming a completed comparison.
        try:
            args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        except OSError:
            pass
        print(annotation(level, f"{prefix}: {message}"))
        return result["exit_code"]
    print("\n".join(lines))
    return result["exit_code"]


def self_test() -> int:
    """Inert fixtures through the same CLI Windows calls; no binaries/builds/network."""
    import subprocess
    import tempfile

    helper = Path(__file__).resolve()
    fixture = helper.parent / "fixtures" / "dsp-dump" / "valid.txt"
    valid = fixture.read_text(encoding="utf-8")
    rows = [line for line in valid.splitlines() if line.startswith(
        ("haas-", "velvet-", "chorus-", "dimensional-"))]
    first = rows[0]
    changed = valid.replace("0000000000000001", "ffffffffffffffff", 1)
    overwrite = changed + first + "\n"
    cases = 0
    with tempfile.TemporaryDirectory(prefix="anamorph-dump-fixtures-") as directory:
        root = Path(directory)

        def run(left: str | bytes, right: str | bytes | None, mode: str = "ab",
                script: Path = helper) -> tuple[subprocess.CompletedProcess, dict]:
            nonlocal cases
            cases += 1
            a, b, report = root / "left.txt", root / "right.txt", root / "result.json"
            a.write_bytes(left.encode("utf-8") if isinstance(left, str) else left)
            if right is None:
                b.unlink(missing_ok=True)
            else:
                b.write_bytes(right.encode("utf-8") if isinstance(right, str) else right)
            process = subprocess.run([sys.executable, str(script), mode, "--left", str(a),
                                      "--right", str(b), "--report", str(report)],
                                     capture_output=True, text=True, timeout=15)
            result = json.loads(report.read_text(encoding="utf-8"))
            assert process.returncode == result["exit_code"], (process, result)
            # The production reader must leave every raw byte intact, even on refusal.
            assert a.read_bytes() == (left.encode("utf-8") if isinstance(left, str) else left)
            if right is not None:
                assert b.read_bytes() == (right.encode("utf-8") if isinstance(right, str) else right)
                assert result["inputs"]["B" if mode == "ab" else "C"]["sha256"] == hashlib.sha256(b.read_bytes()).hexdigest()
            return process, result

        def refuses(left: str | bytes, right: str | bytes | None, reason: str,
                    mode: str = "ab", script: Path = helper) -> dict:
            process, result = run(left, right, mode, script)
            assert result["classification"] == ("INFRASTRUCTURE" if mode == "ab" else "INFORMATIONAL_UNAVAILABLE"), result
            assert result["hashes"]["status"] == result["latency"]["status"] == "NOT_COMPARED", result
            assert result["hashes"]["compared_scenarios"] is None, result
            assert result["latency"]["compared_scenarios"] is None, result
            assert "IDENTICAL" not in process.stdout, process.stdout
            assert reason in "\n".join(result["errors"] + result["warnings"]), result
            if mode == "c":
                assert process.returncode == 0 and "::warning::" in process.stdout, process
                assert "::error::" not in process.stdout and not result["errors"], result
                assert "scenario hashes differ" not in process.stdout, process.stdout
                assert result["inputs"]["C"]["record_count"] is None, result
            else:
                assert process.returncode == 2 and "::error::INFRASTRUCTURE:" in process.stdout, process
            return result

        # Independent committed text matrix: no audio or golden DSP hashes.
        assert len(rows) == 32
        valid_forms = [valid, "\n".join(rows) + "\n", "\n".join(reversed(rows)),
                       "\ufeff\r\n" + "\r\n".join("\t " + "\t".join(line.split()) + "  "
                                                      for line in valid.splitlines()) + "\r\n",
                       valid.replace("\n", "\r"),
                       valid.replace("0000000000000001", "ABCDEF0123456789"),
                       valid.replace(first, first.rsplit(maxsplit=1)[0] + " 0000"),
                       valid.replace(first, first.rsplit(maxsplit=1)[0] + " 2147483647")]
        for text in valid_forms:
            process, result = run(text, text)
            assert process.returncode == 0 and result["classification"] == "IDENTICAL", result
            assert result["hashes"]["compared_scenarios"] == 32, result
            assert result["latency"]["status"] == "EQUAL", result
        _, reordered = run(valid, "\n".join(reversed(rows)))
        assert reordered["classification"] == "IDENTICAL", reordered
        _, normalized = run(valid, valid.replace("000000000000000a", "000000000000000A")
                             .replace(first, first.rsplit(maxsplit=1)[0] + " 0000"))
        assert normalized["classification"] == "IDENTICAL" and normalized["latency"]["status"] == "EQUAL", normalized

        process, numerical = run(valid, changed)
        assert process.returncode == 1 and numerical["classification"] == "NUMERICAL", numerical
        assert numerical["hashes"]["differences"] == [
            {"scenario": "haas-os1-lr", "A": "0000000000000001", "B": "ffffffffffffffff"}], numerical
        assert numerical["latency"]["status"] == "EQUAL" and "::error::NUMERICAL:" in process.stdout, numerical

        latency_only = valid.replace(first, first.rsplit(maxsplit=1)[0] + " 123")
        process, metadata = run(valid, latency_only)
        assert process.returncode == 0 and metadata["classification"] == "IDENTICAL", metadata
        assert metadata["hashes"]["differences"] == [], metadata
        assert metadata["latency"]["differences"] == [{"scenario": "haas-os1-lr", "A": 0, "B": 123}], metadata
        assert "::warning::METADATA:" in process.stdout, process.stdout

        invalid = [
            (valid.replace(rows[-1] + "\n", ""), "missing scenarios: dimensional-os8-ms"),
            (valid + "unknown-os1-lr 0000000000000021 0\n", "unknown scenario"),
            (valid.replace("haas-os1-lr", "unknown-os1-lr"), "unknown scenario"),
            (valid.replace("haas-os1-lr", "Haas-os1-lr"), "unknown scenario"),
            (valid + first + "\n", "duplicate identity"),
            (valid + first.replace("0000000000000001", "ffffffffffffffff") + "\n", "duplicate identity"),
            (overwrite, "duplicate identity"),
            (valid.replace(first, "haas-os1-lr 0000000000000001"), "exactly"),
            (valid.replace(first, first + " extra"), "exactly"),
            (valid.replace(first, "haas-os1-lr"), "exactly"),
            (valid.replace("0000000000000001", "000000000000001"), "16-hex"),
            (valid.replace("0000000000000001", "g000000000000001"), "16-hex"),
            ("", "missing scenarios"), ("\n\t\n", "missing scenarios"),
            (valid[:valid.index(rows[20])], "missing scenarios"),
            (valid + "record-like malformed input\n", "unsupported output"),
            (valid.replace("self-check passed:", "self-check FAILED:"), "unsupported output"),
            (valid + "# haas-os1-lr ffffffffffffffff 0\n", "unsupported output"),
            (valid + "# arbitrary header\n", "unsupported output"),
            (valid.replace(rows[0] + "\n" + rows[1], rows[0] + "\v" + rows[1]), "exactly"),
            (b"\xff\xfe", "codec"), (None, "right.txt"),
        ]
        for token in ("bad", "-1", "+1", "1.0", "1e2", "0x10", "2147483648", "9" * 5000):
            invalid.append((valid.replace(first, first.rsplit(maxsplit=1)[0] + " " + token), "latency"))
        for text, reason in invalid:
            refuses(valid, text, reason)
            refuses(valid, text, reason, "c")
        refuses(overwrite, valid, "duplicate identity")
        refuses(overwrite, changed, "duplicate identity")  # invalid A precedes any hash verdict
        both = refuses("", "", "missing scenarios")
        assert len(both["errors"]) == 2, both
        for text in (valid, changed, latency_only):
            process, result = run(valid, text, "c")
            assert process.returncode == 0 and result["classification"] == "INFORMATIONAL", result
            assert "::error::" not in process.stdout, process.stdout

        # C never changes the already recorded A/B disposition, including when
        # C is absent, ambiguous or a valid numerical/metadata difference.
        _, ab = run(valid, valid)
        ab_report = root / "completed-ab.json"
        ab_report.write_text(json.dumps(ab), encoding="utf-8")
        before = ab_report.read_bytes()
        for text in (None, overwrite, changed, latency_only):
            process, _ = run(valid, text, "c")
            assert process.returncode == 0 and ab_report.read_bytes() == before

        # The exact regression assertion is reused against a seeded production
        # mutant. Disabling only duplicate refusal recreates last-write-wins:
        # changed hash then baseline duplicate -> 32 keys, falsely IDENTICAL.
        mutant = root / "overwrite-mutant.py"
        source = helper.read_text(encoding="utf-8")
        mutant.write_text(source.replace("if scenario in records:", "if False and scenario in records:", 1), encoding="utf-8")
        try:
            refuses(valid, overwrite, "duplicate identity", script=mutant)
        except AssertionError:
            mutant_result = json.loads((root / "result.json").read_text(encoding="utf-8"))
            assert mutant_result["classification"] == "IDENTICAL", mutant_result
            assert mutant_result["inputs"]["B"]["record_count"] == 32, mutant_result
        else:
            raise AssertionError("duplicate-overwrite mutant survived the production-entry regression")

        def output_collision(option: str, input_option: str, alias_kind: str, mode: str = "ab",
                             script: Path = helper, collides: bool = True) -> None:
            nonlocal cases
            cases += 1
            a, b = root / "protected-left.txt", root / "protected-right.txt"
            report, summary = root / "separate-report.json", root / "separate-summary.txt"
            alias, unrelated = root / "evidence-alias", root / "unrelated-output.txt"
            alias.unlink(missing_ok=True)
            before = valid.encode("utf-8")
            a.write_bytes(before)
            b.write_bytes(before)
            report.write_bytes(b"report untouched\n")
            summary.write_bytes(b"summary untouched\n")
            unrelated.write_bytes(b"unrelated output\n")
            target = a if input_option == "--left" else b
            if not collides:
                target = unrelated
            if alias_kind == "same":
                destination = target
            elif alias_kind == "relative":
                destination = Path(target.name)  # Inputs are absolute; subprocess cwd is root.
            elif alias_kind == "dotdot":
                (root / "alias-directory").mkdir(exist_ok=True)
                destination = root / "alias-directory" / ".." / target.name
            elif alias_kind == "symlink":
                alias.symlink_to(target)
                destination = alias
            elif alias_kind == "hardlink":
                alias.hardlink_to(target)
                destination = alias
            else:
                raise AssertionError(alias_kind)
            command = [sys.executable, str(script), mode, "--left", str(a), "--right", str(b),
                       "--report", str(destination if option == "--report" else report),
                       "--summary", str(destination if option == "--summary" else summary)]
            process = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=15)
            # Check bytes first: the bypass mutant must fail for this exact defect.
            assert a.read_bytes() == b.read_bytes() == before, "raw dump bytes changed"
            if collides:
                assert report.read_bytes() == b"report untouched\n", "report was written before summary refusal"
                assert summary.read_bytes() == b"summary untouched\n", "summary was written before report refusal"
                assert process.returncode == (2 if mode == "ab" else 0), process
                assert ("::error::INFRASTRUCTURE:" if mode == "ab" else "::warning::informational C:") in process.stdout, process
                assert option in process.stdout and input_option in process.stdout, process.stdout
                assert "refusing all evidence writes" in process.stdout, process.stdout
                assert "IDENTICAL" not in process.stdout and "scenario hashes differ" not in process.stdout, process.stdout
                if mode == "c":
                    assert "::error::" not in process.stdout, process.stdout
            else:
                assert process.returncode == 0, process
                data = json.loads((destination if option == "--report" else report).read_text(encoding="utf-8"))
                assert data["classification"] == ("IDENTICAL" if mode == "ab" else "INFORMATIONAL"), data

        # Symlink creation can require a privilege absent on Windows. Report
        # that limitation explicitly; canonical-path and hard-link cases remain
        # mandatory everywhere, without changing host privileges or settings.
        link_kinds = ["hardlink"]
        probe = root / "symlink-probe"
        try:
            probe.symlink_to(root / "left.txt")
        except (OSError, NotImplementedError) as error:
            print(annotation("warning", f"DSP dump symlink fixtures unavailable: {error}"))
        else:
            probe.unlink()
            link_kinds.append("symlink")
        for mode in ("ab", "c"):
            for option in ("--report", "--summary"):
                for source_option in ("--left", "--right"):
                    for alias_kind in ("same", "relative", "dotdot", *link_kinds):
                        output_collision(option, source_option, alias_kind, mode)
                for alias_kind in link_kinds:
                    output_collision(option, "--left", alias_kind, mode, collides=False)

        guard_call = "        check_evidence_paths(args.left, args.right, args.report, args.summary)"
        guard_mutant = root / "evidence-guard-bypass.py"
        seeded = source.replace(guard_call, "        pass  # seeded evidence guard bypass", 1)
        assert seeded != source, "evidence guard mutant was not seeded"
        guard_mutant.write_text(seeded, encoding="utf-8")
        for option in ("--report", "--summary"):
            try:
                output_collision(option, "--left", "same", script=guard_mutant)
            except AssertionError as error:
                assert str(error) == "raw dump bytes changed", str(error)
            else:
                raise AssertionError(f"evidence guard bypass survived the {option} raw-byte assertion")

        # Optional C evidence I/O failure must be warning-only as well.
        for mode, code, level in (("ab", 2, "error"), ("c", 0, "warning")):
            process = subprocess.run([sys.executable, str(helper), mode, "--left", str(root / "left.txt"),
                                      "--right", str(root / "right.txt"), "--report", str(root)],
                                     capture_output=True, text=True, timeout=15)
            assert process.returncode == code and f"::{level}::" in process.stdout, process
            report = root / "summary-failure.json"
            process = subprocess.run([sys.executable, str(helper), mode, "--left", str(root / "left.txt"),
                                      "--right", str(root / "right.txt"), "--report", str(report),
                                      "--summary", str(root)], capture_output=True, text=True, timeout=15)
            result = json.loads(report.read_text(encoding="utf-8"))
            assert process.returncode == result["exit_code"] == code and f"::{level}::" in process.stdout, process
            assert result["classification"] == ("INFRASTRUCTURE" if mode == "ab" else "INFORMATIONAL_UNAVAILABLE"), result
    print(f"DSP dump parser self-test: {cases} production-entry fixtures; "
          "duplicate-overwrite and evidence-guard-bypass mutants rejected.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
