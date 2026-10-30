#!/usr/bin/env python3
"""Drive the actual shell preflight through its icon gate, without a product build."""

import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent


class IconPreflightTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in ("scripts/preflight.sh", "scripts/check-icons.py", "scripts/check-product-identity.py",
                     "scripts/check-installer-icon.py", "scripts/check-linux-package.py", "packaging/linux/install.sh", "packaging/linux/uninstall.sh", "packaging/linux/INSTALL.txt", "resources/icons/windows/Anamorph.ico",
                     "resources/icons/windows/PROVENANCE.json", "packaging/windows/Anamorph.iss", "packaging/macos/build-pkg.sh", "CMakeLists.txt",
                     "resources/icons/PROVENANCE.json", "resources/icons/anamorph-32.png",
                     "resources/icons/anamorph-1024.png"):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        # Stop at the first unrelated gate. This also proves it is still reached
        # after the icon gate. The real icon checker and shell entry point run.
        (self.root / "scripts/check-docs.py").write_text("print('NEXT_GATE_REACHED')\nraise SystemExit(73)\n")

    def run_preflight(self):
        return subprocess.run(["bash", "scripts/preflight.sh"], cwd=self.root,
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def assert_rejected(self, expected):
        result = self.run_preflight()
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn(expected, result.stdout)
        self.assertNotIn("NEXT_GATE_REACHED", result.stdout)

    def test_absent_build_reports_skip_then_reaches_existing_gates(self):
        result = self.run_preflight()
        self.assertEqual(result.returncode, 73, result.stdout)
        self.assertIn("matching-platform native good/bad fixtures", result.stdout)
        self.assertIn("SKIPPED native", result.stdout)
        self.assertIn("cmake --build", result.stdout)
        self.assertIn("--platform", result.stdout)
        self.assertIn("NEXT_GATE_REACHED", result.stdout)

    def test_corrupt_png_is_rejected_without_a_build(self):
        path = self.root / "resources/icons/anamorph-32.png"
        path.write_bytes(path.read_bytes() + b"corruption")
        self.assert_rejected("source digest mismatch")

    def test_changing_provenance_does_not_approve_modified_png(self):
        path = self.root / "resources/icons/anamorph-32.png"
        data = path.read_bytes() + b"corruption"
        path.write_bytes(data)
        provenance_path = self.root / "resources/icons/PROVENANCE.json"
        provenance = json.loads(provenance_path.read_text())
        provenance["assets"][1]["sha256"] = hashlib.sha256(data).hexdigest()
        provenance_path.write_text(json.dumps(provenance))
        self.assert_rejected("source digest mismatch")

    def test_wrong_cmake_path_is_rejected_without_a_build(self):
        path = self.root / "CMakeLists.txt"
        path.write_text(path.read_text().replace("resources/icons/anamorph-32.png", "resources/icons/wrong.png"))
        self.assert_rejected("wrong CMake ICON_SMALL input")

    def test_wrong_source_commit_is_rejected_without_a_build(self):
        path = self.root / "resources/icons/PROVENANCE.json"
        provenance = json.loads(path.read_text())
        provenance["source_commit"] = "b" * 40
        path.write_text(json.dumps(provenance))
        self.assert_rejected("unapproved source commit")

    def test_obsolete_linux_desktop_payload_is_rejected_without_a_build(self):
        path = self.root / "resources/icons/linux"
        path.mkdir(parents=True)
        self.assert_rejected("obsolete Linux desktop assets remain")

    def release_notes_fixture(self, prose):
        for name in ("scripts/check-release-notes.py", "scripts/changelog-section.awk"):
            shutil.copyfile(ROOT / name, self.root / name)
        # Keep the real shell aggregate and notes checker. Unrelated structure/source
        # checkers are sentinels so this test needs no git history, build or compiler.
        (self.root / "scripts/check-docs.py").write_text("print('STRUCTURE_GATE_PASSED')\n")
        (self.root / "scripts/check-portability.py").write_text("print('NEXT_SOURCE_GATE')\nraise SystemExit(73)\n")
        version = re.search(r'project\(Anamorph VERSION (\S+)',
                            (self.root / 'CMakeLists.txt').read_text())[1]
        (self.root / 'CHANGELOG.md').write_text(
            f'## [{version}] — 2026-10-30\n\n{prose}\n\n### Fixed\n- Session recall. Evidence: PR #168. [Verified]\n')

    def test_current_release_notes_fail_before_the_next_source_gate(self):
        self.release_notes_fixture('KI-029 remains open')
        result = self.run_preflight()
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn('STRUCTURE_GATE_PASSED', result.stdout)
        self.assertIn('Release-note content self-test:', result.stdout)
        self.assertIn('internal readiness prose in customer release notes', result.stdout)
        self.assertNotIn('NEXT_SOURCE_GATE', result.stdout)

    def test_customer_notes_pass_after_same_run_liveness(self):
        self.release_notes_fixture('Windows installers are unsigned.')
        result = self.run_preflight()
        self.assertEqual(result.returncode, 73, result.stdout)
        self.assertIn('Release-note content self-test:', result.stdout)
        self.assertIn('NEXT_SOURCE_GATE', result.stdout)
        self.assertLess(result.stdout.index('Release-note content self-test:'), result.stdout.index('NEXT_SOURCE_GATE'))


if __name__ == "__main__":
    unittest.main()
