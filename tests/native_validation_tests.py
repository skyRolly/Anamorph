#!/usr/bin/env python3
"""Parser/control fixtures, never hardware measurements or performance targets."""
import copy
import importlib.util
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_validation", ROOT / "scripts/native_validation.py")
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def request(**changes):
    row = dict(kind="engine", case="working", rate=48000, block=128, window=8192,
               variant="corrected", rep=0, reader="none", paced=False,
               seconds=0.05, iterations=20, smoke=True)
    row.update(changes)
    return row


def observation(req):
    n = 0 if req["kind"] == "memory" else req["iterations"] if req["kind"].startswith("scope-") else __import__("math").ceil(req["seconds"] * req["rate"] / req["block"])
    denominator = req["window"] if req["kind"] == "scope-read" else req["block"]
    start = 16384 if req["kind"].startswith("scope-") else 0
    end = start + (req["iterations"] * req["block"] if req["kind"] == "scope-push" else 0)
    return dict(req, schema=2, cpu="PARSER FIXTURE, NOT A MACHINE", block_ns=[10] * n,
                scope_start=start, scope_end=end,
                fp_denormals_disabled=True, fp_round_to_nearest=True,
                samples=n * denominator, scope_bytes=262152, engine_bytes=1000000,
                rss_before=100, rss_constructed=110, rss_prepared=150, rss_available=True,
                reader_accepted=1 if req["reader"] != "none" else 0, reader_rejected=0,
                stall_frames=20000 if req["reader"] == "stalled" else 0,
                clock_min_ns=1, clock_max_ns=2, latency_samples=0,
                input_hash="0000000000000001", audio_hash="0000000000000002", reader_hash="0000000000000003")


class CommandTests(unittest.TestCase):
    def test_windows_round_trip(self):
        cases = [["cl.exe", "/O2", "/GL", r'/IC:\Program Files\JUCE', '/DNAME="value"'],
                 [r"C:\Program Files\Compiler\cl.exe", "", "C:\\has space\\", 'a"b', r'one\"two'],
                 ["link.exe", r"/out:C:\has space\app.exe", r"C:\foo\\", "amp&&path", "semi;path"]]
        for case in cases:
            with self.subTest(case=case):
                self.assertEqual(native.windows_words(subprocess.list2cmdline(case)), case)

    def test_cmake_shell_wrappers_are_only_removed_not_executed(self):
        self.assertEqual(native.command_args(': && c++ "a b.o" -o app && :', False), ["c++", "a b.o", "-o", "app"])
        self.assertEqual(native.command_args('cmd.exe /C "cd . && cl.exe /O2 /Fofoo.obj foo.cpp && cd ."', True),
                         ["cl.exe", "/O2", "/Fofoo.obj", "foo.cpp"])
        # Real Ninja form from the Windows failed artifact: the launcher is an
        # absolute path and its outer quote encloses quoted paths with spaces.
        for launcher in (r'C:\Windows\system32\cmd.exe', r'"C:\Windows\system32\cmd.exe"'):
            command = launcher + r' /C "cd . && "C:\Program Files\CMake\bin\cmake.exe" -E vs_link_exe -- "C:\Program Files\VS\link.exe" CMakeFiles\Bench.dir\bench.obj /out:Bench.exe && cd ."'
            self.assertEqual(native.command_args(command, True),
                             [r'C:\Program Files\CMake\bin\cmake.exe', '-E', 'vs_link_exe', '--',
                              r'C:\Program Files\VS\link.exe', r'CMakeFiles\Bench.dir\bench.obj', '/out:Bench.exe'])
        with self.assertRaisesRegex(ValueError, 'unsupported cmd'):
            native.command_args(r'C:\Windows\system32\cmd.exe /D /C echo-me', True)
        for command in ("c++ foo.cpp && execute-me", "c++ @unexpanded.rsp", "c++ foo.cpp ; surprise"):
            with self.subTest(command=command), self.assertRaises(ValueError):
                native.command_args(command, False)

    def test_compiler_outputs_keep_numerical_flags(self):
        args = ["clang++", "-O3", "-flto", "-ffp-contract=off", "-march=haswell", "-MF", "old.d", "-MT", "old.o", "-c", "engine.cpp", "-o", "old.o"]
        new = native.with_output(args, "temporary.o")
        self.assertEqual(native.output_path(new), "temporary.o")
        self.assertEqual(new[:5], args[:5])
        self.assertIn("temporary.o.d", new)
        self.assertEqual(args[-1], "old.o")
        msvc = ["cl.exe", "/O2", "/GL", "/arch:AVX2", "/Foexisting.obj", "/Fdexisting.pdb", "engine.cpp"]
        new = native.with_output(msvc, "original.obj")
        self.assertEqual(new[:4], msvc[:4])
        self.assertIn("/Fooriginal.obj", new)
        self.assertIn("/Fdoriginal.obj.pdb", new)
        linked = native.with_output(["link.exe", "/out:existing.exe", "/pdb:existing.pdb", "/implib:existing.lib"], "original.exe")
        self.assertIn("/implib:original.exe.lib", linked)

    def test_native_compiler_scope_and_windows_object_identity(self):
        def description(name, version):
            return f'set(CMAKE_CXX_COMPILER_ID "{name}")\nset(CMAKE_CXX_COMPILER_VERSION "{version}")'
        for family, version, system, expected in [
            ("Clang", "22.1.8", "linux", True), ("GNU", "16.0", "linux", False),
            ("Clang", "21.1", "linux", False), ("AppleClang", "17.0", "darwin", True),
            ("Clang", "22.1", "darwin", False), ("MSVC", "19.44", "win32", True),
            ("MSVC", "19.29", "win32", False)]:
            with self.subTest(family=family, system=system):
                self.assertEqual(native.compiler_supported(description(family, version), system), expected)
        self.assertEqual(native.path_key(r"CMakeFiles\Bench.dir\Engine.obj", True),
                         native.path_key("cmakefiles/bench.dir/engine.obj", True))
        self.assertNotEqual(native.path_key("Engine.o", False), native.path_key("engine.o", False))
        # Assert each native path flavor explicitly, including an unchanged
        # parent containing "clang++". A Windows path need not use POSIX '/'.
        for path_class, cpp, c in (
            (PurePosixPath, "/opt/clang++ tools/clang++-22", "/opt/clang++ tools/clang-22"),
            (PureWindowsPath, r"C:\clang++ tools\clang++-22.exe", r"C:\clang++ tools\clang-22.exe"),
        ):
            with mock.patch.object(native, "Path", path_class), self.subTest(cpp=cpp):
                self.assertEqual(native.paired_c_compiler(cpp), c)
        self.assertEqual(native.paired_c_compiler("clang++"), "clang")
        self.assertEqual(native.paired_c_compiler("cl"), "cl")
        self.assertIsNone(native.paired_c_compiler("unknown-wrapper"))


