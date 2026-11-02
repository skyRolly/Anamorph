"""Owner-run native evidence collection. No release/install/publication actions.

Reuses AnamorphBench and Ninja's expanded actual commands. Only the original
engine/bench objects are substituted, with both TUs seeing the same old layout.
The remaining objects, flags, compiler and linker are retained. Original peeks
are serial; actual host/GUI observations remain a separate manual worksheet.
"""
from __future__ import annotations

import argparse
import collections
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path, PureWindowsPath
import platform
import re
import shlex
import signal
import statistics
import subprocess
import sys
import struct
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/fixtures/scope-legacy"


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def save(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def capture(argv, cwd=None):
    result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", check=False, timeout=60)
    return {"argv": [str(x) for x in argv], "exit": result.returncode,
            "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}


def required(argv, cwd=None):
    result = capture(argv, cwd)
    if result["exit"]:
        raise RuntimeError(f"command failed: {argv}: {result['stderr']}")
    return result["stdout"]


def windows_words(command):
    """MS CRT double-quote/backslash rules, independent of the test host OS."""
    result, i = [], 0
    while i < len(command):
        while i < len(command) and command[i] in " \t":
            i += 1
        if i == len(command):
            break
        word, quoted = [], False
        while i < len(command) and (quoted or command[i] not in " \t"):
            slashes = 0
            while i < len(command) and command[i] == "\\":
                slashes += 1
                i += 1
            if i < len(command) and command[i] == '"':
                word.extend("\\" * (slashes // 2))
                if slashes % 2:
                    word.append('"')
                elif quoted and i + 1 < len(command) and command[i + 1] == '"':
                    word.append('"')
                    i += 1
                else:
                    quoted = not quoted
                i += 1
            else:
                word.extend("\\" * slashes)
                if i < len(command) and (quoted or command[i] not in " \t"):
                    word.append(command[i])
                    i += 1
        if quoted:
            raise ValueError("unclosed quote in native build command")
        result.append("".join(word))
    return result


def command_args(command, windows=None):
    windows = os.name == "nt" if windows is None else windows
    command = command.strip()
    # Remove only CMake/Ninja's known no-op shell wrappers. Never run a shell.
    launcher = re.match(r'^(?:"([^"]+)"|(\S+))\s+/[cC]\s+([\s\S]+)$', command) if windows else None
    if launcher and PureWindowsPath(launcher[1] or launcher[2]).name.lower() == "cmd.exe":
        command = launcher[3].strip()
        if command.startswith('"') and command.endswith('"'):
            command = command[1:-1]
    for prefix in (": && ", "cd . && "):
        if command.startswith(prefix):
            command = command[len(prefix):]
    for suffix in (" && :", " && cd ."):
        if command.endswith(suffix):
            command = command[:-len(suffix)]
    args = windows_words(command) if windows else shlex.split(command)
    if not args or any(x in ("&&", "||", ";", "|") or x.startswith("@") for x in args):
        raise ValueError("unexpected shell/response-file command; require Ninja compdb -x")
    if windows and PureWindowsPath(args[0]).name.lower() == "cmd.exe":
        raise ValueError("unsupported cmd launcher: do not execute a shell")
    return args


def output_path(command):
    if "-o" in command:
        return command[command.index("-o") + 1]
    matches = [x[5:] for x in command if x.lower().startswith("/out:")]
    if not matches:
        matches = [x[3:] for x in command if x.lower().startswith("/fo")]
    if len(matches) != 1:
        raise ValueError("missing/ambiguous compiler or linker output")
    return matches[0]


def with_output(command, output):
    result = command.copy()
    if "-o" in result:
        result[result.index("-o") + 1] = str(output)
    else:
        matches = [i for i, x in enumerate(result) if x.lower().startswith(("/fo", "/out:"))]
        if len(matches) != 1:
            raise ValueError("missing/ambiguous output option")
        index = matches[0]
        result[index] = ("/out:" if result[index].lower().startswith("/out:") else "/Fo") + str(output)
    for flag in ("-MF", "-MT"):
        if flag in result:
            result[result.index(flag) + 1] = str(output) + (".d" if flag == "-MF" else "")
    for i, value in enumerate(result):
        if value.lower().startswith(("/fd", "/pdb:")):
            result[i] = ("/Fd" if value.lower().startswith("/fd") else "/pdb:") + str(output) + ".pdb"
        elif value.startswith("--intdir="):
            directory = Path(str(output) + ".intdir")
            directory.mkdir(exist_ok=True)
            result[i] = "--intdir=" + str(directory)
        elif value.startswith("--dependency-file="):
            result[i] = "--dependency-file=" + str(output) + ".d"
        elif value.lower().startswith("/implib:"):
            result[i] = "/implib:" + str(output) + ".lib"
    return result


def run_logged(argv, cwd, stdout, stderr, timeout=3600):
    """Bounded own process group; completed logs survive exceptions/signals."""
    kwargs = {"start_new_session": True} if os.name != "nt" else {
        "creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
    with Path(stdout).open("w", encoding="utf-8") as out, Path(stderr).open("w", encoding="utf-8") as err:
        process = subprocess.Popen([str(x) for x in argv], cwd=cwd, stdout=out, stderr=err, **kwargs)
        try:
            code = process.wait(timeout=timeout)
        except BaseException:
            if os.name == "nt":
                # Hosted/headless Windows may have no console for CTRL_BREAK.
                # Restrict forced cleanup to this exact owned process tree.
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, timeout=10)
            else:
                os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                if os.name == "nt":
                    subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True)
                else:
                    os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            raise
    if code:
        raise RuntimeError(f"exit {code}: {argv}; see {stdout} and {stderr}")


def machine():
    data = {"captured_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "os": platform.platform(), "os_version": platform.version(),
            "process_architecture": platform.machine(), "logical_cpus": os.cpu_count(),
            "python": sys.version, "probes": {}, "virtualization_observations": [],
            "representative_acceptance": None}
    arch = platform.machine().lower()
    data["supported_product_platform"] = (arch in ("arm64", "x86_64") if sys.platform == "darwin" else
                                          arch in ("amd64", "x86_64") if os.name == "nt" else
                                          sys.platform == "linux" and arch == "x86_64")
    commands = {}
    if sys.platform == "darwin":
        commands = {"os": ["sw_vers"], "cpu": ["sysctl", "-n", "machdep.cpu.brand_string"],
                    "model": ["sysctl", "-n", "hw.model"], "physical_cpus": ["sysctl", "-n", "hw.physicalcpu"],
                    "memory": ["sysctl", "-n", "hw.memsize"],
                    "translated": ["sysctl", "-n", "sysctl.proc_translated"]}
    elif os.name == "nt":
        commands = {"cpu": ["powershell", "-NoProfile", "-Command",
                            "Get-CimInstance Win32_Processor | Select-Object Name,Architecture,NumberOfCores,NumberOfLogicalProcessors | ConvertTo-Json"],
                    "os": ["powershell", "-NoProfile", "-Command",
                           "Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,OSArchitecture | ConvertTo-Json"],
                    "power": ["powercfg", "/getactivescheme"]}
    else:
        cpuinfo = Path("/proc/cpuinfo")
        if cpuinfo.exists():
            lines = cpuinfo.read_text().splitlines()
            data["cpu_model"] = next((x.split(":", 1)[1].strip() for x in lines if x.startswith("model name")), "")
        commands = {"virtualization": ["systemd-detect-virt"], "architecture": ["uname", "-m"]}
        for name in ("/proc/loadavg", "/sys/fs/cgroup/cpu.max", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"):
            p = Path(name)
            try:
                data["probes"][name] = p.read_text().strip()
            except OSError as error:
                data["probes"][name] = {"unavailable": str(error)}
        if Path("/.dockerenv").exists():
            data["virtualization_observations"].append("container marker /.dockerenv")
    for key, argv in commands.items():
        try:
            data["probes"][key] = capture(argv)
        except (OSError, subprocess.TimeoutExpired) as error:
            data["probes"][key] = {"unavailable": str(error)}
    if os.environ.get("CI") or os.environ.get("GITHUB_ACTIONS"):
        data["virtualization_observations"].append("CI environment")
    for key in ("virtualization", "translated"):
        probe = data["probes"].get(key, {})
        if probe.get("stdout") not in (None, "", "none", "0"):
            data["virtualization_observations"].append(f"{key}: {probe['stdout']}")
    if not data.get("cpu_model"):
        data["cpu_model"] = data["probes"].get("cpu", {}).get("stdout", "")
        if os.name == "nt" and data["cpu_model"]:
            cpus = json.loads(data["cpu_model"])
            data["cpu_model"] = "; ".join(x["Name"] for x in (cpus if isinstance(cpus, list) else [cpus]))
    return data


def stimulus(output):
    """Deterministic, moderate-level stereo PCM; never installs or plays audio."""
    rate = 48000
    segments = [("active", 0, 4), ("silence", 4, 7), ("active", 7, 11), ("silence", 11, 14)]
    path = output / "playback-fixture.wav"
    with wave.open(str(path), "wb") as wav:
        wav.setparams((2, 2, rate, 0, "NONE", "not compressed"))
        for name, start, end in segments:
            frames = bytearray()
            for frame in range(start * rate, end * rate):
                t = frame / rate
                # Integer rounding is recorded by the output hash, not assumed
                # identical across different platform libm implementations.
                left = int(6000 * math.sin(2 * math.pi * 233 * t)) if name == "active" else 0
                right = int(5000 * math.sin(2 * math.pi * 349 * t)) if name == "active" else 0
                frames.extend(struct.pack("<hh", left, right))
            wav.writeframes(frames)
    save(output / "playback-fixture.json", {"sha256": digest(path), "rate": rate, "channels": 2,
                                             "pcm_bits": 16, "segments_seconds": segments})


def compiler_supported(description, system=None):
    system = sys.platform if system is None else system
    identity = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)', description)
    version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)', description)
    if not identity or not version:
        return False
    family, version = identity[1], tuple(int(x) for x in version[1].split(".")[:2])
    return (family == "AppleClang" if system == "darwin" else
            family == "MSVC" and version >= (19, 30) if system == "win32" else
            family == "Clang" and version[0] == 22)


def paired_c_compiler(cxx):
    if not cxx:
        return None
    path = Path(cxx)
    for cpp, c in (("clang++", "clang"), ("g++", "gcc")):
        if path.name.startswith(cpp):
            return str(path.with_name(path.name.replace(cpp, c, 1)))
    return cxx if path.name.lower() in ("cl", "cl.exe") else None


def path_key(value, windows=None):
    windows = os.name == "nt" if windows is None else windows
    value = value.replace("\\", "/")
    return value.casefold() if windows else value


def plugin_identities(build):
    root = build / "Anamorph_artefacts/Release"
    result = {}
    for format_name in ("VST3", "Standalone", "AU"):
        directory = root / format_name
        result[format_name] = {p.relative_to(directory).as_posix(): digest(p)
                               for p in sorted(directory.rglob("*")) if p.is_file() and not p.is_symlink()}
    return {"build_tree": str(root), "files_sha256": result,
            "loaded_host_module_verified": None, "note": "Match the actual host-loaded path; no automatic installation"}


def cache_values(build):
    result = {}
    for line in (Path(build) / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if not line.startswith(("#", "//")) and "=" in line and ":" in line.split("=", 1)[0]:
            name, value = line.split("=", 1)
            result[name.split(":", 1)[0]] = value
    return result


def source_info():
    tracked = required(["git", "ls-files", "src", "tests/bench.cpp", "tests/native_validation_bench.h",
                        "CMakeLists.txt", "cmake", "scripts/native_validation.py"], ROOT).splitlines()
    tracked = sorted(set(tracked) | {"tests/native_validation_bench.h", "scripts/native_validation.py", "scripts/run-native-validation.py",
                                    "worklogs/native-validation/manual-acceptance-template.json"})
    return {"commit": required(["git", "rev-parse", "HEAD"], ROOT),
            "status": required(["git", "status", "--porcelain"], ROOT),
            "files_sha256": {name: digest(ROOT / name) for name in tracked if (ROOT / name).is_file()}}


def make_plan(smoke=False, repetitions=5):
    cells = []
    for block in ([32, 16401] if smoke else [32, 64, 128, 256, 512, 1024, 16401]):
        cells.append({"kind": "scope-push", "case": "working", "rate": 48000, "block": block})
    for window in ([8192] if smoke else [1200, 8000, 8192]):
        cells.append({"kind": "scope-read", "case": "working", "rate": 48000, "block": 128, "window": window})
    scenarios = ["working", "silence", "os8", "mb-drag"] if smoke else [
        "defaults", "working", "silence", "bypass", "haas", "velvet", "chorus", "dimensional",
        "os2", "os4", "os8", "selected2", "selected4", "selected8", "mb-off", "mb-drag"]
    for scenario in scenarios:
        cells.append({"kind": "engine", "case": scenario, "rate": 48000, "block": 128})
    if not smoke:
        for rate in (44100, 96000, 192000):
            cells.append({"kind": "engine", "case": "working", "rate": rate, "block": 128})
        for block in (32, 64, 256, 512, 1024, 16401):
            cells.append({"kind": "engine", "case": "working", "rate": 48000, "block": block})
    cells.append({"kind": "memory", "case": "os8", "rate": 192000, "block": 16401})
    plan = []
    for cell in cells:
        for rep in range(repetitions):
            # Alternate order by repetition to expose order/thermal drift.
            for variant in (("original", "corrected") if rep % 2 == 0 else ("corrected", "original")):
                plan.append(dict(cell, variant=variant, rep=rep, reader="none", paced=False))
    for block in ([128] if smoke else [128, 16401]):
        for rep in range(repetitions):
            order = [("original", "none"), ("corrected", "none"),
                     ("corrected", "active"), ("corrected", "stalled")]
            for variant, reader in (order if rep % 2 == 0 else reversed(order)):
                plan.append({"kind": "engine", "case": "working", "rate": 48000, "block": block,
                             "variant": variant, "rep": rep, "reader": reader, "paced": True})
    for row in plan:
        row.setdefault("window", 8192)
        row["seconds"] = (1.1 if row["paced"] else 0.05) if smoke else 10.0
        row["iterations"] = 20 if smoke else 2000
        row["smoke"] = smoke
    return plan


def validate_record(row, request):
    for key in ("kind", "case", "variant", "rate", "block", "window", "reader", "paced", "rep", "seconds", "smoke"):
        if row.get(key) != request[key]:
            raise ValueError(f"probe/request mismatch: {key}")
    if row.get("schema") != 2 or not row.get("cpu"):
        raise ValueError("missing schema/CPU")
    if row.get("fp_denormals_disabled") is not True or row.get("fp_round_to_nearest") is not True:
        raise ValueError("missing/incorrect callback floating-point environment")
    if type(row.get("rss_available")) is not bool:
        raise ValueError("missing RSS availability disclosure")
    if row["variant"] == "original" and row["reader"] != "none":
        raise ValueError("unsafe original concurrent reader")
    values = row.get("block_ns")
    if not isinstance(values, list) or any(type(x) is not int or x < 0 for x in values):
        raise ValueError("invalid raw timings")
    expected = 0 if row["kind"] == "memory" else request["iterations"] if row["kind"].startswith("scope-") else math.ceil(row["seconds"] * row["rate"] / row["block"])
    if len(values) != expected:
        raise ValueError("incomplete raw timing vector")
    if values and not any(values):
        raise ValueError("clock/work control produced only zero timings")
    denominator = row["window"] if row["kind"] == "scope-read" else row["block"]
    if row.get("samples") != expected * denominator:
        raise ValueError("wrong frame count/normalization")
    if row["reader"] != "none" and row.get("reader_accepted", 0) <= 0:
        raise ValueError("vacuous reader load")
    if row["reader"] == "stalled" and row.get("stall_frames", 0) <= 0:
        raise ValueError("producer did not advance during stall")
    for key in ("audio_hash", "input_hash", "reader_hash"):
        if not re.fullmatch(r"[0-9a-f]{16}", row.get(key, "")):
            raise ValueError("missing data-consumption control hash")
    if row["kind"] == "engine" and row["case"] == "working" and row["audio_hash"] == row["input_hash"]:
        raise ValueError("working engine is inert")
    if row["kind"].startswith("scope-"):
        expected_end = 16384 + (request["iterations"] * row["block"] if row["kind"] == "scope-push" else 0)
        if row.get("scope_start") != 16384 or row.get("scope_end") != expected_end:
            raise ValueError("scope publication control failed")
        if row["audio_hash"] == "cbf29ce484222325":
            raise ValueError("scope data consumption control is inert")
    for key in ("engine_bytes", "scope_bytes", "rss_before", "rss_constructed", "rss_prepared",
                "reader_accepted", "reader_rejected", "stall_frames", "clock_min_ns", "clock_max_ns", "latency_samples", "scope_start", "scope_end"):
        if type(row.get(key)) is not int or row[key] < 0:
            raise ValueError(f"invalid numeric observation: {key}")
    if row["clock_min_ns"] > row["clock_max_ns"]:
        raise ValueError("reversed clock calibration")
    if row["rss_available"] and row["rss_prepared"] <= 0:
        raise ValueError("RSS availability contradicts its observation")


def summarize(records, repetitions, smoke):
    if not smoke and repetitions < 5:
        raise ValueError("need at least five repetitions")
    groups = collections.defaultdict(list)
    pairs = collections.defaultdict(dict)
    for row in records:
        key = tuple(row[k] for k in ("kind", "case", "rate", "block", "window", "reader", "paced"))
        groups[(row["variant"], *key)].append(row)
        if row["reader"] == "none":
            pair = pairs[(*key, row["rep"])]
            if row["variant"] in pair:
                raise ValueError("duplicate variant/repetition")
            pair[row["variant"]] = row
    for variants in pairs.values():
        if set(variants) != {"original", "corrected"}:
            raise ValueError("incomplete paired comparison")
        if (variants["original"]["audio_hash"] != variants["corrected"]["audio_hash"]
                or variants["original"]["latency_samples"] != variants["corrected"]["latency_samples"]):
            raise ValueError("paired engine output differs; investigate, do not infer a performance win")
    metrics = []
    for key, rows in groups.items():
        if sorted(x["rep"] for x in rows) != list(range(repetitions)):
            raise ValueError("missing or duplicate repetition")
        averages = [sum(x["block_ns"]) / x["samples"] for x in rows if x["samples"] > 0]
        median = statistics.median(averages) if averages else None
        metrics.append({"variant": key[0], "kind": key[1], "case": key[2], "rate": key[3], "block": key[4],
                        "window": key[5], "reader": key[6], "paced": key[7], "repetitions": repetitions,
                        "median_ns_per_frame": median, "min_ns_per_frame": min(averages) if averages else None,
                        "max_ns_per_frame": max(averages) if averages else None,
                        "spread_percent": (max(averages) - min(averages)) / median * 100 if median else None,
                        "worst_observed_call_ns": max((max(x["block_ns"]) for x in rows if x["block_ns"]), default=None),
                        "rss_prepare_deltas": [x["rss_prepared"] - x["rss_constructed"] if x["rss_available"] else None for x in rows],
                        "scope_bytes": rows[0]["scope_bytes"], "engine_bytes": rows[0]["engine_bytes"]})
    lookup = {(m["kind"], m["case"], m["rate"], m["block"], m["window"], m["paced"]): m
              for m in metrics if m["variant"] == "original" and m["reader"] == "none"}
    comparisons = []
    for corrected in metrics:
        key = tuple(corrected[k] for k in ("kind", "case", "rate", "block", "window", "paced"))
        if corrected["variant"] != "corrected" or corrected["reader"] != "none" or key not in lookup:
            continue
        original = lookup[key]
        old, new = original["median_ns_per_frame"], corrected["median_ns_per_frame"]
        comparisons.append({"kind": key[0], "case": key[1], "rate": key[2], "block": key[3],
                            "window": key[4], "paced": key[5], "original": original, "corrected": corrected,
                            "median_delta_percent": (new - old) / old * 100 if old and new else None,
                            "disposition": "requires_context_and_owner_cost_review"})
    return {"schema": 1, "smoke": smoke, "measurement_records": len(records), "metrics": metrics, "comparisons": comparisons,
            "performance_acceptance": None, "manual_acceptance": None,
            "limits": ["Observed wall-clock calls, not WCET or audio-device callback deadlines",
                       "Reader traffic excludes actual rendering, FFT and host overhead",
                       "Clock overhead is reported and not subtracted", "RSS deltas are process observations, not exact allocator totals"]}


def build_probes(build, output, jobs, skip_plugin_build):
    cache = cache_values(build)
    if cache.get("CMAKE_GENERATOR") != "Ninja" or cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("use a Release Ninja build (existing CMake options; no generator migration)")
    if any(cache.get(k) != "ON" for k in ("ANAMORPH_BUILD_BENCH", "ANAMORPH_BUILD_TESTS")):
        raise ValueError("CMake reset the requested opt-in targets; use a fresh build directory after a compiler change")
    targets = ["AnamorphBench", "AnamorphTests", "AnamorphStateTests"]
    if not skip_plugin_build:
        targets += ["Anamorph_VST3", "Anamorph_Standalone"]
        if sys.platform == "darwin":
            targets += ["Anamorph_AU"]
    run_logged(["cmake", "--build", str(build), "--target", *targets, "-j", str(jobs)], ROOT,
               output / "build.stdout.log", output / "build.stderr.log")
    save(output / "plugin-build-identity.json", plugin_identities(build))
    database = json.loads(required(["ninja", "-C", str(build), "-t", "compdb", "-x"]))
    def find(predicate):
        matches = [x for x in database if predicate(x)]
        if len(matches) != 1:
            raise ValueError("ambiguous/missing AnamorphBench build command")
        return matches[0]
    bench = find(lambda x: "AnamorphBench.dir" in x.get("output", "") and x.get("file", "").replace("\\", "/").endswith("tests/bench.cpp"))
    engine = find(lambda x: "AnamorphBench.dir" in x.get("output", "") and x.get("file", "").replace("\\", "/").endswith("src/dsp/AnamorphEngine.cpp"))
    link = find(lambda x: bool(x.get("command")) and x.get("output", "").replace("\\", "/").split("/")[-1] in ("AnamorphBench", "AnamorphBench.exe"))
    save(output / "ninja-owned-commands.json", {"bench": bench, "engine": engine, "link": link})
    commands = {"bench": command_args(bench["command"]), "engine": command_args(engine["command"]),
                "link": command_args(link["command"])}
    save(output / "actual-build-commands.json", commands)
    flags = " ".join(commands["bench"] + commands["engine"] + commands["link"])
    lower = flags.lower()
    compiler_files = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    compiler = "\n".join(p.read_text(encoding="utf-8") for p in compiler_files)
    juce = Path(cache["ANAMORPH_JUCE_PATH"]) if cache.get("ANAMORPH_JUCE_PATH") else build / "_deps/juce-src"
    juce_identity = {"path": str(juce), "commit": required(["git", "rev-parse", "HEAD"], juce),
                     "status": required(["git", "status", "--porcelain"], juce)}
    config_observations = {"release": cache.get("CMAKE_BUILD_TYPE") == "Release",
                           "supported_native_compiler": compiler_supported(compiler),
                           "pinned_clean_juce": juce_identity["commit"] == cache.get("ANAMORPH_JUCE_TAG") and not juce_identity["status"],
                           "lto": any(x.lower().startswith("-flto") or x.lower() in ("/gl", "-gl", "/ltcg", "-ltcg")
                                      for x in commands["bench"] + commands["engine"] + commands["link"]),
                           "ndebug": any(x in lower for x in ("-dndebug", "/dndebug")),
                           "isa_baseline": cache.get("ANAMORPH_X86_ISA_BASELINE") == "ON",
                           "unsupported_measurement_flags": [x for x in ("-ffast-math", "/fp:fast", "/fp:contract", "-ffp-contract=fast", "-fsanitize", "--coverage") if x in lower]}
    save(output / "configuration-observations.json", config_observations)
    original = output / "original-overlay"
    dsp = original / "src/dsp"
    dsp.mkdir(parents=True)
    for name in ("AnamorphEngine.h", "AnamorphEngine.cpp"):
        (dsp / name).write_bytes((ROOT / "src/dsp" / name).read_bytes())
    metadata = read(LEGACY / "provenance.json")
    # Read the committed canonical bytes: Windows checkout CRLF must not turn
    # an unchanged historical source into a different experimental baseline.
    fixture = subprocess.check_output(["git", "show", "HEAD:tests/fixtures/scope-legacy/ScopeBuffer.h"], cwd=ROOT)
    blob = hashlib.sha1(b"blob " + str(len(fixture)).encode() + b"\0" + fixture).hexdigest()
    if hashlib.sha256(fixture).hexdigest() != metadata["sha256"] or blob != metadata["git_blob"] or blob != "ed24ceab5ced2784d9fe96effdc46f8aeb03f472":
        raise ValueError("historical scope fixture changed")
    (dsp / "ScopeBuffer.h").write_bytes(fixture)
    objects, derived = {}, {}
    for name in ("bench", "engine"):
        command = commands[name].copy()
        old_object = output_path(command)
        obj = original / (name + (".obj" if os.name == "nt" else ".o"))
        objects[old_object] = str(obj)
        if name == "engine":
            source = next((i for i, x in enumerate(command) if x.replace("\\", "/").endswith("src/dsp/AnamorphEngine.cpp")), None)
            if source is None:
                raise ValueError("missing engine source operand")
            command[source] = str(dsp / "AnamorphEngine.cpp")
        # Prefix include search before the real source directory; both TUs use
        # the same old Engine layout. No production/header file is rewritten.
        prefix = ["/DANAMORPH_NATIVE_SCOPE_LEGACY=1", "/I" + str(original / "src"), "/I" + str(ROOT / "src/dsp")] if os.name == "nt" else [
            "-DANAMORPH_NATIVE_SCOPE_LEGACY=1", "-I" + str(original / "src"), "-I" + str(ROOT / "src/dsp")]
        compiler_name = Path(cache.get("CMAKE_CXX_COMPILER", "")).name.lower()
        if Path(command[0]).name.lower() != compiler_name:
            raise ValueError("compiler launcher not supported by the overlay: use a fresh build without a launcher")
        command[1:1] = prefix
        command = with_output(command, obj)
        derived[name] = command
        run_logged(command, build, output / ("original-" + name + ".stdout.log"), output / ("original-" + name + ".stderr.log"))
    original_binary = original / ("AnamorphBench-original.exe" if os.name == "nt" else "AnamorphBench-original")
    original_link = commands["link"].copy()
    for old, new in objects.items():
        matches = [i for i, value in enumerate(original_link) if path_key(value) == path_key(old)]
        if len(matches) != 1:
            raise ValueError("original link must replace each owned object exactly once")
        original_link[matches[0]] = new
    original_link = with_output(original_link, original_binary)
    derived["link"] = original_link
    save(output / "original-build-commands.json", derived)
    run_logged(original_link, build, output / "original-link.stdout.log", output / "original-link.stderr.log")
    selected = {k: v for k, v in cache.items() if k.startswith(("CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS", "ANAMORPH_"))
                or k in ("CMAKE_BUILD_TYPE", "CMAKE_GENERATOR", "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_OSX_ARCHITECTURES", "CMAKE_SYSTEM_PROCESSOR")}
    save(output / "build-identity.json", {"cache": selected, "compiler_description": compiler,
                                          "c_compiler_description": "\n".join(p.read_text(encoding="utf-8") for p in (build / "CMakeFiles").glob("*/CMakeCCompiler.cmake")),
                                          "juce": juce_identity,
                                          "original_fixture": metadata, "ninja": required(["ninja", "--version"]),
                                          "cmake": required(["cmake", "--version"])})
    binaries = {"original": original_binary, "corrected": build / output_path(commands["link"])}
    save(output / "binaries.json", {k: {"path": str(v), "sha256": digest(v)} for k, v in binaries.items()})
    return binaries


def collect(args):
    output, build = args.output_dir.resolve(), args.build_dir.resolve()
    if output.is_relative_to(ROOT):
        raise ValueError("keep evidence outside the checkout so it cannot dirty the measured source")
    info = source_info()
    if info["status"] and not args.smoke:
        raise ValueError("acceptance collection requires a clean checkout; --smoke is diagnostic only")
    if args.repetitions < (1 if args.smoke else 5):
        raise ValueError("normal collection requires at least five repetitions")
    output.mkdir(parents=True, exist_ok=False)
    state = {"state": "preparing", "performance_acceptance": None, "manual_acceptance": None,
             "source_commit": info["commit"], "completed": [], "current": None}
    save(output / "run-status.json", state)
    save(output / "source.json", info)
    try:
        environment = machine()
        environment["owner_machine_note"] = args.machine_note
        environment["smoke"] = args.smoke
        save(output / "machine.json", environment)
        configure = ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                     "-DANAMORPH_BUILD_BENCH=ON", "-DANAMORPH_BUILD_TESTS=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
        if args.juce_path:
            configure.append("-DANAMORPH_JUCE_PATH=" + str(args.juce_path.resolve()))
        if args.cxx:
            configure.append("-DCMAKE_CXX_COMPILER=" + args.cxx)
        cc = args.cc or paired_c_compiler(args.cxx)
        if cc:
            configure.append("-DCMAKE_C_COMPILER=" + cc)
        run_logged(configure, ROOT, output / "configure.stdout.log", output / "configure.stderr.log")
        binaries = build_probes(build, output, args.jobs, args.skip_plugin_build)
        observations = read(output / "configuration-observations.json")
        if not args.smoke and (not all(observations[k] for k in ("release", "lto", "ndebug", "isa_baseline", "supported_native_compiler", "pinned_clean_juce"))
                               or observations["unsupported_measurement_flags"]):
            raise ValueError("normal measurement requires the existing shipping-class Release/LTO/ISA configuration without instrumentation")
        if not environment["cpu_model"]:
            raise ValueError("CPU identity unavailable: do not emit unattributable performance results")
        if not args.smoke and not environment["supported_product_platform"]:
            raise ValueError("normal collection requires an existing supported native product platform")
        raw = output / "raw"
        raw.mkdir()
        plan = make_plan(args.smoke, args.repetitions)
        save(output / "plan.json", plan)
        worksheet = ROOT / "worklogs/native-validation/manual-acceptance-template.json"
        save(output / "manual-acceptance.json", read(worksheet))
        stimulus(output)
        state["state"] = "measuring"
        # The DSP suite supplies the existing native allocation/liveness checks.
        # Do not automatically run the state suite against an owner's live user
        # profile; it is available for a separately isolated test account.
        dsp_exe = build / "AnamorphTests_artefacts/Release" / ("AnamorphTests.exe" if os.name == "nt" else "AnamorphTests")
        run_logged([str(dsp_exe)], ROOT, output / "dsp.stdout.log", output / "dsp.stderr.log")
        records = []
        for index, request in enumerate(plan):
            stem = f"{index:04d}"
            state["current"] = {"index": index, "request": request, "state": "started"}
            save(output / "run-status.json", state)
            argv = [str(binaries[request["variant"]]), "--native-json"]
            for key in ("kind", "case", "rate", "block", "window", "seconds", "rep", "iterations", "reader"):
                argv += ["--" + key, str(request[key])]
            if request["smoke"]:
                argv.append("--smoke")
            if request["paced"]:
                argv.append("--paced")
            save(raw / (stem + ".request.json"), request)
            run_logged(argv, ROOT, raw / (stem + ".stdout.jsonl"), raw / (stem + ".stderr.log"), timeout=1800)
            row = read(raw / (stem + ".stdout.jsonl"))
            validate_record(row, request)
            records.append(row)
            state["completed"].append(index)
            state["current"] = None
            save(output / "run-status.json", state)
            print(f"{index + 1}/{len(plan)} {request['variant']} {request['kind']} {request['case']} block{request['block']} rep{request['rep']}", flush=True)
        identities = read(output / "binaries.json")
        for variant, binary in binaries.items():
            if digest(binary) != identities[variant]["sha256"]:
                raise ValueError("probe binary changed during measurement")
        if source_info()["files_sha256"] != info["files_sha256"]:
            raise ValueError("measured source changed during collection")
        summary = summarize(records, args.repetitions, args.smoke)
        summary["hardware_classification"] = "synthetic" if args.smoke or environment["virtualization_observations"] else "requires_owner_context_and_review"
        save(output / "summary.json", summary)
        state["state"] = "measurements_completed"
        state["current"] = None
    except BaseException as error:
        state["state"] = "interrupted" if isinstance(error, (KeyboardInterrupt, SystemExit)) else "failed"
        state["error"] = str(error)
        raise
    finally:
        save(output / "run-status.json", state)


def analyze(directory):
    directory = directory.resolve()
    state, plan = read(directory / "run-status.json"), read(directory / "plan.json")
    if state.get("state") != "measurements_completed" or state.get("completed") != list(range(len(plan))) or not plan:
        raise ValueError("incomplete collection: preserve and return its logs; do not call it acceptance evidence")
    smoke = plan[0]["smoke"]
    repetitions = max(x["rep"] for x in plan) + 1
    if make_plan(smoke, repetitions) != plan:
        raise ValueError("incomplete or modified measurement matrix")
    records = []
    for index, request in enumerate(plan):
        stem = f"{index:04d}"
        if read(directory / "raw" / (stem + ".request.json")) != request:
            raise ValueError("saved request no longer matches the plan")
        row = read(directory / "raw" / (stem + ".stdout.jsonl"))
        validate_record(row, request)
        records.append(row)
    summary = summarize(records, repetitions, smoke)
    environment = read(directory / "machine.json")
    source = read(directory / "source.json")
    if (not environment.get("os") or not environment.get("cpu_model")
            or not re.fullmatch(r"[0-9a-f]{40}", source.get("commit", ""))
            or state.get("source_commit") != source["commit"] or not source.get("files_sha256")):
        raise ValueError("missing machine/source provenance")
    if not read(directory / "build-identity.json").get("compiler_description"):
        raise ValueError("missing compiler provenance")
    observations = read(directory / "configuration-observations.json")
    if not smoke and (source.get("status") or not all(observations.get(k) for k in
                     ("release", "lto", "ndebug", "isa_baseline", "supported_native_compiler", "pinned_clean_juce"))
                     or observations.get("unsupported_measurement_flags")):
        raise ValueError("normal collection has incomplete/unsupported build provenance")
    if not smoke and not environment.get("supported_product_platform"):
        raise ValueError("normal collection requires supported platform provenance")
    summary["hardware_classification"] = "synthetic" if smoke or environment["virtualization_observations"] else "requires_owner_context_and_review"
    save(directory / "analysis.json", summary)
    return summary


def pack(directory, destination):
    directory, destination = directory.resolve(), destination.resolve()
    if destination.is_relative_to(directory):
        raise ValueError("archive must be outside evidence directory")
    manifest = {}
    for path in directory.rglob("*"):
        if path.is_symlink():
            raise ValueError("evidence symlinks are not archived")
        if path.is_file() and "original-overlay" not in path.relative_to(directory).parts:
            manifest[path.relative_to(directory).as_posix()] = digest(path)
    with zipfile.ZipFile(destination, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(manifest):
            archive.write(directory / name, name)
        archive.writestr("SHA256.json", json.dumps(manifest, indent=2) + "\n")
    return digest(destination)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    get = sub.add_parser("collect")
    get.add_argument("--build-dir", type=Path, required=True)
    get.add_argument("--output-dir", type=Path, required=True)
    get.add_argument("--juce-path", type=Path)
    get.add_argument("--cxx")
    get.add_argument("--cc", help="C compiler; defaults to matching clang/gcc/cl when --cxx identifies one")
    get.add_argument("--jobs", type=int, default=3)
    get.add_argument("--repetitions", type=int)
    get.add_argument("--machine-note", default="NOT RECORDED")
    get.add_argument("--smoke", action="store_true")
    get.add_argument("--skip-plugin-build", action="store_true")
    archive = sub.add_parser("pack")
    archive.add_argument("--evidence-dir", type=Path, required=True)
    archive.add_argument("--archive", type=Path, required=True)
    review = sub.add_parser("analyze")
    review.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.action == "collect":
        args.repetitions = args.repetitions if args.repetitions is not None else 1 if args.smoke else 5
        def terminated(signum, frame):
            raise SystemExit(128 + signum)
        previous = signal.signal(signal.SIGTERM, terminated)
        try:
            collect(args)
        finally:
            signal.signal(signal.SIGTERM, previous)
    elif args.action == "analyze":
        result = analyze(args.evidence_dir)
        print(f"{result['measurement_records']} validated records; performance/manual acceptance remain pending")
    else:
        print(pack(args.evidence_dir, args.archive))
    return 0
