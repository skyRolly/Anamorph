#!/usr/bin/env python3
"""Exercise the real top-level CMake cache handling without downloading JUCE.

A local FetchContent stand-in records the selected pin and deliberately stops
configuration at the dependency boundary. Reaching that marker is not a plugin
build; failing before it proves stale caches cannot reach FetchContent.
"""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
OLD_VERSION = "9.0.4"
OLD_TAG = "3fb5cbc0340758596dac39f7c8de47d562b4896e"
VERSION = "9.0.5"
TAG = "d60b7a78030661574a615b586d9325ba7a7fa7dd"
MARKER = "ANAMORPH_JUCE_CACHE_PROBE_REACHED"


class JuceCacheTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="anamorph-juce-cache-")
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.build = self.base / "build with spaces"
        self.juce = self.base / "local JUCE"
        self.juce.mkdir()
        (self.juce / "CMakeLists.txt").write_text(
            'file(WRITE "${CMAKE_BINARY_DIR}/selected-pin.txt" '
            '"${ANAMORPH_JUCE_VERSION}\\n${ANAMORPH_JUCE_TAG}\\n")\n'
            f'message(FATAL_ERROR "{MARKER}")\n',
            encoding="utf-8",
        )

    def seed_cache(self, version, tag):
        self.build.mkdir()
        (self.build / "CMakeCache.txt").write_text(
            f"ANAMORPH_JUCE_VERSION:STRING={version}\n"
            f"ANAMORPH_JUCE_TAG:STRING={tag}\n",
            encoding="utf-8",
        )

    def configure(self, *args):
        return subprocess.run(
            ["cmake", "-S", str(ROOT), "-B", str(self.build), "-G", "Ninja",
             f"-DFETCHCONTENT_SOURCE_DIR_JUCE={self.juce}", *args],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            timeout=60, check=False,
        )

    def assert_selected(self, result, version=VERSION, tag=TAG):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(MARKER, result.stdout)
        self.assertEqual(
            (self.build / "selected-pin.txt").read_text().splitlines(),
            [version, tag],
        )

    def assert_stale(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertNotIn(MARKER, result.stdout)
        self.assertIn("stale JUCE cache", result.stdout)
        self.assertIn("this tree pins JUCE 9.0.5", result.stdout)
        self.assertIn(
            "-UANAMORPH_JUCE_TAG -UANAMORPH_JUCE_VERSION",
            " ".join(result.stdout.split()),
        )
        self.assertFalse((self.build / "selected-pin.txt").exists())

    def test_fresh_and_repeated_configuration_select_current_pin(self):
        self.assert_selected(self.configure())
        self.assert_selected(self.configure())

    def test_existing_904_cache_fails_until_both_values_are_reset(self):
        self.seed_cache(OLD_VERSION, OLD_TAG)
        self.assert_stale(self.configure())
        self.assert_stale(self.configure())
        cache = (self.build / "CMakeCache.txt").read_text()
        self.assertIn(f"ANAMORPH_JUCE_TAG:STRING={OLD_TAG}", cache)
        self.assertIn(f"ANAMORPH_JUCE_VERSION:STRING={OLD_VERSION}", cache)
        self.assert_selected(self.configure(
            "-UANAMORPH_JUCE_TAG", "-UANAMORPH_JUCE_VERSION"))

    def test_resetting_only_tag_still_refuses_stale_version(self):
        self.seed_cache(OLD_VERSION, OLD_TAG)
        self.assert_stale(self.configure("-UANAMORPH_JUCE_TAG"))

    def test_resetting_only_version_still_refuses_stale_tag(self):
        self.seed_cache(OLD_VERSION, OLD_TAG)
        self.assert_stale(self.configure("-UANAMORPH_JUCE_VERSION"))

    def test_custom_pin_and_version_survive_reconfiguration(self):
        args = ("-DANAMORPH_JUCE_TAG=custom-pin",
                "-DANAMORPH_JUCE_VERSION=custom-version")
        self.assert_selected(self.configure(*args), "custom-version", "custom-pin")
        self.assert_selected(self.configure(), "custom-version", "custom-pin")

    def test_local_checkout_allows_intentional_904_comparison(self):
        self.seed_cache(OLD_VERSION, OLD_TAG)
        self.assert_selected(self.configure(f"-DANAMORPH_JUCE_PATH={self.juce}"),
                             OLD_VERSION, OLD_TAG)

    def test_legacy_902_fields_are_also_refused(self):
        legacy_tag = "72782788ce18c2d4d760b28e0921d6ffc6431102"
        for i, (version, tag) in enumerate([
            ("9.0.2", legacy_tag), (VERSION, legacy_tag), ("9.0.2", TAG),
        ]):
            with self.subTest(version=version, tag=tag):
                self.build = self.base / f"legacy-{i}"
                self.seed_cache(version, tag)
                self.assert_stale(self.configure())

    def test_mixed_904_and_current_fields_are_refused(self):
        for i, pair in enumerate(((OLD_VERSION, TAG), (VERSION, OLD_TAG))):
            with self.subTest(pair=pair):
                self.build = self.base / f"mixed-904-{i}"
                self.seed_cache(*pair)
                self.assert_stale(self.configure())

    def test_legacy_903_fields_are_also_refused(self):
        legacy_tag = "be29c81492b6151c8ea8d14c840e1311963b3a83"
        for i, pair in enumerate((("9.0.3", legacy_tag), (VERSION, legacy_tag), ("9.0.3", TAG))):
            with self.subTest(pair=pair):
                self.build = self.base / f"legacy-903-{i}"
                self.seed_cache(*pair)
                self.assert_stale(self.configure())


    def source_checkout(self):
        (self.juce / "CMakeLists.txt").write_text("# Minimal git checkout for source provenance\n")
        for args in (("init", "--quiet"), ("add", "CMakeLists.txt"),
                     ("-c", "user.name=Cache Test", "-c", "user.email=cache-test@example.invalid",
                      "commit", "--quiet", "-m", "fixture")):
            subprocess.run(["git", "-C", str(self.juce), *args], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return subprocess.check_output(
            ["git", "-C", str(self.juce), "rev-parse", "HEAD"], text=True).strip()

    def test_fetched_source_must_match_immutable_pin(self):
        actual = self.source_checkout()
        result = self.configure()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("JUCE source HEAD " + actual, " ".join(result.stdout.split()))
        self.assertIn("does not match immutable pin " + TAG, " ".join(result.stdout.split()))
        self.assertNotIn("verified JUCE source HEAD", result.stdout)

    def test_matching_custom_immutable_source_is_verified(self):
        actual = self.source_checkout()
        result = self.configure("-DANAMORPH_JUCE_TAG=" + actual,
                                "-DANAMORPH_JUCE_VERSION=custom-version")
        # The minimal checkout has no JUCE plugin CMake functions; reaching their
        # later error proves the provenance gate admitted this real custom SHA.
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("verified JUCE source HEAD " + actual, result.stdout)
        self.assertNotIn("does not match immutable pin", result.stdout)


if __name__ == "__main__":
    unittest.main()
