#!/usr/bin/env python3
"""Offline acquisition regressions. ZIP contents are data and are never executed."""

import copy
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock
import urllib.error
import zipfile


SCRIPTS = Path(__file__).resolve().parent
PRODUCT = "Anamorph"
spec = importlib.util.spec_from_file_location("acquisition", SCRIPTS / "acquire-pluginval.py")
pv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pv)
REQUIRE_PS = "--require-powershell" in sys.argv
if REQUIRE_PS:
    sys.argv.remove("--require-powershell")


def fixture(platform, extra=()):
    executable = pv.PLATFORMS[platform][1]
    members = [(executable, b"INERT executable fixture; never run\n")]
    if platform == "macOS":
        members += [("pluginval.app/Contents/Info.plist", b"inert metadata"),
                    ("pluginval.app/Contents/PkgInfo", b"APPL????"),
                    ("pluginval.app/Contents/Resources/Icon.icns", b"inert resource")]
    members += list(extra)
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w") as archive:
        for name, data in members:
            archive.writestr(name, data)
    blob = stream.getvalue()
    lock = copy.deepcopy(pv.load_lock())
    asset = lock["assets"][platform]
    asset.update(bytes=len(blob), sha256=hashlib.sha256(blob).hexdigest(),
                 executable_sha256=hashlib.sha256(members[0][1]).hexdigest())
    return lock, blob


class AcquisitionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pluginval test-")
        self.root = Path(self.temp.name)
        self.tools = self.root / ".tools"
        self.fetches = 0

    def tearDown(self):
        self.temp.cleanup()

    def fetch(self, blob):
        def write(asset, destination):
            self.fetches += 1
            destination.write_bytes(blob)
        return write

    def test_valid_prepare_and_reuse_all_platforms(self):
        for platform in pv.PLATFORMS:
            with self.subTest(platform=platform):
                lock, blob = fixture(platform)
                exe = pv.prepare(self.tools, lock, platform, self.fetch(blob))
                self.assertEqual(exe.read_bytes(), b"INERT executable fixture; never run\n")
                same = pv.prepare(self.tools, lock, platform, mock.Mock(side_effect=AssertionError("download on reuse")))
                self.assertEqual(exe, same)
                self.assertIn(lock["assets"][platform]["sha256"], str(exe))
        self.assertEqual(self.fetches, 3)

    def test_concurrent_first_acquisition_reverifies_winner(self):
        for platform in pv.PLATFORMS:
            with self.subTest(platform=platform):
                lock, blob = fixture(platform)
                barrier = threading.Barrier(2)
                rename = Path.rename

                def publish(stage, target):
                    # Both invocations must finish staging before either publishes.
                    barrier.wait(timeout=10)
                    return rename(stage, target)

                with mock.patch.object(Path, "rename", publish), \
                        mock.patch.object(pv, "verify_archive", wraps=pv.verify_archive) as archives, \
                        mock.patch.object(pv, "inventory", wraps=pv.inventory) as inventories, \
                        mock.patch.object(pv, "verify_tree", wraps=pv.verify_tree) as trees, \
                        ThreadPoolExecutor(max_workers=2) as pool:
                    futures = [pool.submit(pv.prepare, self.tools, lock, platform, self.fetch(blob))
                               for _ in range(2)]
                    paths = [future.result(timeout=20) for future in futures]
                self.assertEqual(paths[0], paths[1])
                target = paths[0].parents[len(Path(lock["assets"][platform]["executable"]).parts)]
                # The losing invocation must check the PUBLISHED archive/tree,
                # not return a path justified only by its own disposable stage.
                self.assertIn(target / "archive.zip", [call.args[0] for call in archives.call_args_list])
                self.assertIn(str(target / "archive.zip"), [call.args[0].filename for call in inventories.call_args_list])
                self.assertIn(target / "tree", [call.args[0] for call in trees.call_args_list])
                self.assertFalse(any(p.name.startswith(".stage-") for p in target.parent.iterdir()))
        self.assertEqual(self.fetches, 6)

    def test_concurrent_invalid_winner_is_not_replaced_or_reused(self):
        cases = (("Linux", "archive.zip", b"corrupt archive"),
                 ("Linux", "archive.zip", None),
                 ("Linux", "tree/pluginval", b"modified executable"),
                 ("macOS", "tree/pluginval.app/Contents/Info.plist", None),
                 ("macOS", "tree/pluginval.app/Contents/Resources/Icon.icns", b"modified resource"))
        for index, (platform, relative, replacement) in enumerate(cases):
            with self.subTest(platform=platform, relative=relative, replacement=replacement):
                lock, blob = fixture(platform)
                tools = self.root / ("race-" + str(index))
                published = []

                def publish(stage, target):
                    # An independent winner appears after the initial cache check.
                    shutil.copytree(stage, target)
                    victim = target / relative
                    if replacement is None:
                        victim.unlink()
                    else:
                        victim.write_bytes(replacement)
                    published.append(target)
                    raise FileExistsError("another invocation published this target")

                with mock.patch.object(Path, "rename", publish):
                    with self.assertRaisesRegex(pv.SetupError, "invalid cached tool at"):
                        pv.prepare(tools, lock, platform, self.fetch(blob))
                victim = published[0] / relative
                if replacement is None:
                    self.assertFalse(victim.exists())
                else:
                    self.assertEqual(victim.read_bytes(), replacement)
                self.assertEqual(list(published[0].parent.iterdir()), published)

    def test_publication_error_without_winner_still_fails(self):
        lock, blob = fixture("Linux")
        with mock.patch.object(Path, "rename", side_effect=PermissionError("publication denied")):
            with self.assertRaisesRegex(PermissionError, "publication denied"):
                pv.prepare(self.tools, lock, "Linux", self.fetch(blob))
        self.assertEqual(list((self.tools / "pluginval-releases").iterdir()), [])

    def test_bad_downloads_never_extract_or_publish(self):
        lock, blob = fixture("Linux")
        for data in (b"<html>404</html>", blob[:30], b"", blob + b"extra"):
            with self.subTest(size=len(data)), mock.patch.object(zipfile, "ZipFile", side_effect=AssertionError("extraction reached")):
                with self.assertRaises(pv.SetupError):
                    pv.prepare(self.tools, lock, "Linux", self.fetch(data))
            self.assertEqual(list((self.tools / "pluginval-releases").iterdir()), [])
        for failure in (FileNotFoundError("missing input"), urllib.error.HTTPError("https://github.com", 404, "missing", {}, None)):
            with self.assertRaises(OSError), mock.patch.object(zipfile, "ZipFile", side_effect=AssertionError("extraction reached")):
                pv.prepare(self.tools, lock, "Linux", mock.Mock(side_effect=failure))

    def test_missing_and_modified_cache_refused(self):
        for platform in pv.PLATFORMS:
            lock, blob = fixture(platform)
            exe = pv.prepare(self.tools, lock, platform, self.fetch(blob))
            original = exe.read_bytes()
            exe.write_bytes(b"modified executable")
            with self.assertRaisesRegex(pv.SetupError, "cached content mismatch"):
                pv.prepare(self.tools, lock, platform)
            exe.write_bytes(original)
            exe.unlink()
            with self.assertRaisesRegex(pv.SetupError, "incomplete cached"):
                pv.prepare(self.tools, lock, platform)

    def test_bundle_metadata_resources_extra_and_links_refused(self):
        lock, blob = fixture("macOS")
        exe = pv.prepare(self.tools, lock, "macOS", self.fetch(blob))
        tree = exe.parents[3]
        for relative in ("pluginval.app/Contents/Info.plist", "pluginval.app/Contents/Resources/Icon.icns"):
            path = tree / relative
            original = path.read_bytes()
            path.write_bytes(b"modified")
            with self.assertRaisesRegex(pv.SetupError, "cached content mismatch"):
                pv.prepare(self.tools, lock, "macOS")
            path.unlink()
            with self.assertRaisesRegex(pv.SetupError, "incomplete cached"):
                pv.prepare(self.tools, lock, "macOS")
            path.write_bytes(original)
        (tree / "unexpected").mkdir()
        with self.assertRaisesRegex(pv.SetupError, "unexpected cached"):
            pv.prepare(self.tools, lock, "macOS")
        (tree / "unexpected").rmdir()
        if os.name != "nt":
            resource = tree / "pluginval.app/Contents/Resources/Icon.icns"
            resource.unlink()
            resource.symlink_to(self.root / "outside")
            with self.assertRaisesRegex(pv.SetupError, "linked|file/type"):
                pv.prepare(self.tools, lock, "macOS")

    def test_cached_archive_is_reverified_before_tree(self):
        lock, blob = fixture("Linux")
        exe = pv.prepare(self.tools, lock, "Linux", self.fetch(blob))
        archive = exe.parents[1] / "archive.zip"
        archive.write_bytes(b"corrupt cached archive")
        with mock.patch.object(pv, "verify_tree", side_effect=AssertionError("tree used before archive")):
            with self.assertRaisesRegex(pv.SetupError, "archive SHA-256"):
                pv.prepare(self.tools, lock, "Linux")

    def test_legacy_version_and_platform_do_not_authorize_reuse(self):
        self.tools.mkdir()
        (self.tools / "pluginval").write_bytes(b"legacy Linux tool")
        (self.tools / "pluginval.exe").write_bytes(b"legacy Windows tool")
        lock, blob = fixture("Linux")
        first = pv.prepare(self.tools, lock, "Linux", self.fetch(blob))
        old = copy.deepcopy(lock)
        old["version"] = "v0.0.1"
        old["release_url"] = old["release_url"].replace(lock["version"], "v0.0.1")
        for asset in old["assets"].values():
            asset["url"] = asset["url"].replace(lock["version"], "v0.0.1")
        second = pv.prepare(self.tools, old, "Linux", self.fetch(blob))
        self.assertNotEqual(first, second)
        windows, _ = fixture("Windows")
        with self.assertRaises(pv.SetupError):
            pv.prepare(self.tools, windows, "Windows", self.fetch(blob))
        self.assertEqual((self.tools / "pluginval").read_bytes(), b"legacy Linux tool")
        self.assertEqual((self.tools / "pluginval.exe").read_bytes(), b"legacy Windows tool")
        with self.assertRaisesRegex(pv.SetupError, "unsupported host"):
            pv.prepare(self.tools, lock, "FreeBSD")

    def test_malformed_lock_refused(self):
        valid = pv.load_lock()
        mutations = [dict(valid, schema=True), dict(valid, version="latest"), dict(valid, assets={}),
                     dict(valid, source_commit="bad"), dict(valid, extra="unknown")]
        for key, value in (("url", valid["assets"]["Windows"]["url"]), ("sha256", "xyz"),
                           ("bytes", 0), ("id", "210999199"), ("executable", "pluginval.exe")):
            data = copy.deepcopy(valid)
            data["assets"]["Linux"][key] = value
            mutations.append(data)
        for data in mutations:
            path = self.root / "lock.json"
            path.write_text(json.dumps(data))
            with self.assertRaises(pv.SetupError):
                pv.load_lock(path)
        for text in ('{"schema":1,"schema":1}', '{', '[]'):
            path.write_text(text)
            with self.assertRaises((pv.SetupError, ValueError)):
                pv.load_lock(path)

    def test_unsafe_archives_contained_and_unpublished(self):
        bad_names = ("../escape", "../../escape", "/absolute", "C:/escape", "C:escape", "//host/share",
                     r"..\escape", r"\host\share", "pluginval.app/Contents/../escape",
                     "pluginval.app/Contents/evil:ads", "pluginval.app/Contents/CON.txt",
                     "pluginval.app/Contents/NUL", "pluginval.app/Contents/name.",
                     "pluginval.app/Contents/name ", "pluginval.app//double", "pluginval.app//",
                     "pluginval.app/Contents/INFO.plist", "pluginval.app/Contents/Resources/Icon.icns/child")
        for name in bad_names:
            lock, blob = fixture("macOS", [(name, b"bad")])
            with self.subTest(name=name), self.assertRaises(pv.SetupError):
                pv.prepare(self.tools, lock, "macOS", self.fetch(blob))
            self.assertEqual(list((self.tools / "pluginval-releases").iterdir()), [])
            self.assertFalse((self.root / "escape").exists())
        for kind in (stat.S_IFLNK, stat.S_IFIFO, stat.S_IFSOCK):
            entry = zipfile.ZipInfo("pluginval.app/Contents/link")
            entry.create_system = 3
            entry.external_attr = (kind | 0o777) << 16
            lock, blob = fixture("macOS", [(entry, b"../../escape")])
            with self.assertRaisesRegex(pv.SetupError, "entry type"):
                pv.prepare(self.tools, lock, "macOS", self.fetch(blob))
        lock, blob = fixture("macOS", [("pluginval.app/Contents/Resources/Icon.icns", b"duplicate")])
        with self.assertRaisesRegex(pv.SetupError, "duplicate"):
            pv.prepare(self.tools, lock, "macOS", self.fetch(blob))
        lock, blob = fixture("Linux")
        # A digest-valid non-ZIP is still a setup error, never a prepared tool.
        lock["assets"]["Linux"].update(bytes=3, sha256=hashlib.sha256(b"bad").hexdigest())
        with self.assertRaises(zipfile.BadZipFile):
            pv.prepare(self.tools, lock, "Linux", self.fetch(b"bad"))

    def test_integrity_canary_kills_disabled_verifier(self):
        pv.integrity_canary()
        with mock.patch.object(pv, "verify_archive", return_value=None):
            with self.assertRaisesRegex(pv.SetupError, "verifier is not live"):
                pv.integrity_canary()

    def test_redirect_and_download_failures(self):
        for url in ("http://github.com/bad", "https://example.com/bad", "file:///tmp/bad",
                    "https://github.com@evil.example/a", "https://github.com:444/a"):
            with self.assertRaises(pv.SetupError):
                pv.check_download_url(url)
            with self.assertRaises(pv.SetupError):
                pv.OfficialRedirects().redirect_request(None, None, 302, "redirect", {}, url)
        lock, blob = fixture("Linux")
        asset = lock["assets"]["Linux"]
        for data, status in ((blob, 200), (blob + b"oversize", 200), (b"missing", 404)):
            response = mock.MagicMock()
            response.__enter__.return_value = response
            response.geturl.return_value = asset["url"]
            response.status = status
            response.read.side_effect = io.BytesIO(data).read
            output = self.root / "response.zip"
            with mock.patch.object(pv.urllib.request, "build_opener") as opener:
                opener.return_value.open.return_value = response
                if status == 200 and data == blob:
                    pv.download(asset, output)
                    pv.verify_archive(output, asset)
                else:
                    with self.assertRaises(pv.SetupError):
                        pv.download(asset, output)
            output.unlink(missing_ok=True)
        # Chunked transport and malformed compressed data must retain the same
        # clear setup-error contract, rather than leaking a traceback.
        for failure in (pv.http.client.IncompleteRead(b"partial", 40), pv.zlib.error("broken stream")):
            with mock.patch.object(sys, "argv", ["acquire", "--tools", str(self.tools)]), \
                    mock.patch.object(pv, "prepare", side_effect=failure), \
                    mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
                self.assertEqual(pv.main(), 1)
                self.assertIn("pluginval setup failure", errors.getvalue())

    def wrapper(self, powershell):
        if powershell:
            runner = shutil.which("pwsh")
            if not runner:
                if REQUIRE_PS:
                    self.fail("PowerShell required but unavailable")
                self.skipTest("Warning: PowerShell integration not executed; pwsh unavailable")
        elif os.name == "nt":
            self.skipTest("Warning: POSIX wrapper integration runs on POSIX hosts")
        root = self.root / ("ps" if powershell else "sh")
        scripts = root / "scripts"
        scripts.mkdir(parents=True)
        # Real production CLI, with only data transport + lock location substituted.
        # No production environment variable accepts test hashes or alternate URLs.
        launcher = scripts / "launcher.py"
        launcher.write_text(
            "import importlib.util,sys,os\nfrom pathlib import Path\n"
            "s=importlib.util.spec_from_file_location('pv',os.environ['PV_SOURCE'])\n"
            "p=importlib.util.module_from_spec(s);s.loader.exec_module(p)\n"
            "load=p.load_lock;p.load_lock=lambda:load(Path(os.environ['PV_LOCK']))\n"
            "sys.platform=os.environ['PV_HOST']\n"
            "def fetch(asset,dest):\n"
            "    if os.environ['PV_DOWNLOAD']=='missing': raise FileNotFoundError('fixture input missing')\n"
            "    dest.write_bytes(Path(os.environ['PV_DOWNLOAD']).read_bytes())\n"
            "prepare=p.prepare;p.prepare=lambda tools,lock,platform:prepare(tools,lock,platform,fetch)\n"
            "sys.argv=[sys.argv[0]]+sys.argv[2:]\n"
            "sys.exit(p.main())\n", encoding="utf-8")
        marker = root / "invocations.txt"
        if powershell:
            source = (SCRIPTS / "run-pluginval.ps1").read_text()
            quote = lambda s: "'" + str(s).replace("'", "''") + "'"
            # Function shadows only Python transport, and the final validator call
            # is an inert counting boundary. Setup code is otherwise unchanged.
            source = source.replace('$root  =', 'function python { & ' + quote(sys.executable) +
                                    ' ' + quote(launcher) + ' @args }\n$root  =', 1)
            call = '$rc = Invoke-Pluginval -Exe $pv -PvArgs $pvArgs'
            self.assertEqual(source.count(call), 1)
            source = source.replace(call, '$rc = 0; Add-Content -LiteralPath ' + quote(marker) + " -Value invoked")
            path = scripts / "run-pluginval.ps1"
            path.write_text(source, encoding="utf-8")
            shutil.copyfile(SCRIPTS / "find-windows-vst3.ps1", scripts / "find-windows-vst3.ps1")
            command = [runner, "-NoProfile", "-File", str(path)]
            platform, host = "Windows", "win32"
            bundle = root / "build" / (PRODUCT + ".vst3") / "Contents/x86_64-win"
            bundle.mkdir(parents=True)
            (bundle / (PRODUCT + ".vst3")).write_bytes(b"inert module")
            env = dict(os.environ)
        else:
            import shlex
            bin_dir = root / "bin"
            bin_dir.mkdir()
            python = bin_dir / "python3"
            python.write_text("#!/bin/sh\nexec " + shlex.quote(sys.executable) + " " + shlex.quote(str(launcher)) + ' "$@"\n')
            python.chmod(0o755)
            source = (SCRIPTS / "run-pluginval.sh").read_text()
            call = 'run_one_pass "$FORMAT $MODE pass $pass/$PASSES"'
            self.assertEqual(source.count(call), 1)
            source = source.replace(call, "printf 'invoked\\n' >> " + shlex.quote(str(marker)))
            path = scripts / "run-pluginval.sh"
            path.write_text(source)
            command = ["bash", str(path)]
            platform, host = ("macOS", "darwin") if sys.platform == "darwin" else ("Linux", "linux")
            (root / "build" / (PRODUCT + ".vst3")).mkdir(parents=True)
            env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
        lock, blob = fixture(platform)
        lock_path = scripts / "fixture-lock.json"
        lock_path.write_text(json.dumps(lock))
        archive = scripts / "fixture.zip"
        env.update(PV_SOURCE=str(SCRIPTS / "acquire-pluginval.py"), PV_LOCK=str(lock_path), PV_HOST=host)
        for data in (b"wrong bytes", blob[:30], None, blob):
            env["PV_DOWNLOAD"] = str(archive) if data is not None else "missing"
            archive.write_bytes(data or b"")
            result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if data == blob:
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(marker.read_text().splitlines(), ["invoked"] * 3)
            else:
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("pluginval setup failure", result.stdout)
                self.assertFalse(marker.exists(), result.stdout)
        # Valid reuse reaches the boundary without touching an unavailable download.
        env["PV_DOWNLOAD"] = "missing"
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(len(marker.read_text().splitlines()), 6)
        # A present executable cannot bypass integrity, lock or host refusal.
        exe = pv.prepare(root / ".tools", lock, platform)
        original = exe.read_bytes()
        exe.write_bytes(b"modified after valid reuse")
        for fault in ("cached", "lock", "host"):
            if fault == "lock":
                exe.write_bytes(original)
                lock_path.write_text("{}")
            elif fault == "host":
                lock_path.write_text(json.dumps(lock))
                env["PV_HOST"] = "freebsd"
            result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("pluginval setup failure", result.stdout)
            self.assertEqual(len(marker.read_text().splitlines()), 6)

    def test_shell_wrapper_setup_propagation(self):
        self.wrapper(False)

    def test_powershell_wrapper_setup_propagation(self):
        self.wrapper(True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
