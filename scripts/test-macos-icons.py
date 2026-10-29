#!/usr/bin/env python3
"""Exercise iconutil and the native application-icon gate with real good/bad bundles."""

import argparse
import binascii
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("icons", ROOT / "scripts/check-icons.py")
icons = importlib.util.module_from_spec(spec)
spec.loader.exec_module(icons)
EVIDENCE = ROOT / "build/icon-render-regression"


@unittest.skipUnless(sys.platform == "darwin", "requires native macOS iconutil and AppKit")
class MacIconTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        cls.directory = tempfile.TemporaryDirectory()
        cls.root = Path(cls.directory.name)
        cls.fixed = cls.root / "AppIcon.icns"
        subprocess.run(["cmake", "-DBIG=" + str(ROOT / "resources/icons/anamorph-1024.png"),
                        "-DSMALL=" + str(ROOT / "resources/icons/anamorph-32.png"),
                        "-DOUTPUT=" + str(cls.fixed), "-P", str(ROOT / "cmake/GenerateMacIcon.cmake")],
                       check=True, capture_output=True, text=True)
        cls.old_pngs = {"ic10": (ROOT / "resources/icons/anamorph-1024.png").read_bytes(),
                       "icp5": (ROOT / "resources/icons/anamorph-32.png").read_bytes()}
        body = b"".join(k.encode() + struct.pack(">I", len(v) + 8) + v for k, v in cls.old_pngs.items())
        cls.old = b"icns" + struct.pack(">I", len(body) + 8) + body
        (EVIDENCE / "old-layout.icns").write_bytes(cls.old)
        (EVIDENCE / "corrected.icns").write_bytes(cls.fixed.read_bytes())

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_actual_iconutil_output_and_strict_parser(self):
        data = self.fixed.read_bytes()
        entries = icons.icns_images(data)
        icons.icns_coverage(entries)
        self.assertFalse({"icp4", "icp5", "icp6"}.intersection(entries))
        report = {k: {"pixels": icons.icns_entry_size(k), "payload_bytes": len(v)} for k, v in entries.items()}
        (EVIDENCE / "entries.json").write_text(json.dumps(report, indent=2) + "\n")
        with self.assertRaises(ValueError):
            icons.icns_images(data[:-1])
        # Duplicate a genuine native entry with an otherwise correct outer size.
        length = struct.unpack_from(">I", data, 12)[0]
        duplicated = data + data[8:8 + length]
        duplicated = duplicated[:4] + struct.pack(">I", len(duplicated)) + duplicated[8:]
        with self.assertRaisesRegex(ValueError, "duplicate ICNS"):
            icons.icns_images(duplicated)

    def test_old_png_decodes_but_its_container_is_rejected(self):
        self.assertEqual(icons.png_size(self.old_pngs["icp5"]), 32)
        self.assertEqual(icons.png_size(self.old_pngs["ic10"]), 1024)
        with self.assertRaisesRegex(ValueError, "unsafe small ICNS slot: icp5"):
            icons.icns_images(self.old)

    def test_native_bundle_render_positive_and_negative_controls(self):
        results = {}
        for name, data in (("corrected", self.fixed.read_bytes()), ("old", self.old)):
            directory = self.root / name
            bundle = icons.macos_fixture(directory, data)
            rows = icons.render_macos_bundle(bundle, EVIDENCE / name)
            try:
                icons.validate_macos_render(rows)
            except ValueError as error:
                results[name] = {"accepted": False, "diagnostic": str(error)}
                if name == "corrected":
                    raise
            else:
                results[name] = {"accepted": True}
                if name == "old":
                    print("::warning::this OS did not reproduce the old AppKit rendering defect; structural regression still rejects it")
        self.assertTrue(results["corrected"]["accepted"])
        (EVIDENCE / "controls.json").write_text(json.dumps(results, indent=2) + "\n")

    def test_wrong_artwork_cannot_become_its_own_reference(self):
        # A complete, renderable ICNS with different artwork must still fail.
        # This catches accidentally deriving the golden icon from the candidate.
        directory = self.root / "wrong-artwork"
        directory.mkdir()

        def red_png(size):
            def chunk(kind, payload):
                return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
                    ">I", binascii.crc32(kind + payload) & 0xffffffff)
            margin = size // 8
            empty = b"\0" + bytes(size * 4)
            row = b"\0" + bytes(margin * 4) + b"\xff\0\0\xff" * (size - 2 * margin) + bytes(margin * 4)
            raw = empty * margin + row * (size - 2 * margin) + empty * margin
            return icons.PNG + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) \
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")

        for size in (32, 1024):
            (directory / f"red-{size}.png").write_bytes(red_png(size))
        icon = directory / "Wrong.icns"
        subprocess.run(["cmake", "-DBIG=" + str(directory / "red-1024.png"),
                        "-DSMALL=" + str(directory / "red-32.png"), "-DOUTPUT=" + str(icon),
                        "-P", str(ROOT / "cmake/GenerateMacIcon.cmake")],
                       check=True, capture_output=True, text=True)
        icons.icns_coverage(icons.icns_images(icon.read_bytes()))
        bundle = icons.macos_fixture(directory / "bundle", icon.read_bytes())
        rows = icons.render_macos_bundle(bundle, EVIDENCE / "wrong-artwork")
        with self.assertRaisesRegex(ValueError, "native application icon differs"):
            icons.validate_macos_render(rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-dir", type=Path, default=EVIDENCE)
    args, remaining = parser.parse_known_args()
    EVIDENCE = args.evidence_dir.resolve()
    unittest.main(argv=[sys.argv[0]] + remaining)
