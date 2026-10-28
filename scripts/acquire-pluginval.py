#!/usr/bin/env python3
"""Prepare the content-locked release tool; never execute it. Standard library only."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import sys
import tempfile
import urllib.request
import zipfile


class SetupError(Exception):
    pass


PLATFORMS = {
    "Linux": ("pluginval_Linux.zip", "pluginval"),
    "macOS": ("pluginval_macOS.zip", "pluginval.app/Contents/MacOS/pluginval"),
    "Windows": ("pluginval_Windows.zip", "pluginval.exe"),
}
LOCK = Path(__file__).with_name("pluginval-release.json")


def require(condition, message):
    if not condition:
        raise SetupError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate lock key: " + key)
        result[key] = value
    return result


def load_lock(path=LOCK):
    data = json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    require(isinstance(data, dict) and set(data) == {
        "schema", "version", "release_url", "tag_object", "source_commit", "assets"
    }, "malformed release lock fields")
    require(type(data["schema"]) is int and data["schema"] == 1, "unsupported lock schema")
    require(isinstance(data["version"], str) and
            re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", data["version"]), "invalid locked version")
    require(data["release_url"] == "https://github.com/Tracktion/pluginval/releases/tag/" +
            data["version"], "lock must identify the official release")
    for key in ("tag_object", "source_commit"):
        require(isinstance(data[key], str) and re.fullmatch(r"[0-9a-f]{40}", data[key]),
                "invalid lock " + key)
    require(isinstance(data["assets"], dict) and set(data["assets"]) == set(PLATFORMS),
            "lock must contain exactly Linux, macOS and Windows assets")
    for platform, (name, executable) in PLATFORMS.items():
        asset = data["assets"][platform]
        require(isinstance(asset, dict) and set(asset) == {
            "id", "url", "bytes", "sha256", "executable", "executable_sha256"
        }, "malformed asset lock: " + platform)
        require(asset["url"] == "https://github.com/Tracktion/pluginval/releases/download/" +
                data["version"] + "/" + name, "wrong release/platform URL: " + platform)
        require(asset["executable"] == executable, "wrong executable member: " + platform)
        for key in ("id", "bytes"):
            require(type(asset[key]) is int and asset[key] > 0, "invalid asset " + key)
        require(asset["bytes"] <= 64 * 1024 * 1024, "archive exceeds download limit")
        for key in ("sha256", "executable_sha256"):
            require(isinstance(asset[key], str) and re.fullmatch(r"[0-9a-f]{64}", asset[key]),
                    "invalid asset " + key)
    return data


def metadata(path):
    info = path.lstat()
    require(not stat.S_ISLNK(info.st_mode) and not getattr(info, "st_file_attributes", 0) & 0x400,
            "linked/reparse cache path: " + str(path))
    return info


def digest(path):
    require(stat.S_ISREG(metadata(path).st_mode), "expected regular file: " + str(path))
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def verify_archive(path, asset):
    require(digest(path) == asset["sha256"], "archive SHA-256 mismatch: " + str(path))
    require(path.stat().st_size == asset["bytes"], "archive byte count mismatch: " + str(path))


def safe_name(name):
    # Reject alternate Windows spellings, ADS, UNC/drives, DOS devices and aliases
    # even on POSIX. Do not normalise an unsafe entry into an acceptable one.
    require(isinstance(name, str) and name and "\\" not in name, "unsafe ZIP path: " + repr(name))
    parts = name.rstrip("/").split("/")
    require(name == "/".join(parts) + ("/" if name.endswith("/") else ""),
            "noncanonical ZIP path: " + repr(name))
    for part in parts:
        require(part not in ("", ".", "..") and not part.endswith((".", " ")) and
                not re.search(r'[\x00-\x1f\x7f<>:"|?*]', part) and part.isascii() and
                not re.fullmatch(r"(?i:CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\..*)?", part),
                "unsafe ZIP path: " + repr(name))
    return "/".join(parts)


def inventory(archive, asset, platform):
    files, directories, names = {}, set(), set()
    entries = archive.infolist()
    require(0 < len(entries) <= 4096 and sum(i.file_size for i in entries) <= 128 * 1024 * 1024,
            "ZIP inventory exceeds tool limits")
    for entry in entries:
        require(entry.orig_filename == entry.filename, "NUL in ZIP name")
        name = safe_name(entry.filename)
        require(name.casefold() not in names, "duplicate/case-colliding ZIP entry: " + name)
        names.add(name.casefold())
        kind = stat.S_IFMT(entry.external_attr >> 16)
        is_dir = entry.is_dir()
        require(kind in ((0, stat.S_IFDIR) if is_dir else (0, stat.S_IFREG)),
                "unsupported ZIP entry type: " + name)
        require(not entry.flag_bits & 1 and entry.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED),
                "encrypted or unsupported ZIP entry: " + name)
        require((platform == "macOS" and (name == "pluginval.app" or name.startswith("pluginval.app/"))) or
                (platform != "macOS" and name == asset["executable"] and not is_dir),
                "unexpected platform ZIP member: " + name)
        if is_dir:
            require(entry.file_size == 0, "nonempty ZIP directory: " + name)
            directories.add(name)
        else:
            # Reading every member also checks CRC/truncation BEFORE extraction.
            files[name] = (hashlib.sha256(archive.read(entry)).hexdigest(), entry.file_size)
        directories.update(str(p) for p in PurePosixPath(name).parents if str(p) != ".")
    require(not set(files) & directories, "ZIP file/directory collision")
    all_names = list(files) + list(directories)
    require(len({n.casefold() for n in all_names}) == len(all_names), "ZIP parent case collision")
    require(asset["executable"] in files and files[asset["executable"]][0] == asset["executable_sha256"],
            "missing or wrong-platform executable in ZIP")
    if platform == "macOS":
        require("pluginval.app/Contents/Info.plist" in files and
                "pluginval.app/Contents/PkgInfo" in files and
                any(n.startswith("pluginval.app/Contents/Resources/") for n in files),
                "incomplete macOS bundle in ZIP")
    return files, directories


def verify_tree(tree, manifest, executable):
    files, directories = manifest
    require(stat.S_ISDIR(metadata(tree).st_mode), "missing tool tree")
    seen_files, seen_dirs = set(), set()
    for parent, dirs, leaves in os.walk(tree, followlinks=False):
        for name in dirs + leaves:
            path = Path(parent) / name
            relative = path.relative_to(tree).as_posix()
            mode = metadata(path).st_mode
            if stat.S_ISDIR(mode):
                seen_dirs.add(relative)
                require(relative in directories, "unexpected cached directory: " + relative)
            else:
                require(stat.S_ISREG(mode) and relative in files, "unexpected cached file/type: " + relative)
                require((digest(path), path.stat().st_size) == files[relative],
                        "cached content mismatch: " + relative)
                seen_files.add(relative)
    require(seen_files == set(files) and seen_dirs == directories, "incomplete cached tool tree")
    if os.name != "nt":
        require(os.access(tree / executable, os.X_OK), "cached tool is not executable")


class OfficialRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        check_download_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def check_download_url(url):
    from urllib.parse import urlsplit
    parsed = urlsplit(url)
    require(parsed.scheme == "https" and parsed.hostname in (
        "github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com"
    ) and parsed.username is None and parsed.password is None and parsed.port in (None, 443),
        "refusing non-official HTTPS download/redirect")


def download(asset, destination):
    check_download_url(asset["url"])
    opener = urllib.request.build_opener(OfficialRedirects())
    with opener.open(asset["url"], timeout=60) as response, destination.open("xb") as output:
        check_download_url(response.geturl())
        require(response.status == 200, "download did not return HTTP 200")
        remaining = asset["bytes"] + 1
        while remaining:
            block = response.read(min(1024 * 1024, remaining))
            if not block:
                break
            output.write(block)
            remaining -= len(block)
        require(remaining > 0, "download exceeds locked byte count")


def integrity_canary():
    # This runs in EVERY acquisition job, before using the protected path. A
    # seeded wrong digest must fail the SAME assertion as a real bad archive.
    with tempfile.TemporaryDirectory(prefix="pluginval-canary-") as tmp:
        path = Path(tmp) / "bad.zip"
        path.write_bytes(b"broken integrity canary\n")
        try:
            verify_archive(path, {"sha256": "0" * 64, "bytes": path.stat().st_size})
        except SetupError:
            return
        raise SetupError("integrity canary was accepted; archive verifier is not live")


def prepare(tools, lock, platform, fetch=download):
    require(platform in PLATFORMS, "unsupported host: " + platform)
    asset = lock["assets"][platform]
    base = Path(tools).absolute()
    # Refuse linked cache roots rather than writing through them.
    for path in (base, base / "pluginval-releases"):
        if path.exists() or path.is_symlink():
            require(stat.S_ISDIR(metadata(path).st_mode), "invalid cache root: " + str(path))
        path.mkdir(exist_ok=True)
    base /= "pluginval-releases"
    target = base / (lock["version"] + "-" + platform + "-" + asset["sha256"])
    if target.exists() or target.is_symlink():
        try:
            require(stat.S_ISDIR(metadata(target).st_mode), "invalid cache directory")
            verify_archive(target / "archive.zip", asset)
            with zipfile.ZipFile(target / "archive.zip") as archive:
                verify_tree(target / "tree", inventory(archive, asset, platform), asset["executable"])
        except (SetupError, OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
            raise SetupError("invalid cached tool at " + str(target) + ": " + str(error)) from error
        return target / "tree" / asset["executable"]
    stage = Path(tempfile.mkdtemp(prefix=".stage-", dir=base))
    try:
        fetch(asset, stage / "archive.zip")
        verify_archive(stage / "archive.zip", asset)
        with zipfile.ZipFile(stage / "archive.zip") as archive:
            manifest = inventory(archive, asset, platform)
            tree = stage / "tree"
            tree.mkdir()
            for name in sorted(manifest[1], key=lambda n: (n.count("/"), n)):
                (tree / name).mkdir()
            for name in manifest[0]:
                with archive.open(name) as source, (tree / name).open("xb") as dest:
                    shutil.copyfileobj(source, dest)
                (tree / name).chmod(0o755 if name == asset["executable"] else 0o644)
            verify_tree(tree, manifest, asset["executable"])
        # Nothing is usable at its final path until every preparation check passed.
        stage.rename(target)
        return target / "tree" / asset["executable"]
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", type=Path, required=True)
    args = parser.parse_args()
    try:
        platform = {"linux": "Linux", "darwin": "macOS", "win32": "Windows"}.get(sys.platform, sys.platform)
        integrity_canary()
        print(prepare(args.tools, load_lock(), platform))
        return 0
    except (SetupError, OSError, ValueError, KeyError, zipfile.BadZipFile, RuntimeError) as error:
        print("pluginval setup failure: " + str(error) +
              "; remove only the named content cache if damaged, then retry", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