class EvidenceTests(unittest.TestCase):
    def test_preparation_failure_is_journaled(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "evidence"
            args = type("Args", (), dict(output_dir=output, build_dir=Path(temporary) / "build", smoke=True, repetitions=1))()
            with mock.patch.object(native, "source_info", return_value={"status": "", "commit": "a" * 40}), \
                 mock.patch.object(native, "machine", side_effect=RuntimeError("fixture: metadata unavailable")):
                with self.assertRaisesRegex(RuntimeError, "metadata unavailable"):
                    native.collect(args)
            state = native.read(output / "run-status.json")
            self.assertEqual(state["state"], "failed")
            self.assertEqual(state["completed"], [])
            self.assertIsNone(state["performance_acceptance"])

    def test_owned_timeout_retains_logs(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            with self.assertRaises(subprocess.TimeoutExpired):
                native.run_logged([sys.executable, "-c", "import time; print('partial output', flush=True); time.sleep(60)"],
                                  directory, directory / "stdout", directory / "stderr", timeout=2)
            self.assertIn("partial output", (directory / "stdout").read_text())

    def test_audio_fixture_contains_real_silence_and_stereo_activity(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            native.stimulus(directory)
            with wave.open(str(directory / "playback-fixture.wav"), "rb") as wav:
                self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()),
                                 (2, 2, 48000, 14 * 48000))
                self.assertNotEqual(wav.readframes(48000), bytes(48000 * 4))
                wav.setpos(4 * 48000)
                self.assertEqual(wav.readframes(3 * 48000), bytes(3 * 48000 * 4))
            self.assertEqual(native.read(directory / "playback-fixture.json")["sha256"],
                             native.digest(directory / "playback-fixture.wav"))

    def test_analyzer_rejects_partial_or_trimmed_matrix(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            native.save(directory / "run-status.json", {"state": "interrupted", "completed": []})
            native.save(directory / "plan.json", native.make_plan(True, 1))
            with self.assertRaisesRegex(ValueError, "incomplete collection"):
                native.analyze(directory)
            native.save(directory / "run-status.json", {"state": "measurements_completed", "completed": [0]})
            native.save(directory / "plan.json", [native.make_plan(True, 1)[0]])
            with self.assertRaisesRegex(ValueError, "modified measurement matrix"):
                native.analyze(directory)

    def test_plan_keeps_original_serial_and_paired_order(self):
        plan = native.make_plan(False, 5)
        self.assertTrue(all(x["seconds"] >= 10 for x in plan))
        self.assertTrue(all(x["reader"] == "none" for x in plan if x["variant"] == "original"))
        self.assertEqual([x["variant"] for x in plan[:4]], ["original", "corrected", "corrected", "original"])
        points = {x["block"] for x in plan}
        self.assertTrue({32, 64, 128, 256, 16401}.issubset(points))
        self.assertTrue({44100, 48000, 96000, 192000}.issubset({x["rate"] for x in plan}))
        self.assertTrue({"active", "stalled"}.issubset({x["reader"] for x in plan}))

    def test_complete_record_and_read_normalization(self):
        req = request(kind="scope-read", window=1200)
        row = observation(req)
        native.validate_record(row, req)
        row["samples"] = req["block"] * req["iterations"]
        with self.assertRaisesRegex(ValueError, "normalization"):
            native.validate_record(row, req)
        for change in ({"scope_start": 0}, {"scope_end": 0}, {"audio_hash": "cbf29ce484222325"}, {"schema": 1}):
            row = dict(observation(req), **change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                native.validate_record(row, req)

    def test_negative_record_controls(self):
        req = request()
        changes = [{"block_ns": []}, {"block_ns": [0] * 19}, {"cpu": ""}, {"rep": 2},
                   {"samples": 1}, {"rss_available": None}, {"audio_hash": "bad"},
                   {"audio_hash": "0000000000000001"}, {"clock_min_ns": 3},
                   {"latency_samples": -1}, {"fp_denormals_disabled": False}, {"fp_round_to_nearest": None}]
        for change in changes:
            row = observation(req)
            row.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                native.validate_record(row, req)

    def test_reader_controls_are_nonvacuous(self):
        req = request(reader="stalled", paced=True, seconds=1.1)
        row = observation(req)
        native.validate_record(row, req)
        for change in ({"reader_accepted": 0}, {"stall_frames": 0}):
            broken = dict(row, **change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                native.validate_record(broken, req)
        unsafe = request(variant="original", reader="active", paced=True, seconds=1.1)
        with self.assertRaisesRegex(ValueError, "unsafe"):
            native.validate_record(observation(unsafe), unsafe)

    def test_summary_refuses_missing_rep_duplicate_or_changed_audio_latency(self):
        rows = [observation(request(variant=variant, rep=rep)) for rep in range(5) for variant in ("original", "corrected")]
        result = native.summarize(rows, 5, False)
        self.assertIsNone(result["performance_acceptance"])
        self.assertIsNone(result["manual_acceptance"])
        self.assertEqual(len(result["comparisons"]), 1)
        for mutation in ("missing", "duplicate", "audio", "latency"):
            broken = copy.deepcopy(rows)
            if mutation == "missing": broken.pop()
            if mutation == "duplicate": broken.append(broken[0])
            if mutation == "audio": broken[1]["audio_hash"] = "0000000000000099"
            if mutation == "latency": broken[1]["latency_samples"] += 1
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                native.summarize(broken, 5, False)
        with self.assertRaisesRegex(ValueError, "five"):
            native.summarize(rows[:2], 1, False)

    def test_archive_retains_partial_status_and_hashes_without_binaries(self):
        with tempfile.TemporaryDirectory(prefix="native-validation-") as temporary:
            root = Path(temporary)
            # Parent name intentionally matches the excluded overlay basename.
            evidence = root / "original-overlay" / "evidence"
            (evidence / "original-overlay").mkdir(parents=True)
            native.save(evidence / "run-status.json", {"state": "interrupted", "performance_acceptance": None})
            (evidence / "partial.stderr.log").write_text("partial diagnostic\n")
            (evidence / "original-overlay/probe.exe").write_bytes(b"fixture, not an executable")
            archive = root / "packet.zip"
            native.pack(evidence, archive)
            with zipfile.ZipFile(archive) as zipped:
                self.assertEqual(set(zipped.namelist()), {"run-status.json", "partial.stderr.log", "SHA256.json"})
                self.assertEqual(json.loads(zipped.read("run-status.json"))["state"], "interrupted")
                manifest = json.loads(zipped.read("SHA256.json"))
                self.assertEqual(manifest["partial.stderr.log"], native.digest(evidence / "partial.stderr.log"))
            with self.assertRaises(ValueError):
                native.pack(evidence, evidence / "recursive.zip")
            with self.assertRaises(FileExistsError):
                native.pack(evidence, archive)


if __name__ == "__main__":
    unittest.main()
