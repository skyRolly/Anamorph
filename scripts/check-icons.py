#!/usr/bin/env python3
"""Check the pinned icon inputs and real JUCE outputs; standard library only."""

import argparse
import binascii
import ctypes
import hashlib
import json
import plistlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


ROOT = Path(__file__).resolve().parent.parent
PNG = b"\x89PNG\r\n\x1a\n"
INPUTS = {"anamorph-1024.png": (1024, "ICON_BIG"), "anamorph-32.png": (32, "ICON_SMALL")}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png_size(data):
    """Validate the PNG chunk stream, RGBA layout, CRCs and compressed rows."""
    require(data.startswith(PNG), "invalid PNG signature")
    offset, chunks, packed = 8, [], bytearray()
    while offset < len(data):
        require(offset + 12 <= len(data), "truncated PNG chunk")
        length = struct.unpack_from(">I", data, offset)[0]
        end = offset + length + 12
        require(end <= len(data), "truncated PNG payload")
        kind, payload = data[offset + 4:offset + 8], data[offset + 8:end - 4]
        require(binascii.crc32(kind + payload) & 0xffffffff == struct.unpack_from(">I", data, end - 4)[0], "PNG CRC mismatch")
        chunks.append(kind)
        if kind == b"IDAT":
            packed.extend(payload)
        if kind == b"IHDR":
            require(length == 13 and len(chunks) == 1, "invalid PNG IHDR")
            width, height, depth, colour, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            require(0 < width <= 1024 and width == height, "PNG must be square, <=1024")
            require((depth, colour, compression, filtering, interlace) == (8, 6, 0, 0, 0), "expected non-interlaced 8-bit RGBA PNG")
        offset = end
        if kind == b"IEND":
            require(length == 0 and end == len(data), "invalid PNG ending")
            break
    require(chunks and chunks[0] == b"IHDR" and chunks[-1] == b"IEND" and packed, "incomplete PNG")
    raw = zlib.decompress(packed)
    stride = width * 4 + 1
    require(len(raw) == height * stride, "invalid PNG decoded row length")
    require(all(raw[row * stride] <= 4 for row in range(height)), "invalid PNG row filter")
    return width


def icon_images(data):
    require(len(data) >= 6, "truncated ICO")
    reserved, kind, count = struct.unpack_from("<HHH", data)
    require((reserved, kind) == (0, 1) and count > 0 and 6 + count * 16 <= len(data), "invalid ICO directory")
    images = {}
    for i in range(count):
        w, h, colours, reserved, planes, bits, length, offset = struct.unpack_from("<BBBBHHII", data, 6 + i * 16)
        w, h = w or 256, h or 256
        require(w == h and w not in images and reserved == 0 and planes == 1 and bits == 32, "invalid ICO image entry")
        require(offset >= 6 + count * 16 and offset + length <= len(data), "ICO image outside container")
        image = data[offset:offset + length]
        if image.startswith(PNG):
            require(png_size(image) == w, "ICO PNG dimension mismatch")
        else:
            require(len(image) >= 40, "truncated ICO DIB")
            header, dw, dh, planes, bits, compression = struct.unpack_from("<IiiHHI", image)
            require((header, dw, dh, planes, bits, compression) == (40, w, h * 2, 1, 32, 0), "invalid ICO DIB layout")
            require(len(image) == 40 + h * (w * 4 + ((w + 31) // 32) * 4), "invalid ICO DIB/mask length")
            alpha = image[43:40 + w * h * 4:4]
            require(min(alpha) == 0 and max(alpha) == 255, "ICO DIB lost transparent silhouette")
        images[w] = image
    return images


def icns_images(data):
    require(len(data) >= 8 and data[:4] == b"icns" and struct.unpack_from(">I", data, 4)[0] == len(data), "invalid ICNS header")
    offset, images = 8, {}
    sizes = {b"icp4": 16, b"icp5": 32, b"icp6": 64, b"ic07": 128, b"ic08": 256, b"ic09": 512, b"ic10": 1024}
    while offset < len(data):
        require(offset + 8 <= len(data), "truncated ICNS entry")
        kind, length = struct.unpack_from(">4sI", data, offset)
        require(length >= 8 and offset + length <= len(data), "invalid ICNS entry length")
        require(kind in sizes and kind.decode() not in images, "unexpected/duplicate JUCE ICNS entry")
        payload = data[offset + 8:offset + length]
        require(png_size(payload) == sizes[kind], "ICNS PNG dimension mismatch")
        images[kind.decode()] = payload
        offset += length
    require(images, "empty ICNS")
    return images


def check_sources(root):
    provenance = json.loads((root / "resources/icons/PROVENANCE.json").read_text())
    require(provenance["source_repository"] == "https://github.com/skyRolly/icon-vector-reconstruction", "wrong icon repository")
    require(provenance["selected_design"] == "B0", "unapproved icon design")
    require(len(provenance["source_commit"]) == 40 and all(c in "0123456789abcdef" for c in provenance["source_commit"]), "invalid source commit")
    require(len(provenance["assets"]) == 2 and {Path(item["destination"]).name for item in provenance["assets"]} == set(INPUTS), "expected exactly the two approved inputs")
    cmake = (root / "CMakeLists.txt").read_text()
    plugin = cmake.split("juce_add_plugin(Anamorph\n", 1)[1].split("\n)", 1)[0]
    for item in provenance["assets"]:
        name = Path(item["destination"]).name
        size, argument = INPUTS[name]
        require(item["destination"] == "resources/icons/" + name, "unexpected icon destination")
        data = (root / item["destination"]).read_bytes()
        require(sha(data) == item["sha256"], f"source digest mismatch: {name}")
        require(png_size(data) == size, f"wrong source dimensions: {name}")
        declarations = [line.split() for line in plugin.splitlines() if line.strip().startswith(argument + " ")]
        require(declarations == [[argument, '"${CMAKE_CURRENT_SOURCE_DIR}/' + item["destination"] + '"']], f"wrong CMake {argument} input")
    return provenance


def group_entries(data):
    require(len(data) >= 6, "truncated PE icon group")
    reserved, kind, count = struct.unpack_from("<HHH", data)
    require((reserved, kind) == (0, 1) and count > 0 and len(data) == 6 + count * 14, "invalid PE icon group")
    entries = []
    for i in range(count):
        w, h, colours, reserved, planes, bits, length, resource_id = struct.unpack_from("<BBBBHHIH", data, 6 + i * 14)
        require((w or 256) == (h or 256) and reserved == 0 and planes == 1 and bits == 32, "invalid PE group entry")
        entries.append((w or 256, length, resource_id))
    require(len({w for w, _, _ in entries}) == count, "duplicate PE icon size")
    return entries


def windows_resources(path, expected):
    """Use the real Windows loader resource API, never load/execute plug-in code."""
    require(sys.platform == "win32", "PE resource validation requires Windows")
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = ctypes.c_void_p
    kernel.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    kernel.FindResourceW.restype = ctypes.c_void_p
    kernel.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    kernel.LoadResource.restype = ctypes.c_void_p
    kernel.LockResource.argtypes = [ctypes.c_void_p]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
    module = kernel.LoadLibraryExW(str(path.resolve()), None, 0x22)  # DATAFILE | IMAGE_RESOURCE
    require(module, f"cannot open PE resources: {path}")
    groups, errors = [], []

    def resource(kind, name):
        found = kernel.FindResourceW(module, name, kind)
        require(found, f"missing PE resource type {kind}")
        size = kernel.SizeofResource(module, found)
        memory = kernel.LockResource(kernel.LoadResource(module, found))
        require(memory and size, "unreadable PE resource")
        return ctypes.string_at(memory, size)

    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ssize_t)

    @callback_type
    def collect(module_handle, kind, name, context):
        try:
            entries = group_entries(resource(14, name))
            require({w for w, _, _ in entries} == set(expected), "embedded PE frame sizes differ from generated ICO")
            for width, size, resource_id in entries:
                payload = resource(3, resource_id)
                require(size == len(payload) and payload == expected[width], f"embedded PE {width}px pixels differ from generated ICO")
            groups.append({"name": name if name <= 0xffff else ctypes.wstring_at(name), "sizes": sorted(expected)})
            return True
        except Exception as error:
            errors.append(str(error))
            return False

    try:
        kernel.EnumResourceNamesW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, callback_type, ctypes.c_ssize_t]
        kernel.EnumResourceNamesW.restype = wintypes.BOOL
        ok = kernel.EnumResourceNamesW(module, 14, collect, 0)
        require(not errors, "; ".join(errors))
        require(ok and groups, f"no readable RT_GROUP_ICON/RT_ICON: {path}")
        return {"path": str(path), "groups": groups}
    finally:
        kernel.FreeLibrary(module)


def check_native(build, platform, staged=None):
    require((platform == "macos" and sys.platform == "darwin") or (platform == "windows" and sys.platform == "win32") or (platform == "linux" and sys.platform.startswith("linux")), "native checks must run on their actual platform")
    cache = (build / "CMakeCache.txt").read_text()
    standalone = "ANAMORPH_BUILD_STANDALONE:BOOL=ON" in cache
    require(standalone or "ANAMORPH_BUILD_STANDALONE:BOOL=OFF" in cache, "missing Standalone build policy")
    generated = build / "Anamorph_artefacts/JuceLibraryCode"
    products = build / "Anamorph_artefacts/Release"
    result = {"platform": platform, "standalone": standalone, "staged": str(staged) if staged else None}
    if platform == "windows":
        images = icon_images((generated / "icon.ico").read_bytes())
        require(set(images) == {16, 32, 48, 256}, "unexpected pinned-JUCE Windows frames")
        bundle = (staged / "Anamorph.vst3") if staged else (products / "VST3/Anamorph.vst3")
        require((bundle / "Plugin.ico").read_bytes() == (generated / "icon.ico").read_bytes(), "VST3 Plugin.ico differs")
        ini = (bundle / "desktop.ini").read_text()
        require("IconResource=Plugin.ico,0" in ini, "VST3 desktop.ini lost its icon")
        targets = [bundle / "Contents/x86_64-win/Anamorph.vst3"]
        if standalone:
            targets.append((staged / "Anamorph.exe") if staged else (products / "Standalone/Anamorph.exe"))
        result["pe_resources"] = [windows_resources(path, images) for path in targets]
        result["frame_sizes"] = sorted(images)
    elif platform == "macos":
        data = (generated / "AppIcon.icns").read_bytes()
        images = icns_images(data)
        require(set(images) == {"icp5", "ic10"}, "unexpected pinned-JUCE macOS entries")
        formats = [("VST3", "Anamorph.vst3"), ("AU", "Anamorph.component")]
        if standalone:
            formats.append(("Standalone", "Anamorph.app"))
        result["bundles"] = []
        for kind, name in formats:
            bundle = (staged / name) if staged else (products / kind / name)
            plist = bundle / "Contents/Info.plist"
            subprocess.run(["plutil", "-lint", str(plist)], check=True, stdout=subprocess.DEVNULL)
            info = plistlib.loads(plist.read_bytes())
            icon_name = info.get("CFBundleIconFile", "")
            require(icon_name and Path(icon_name).name == icon_name, "invalid CFBundleIconFile")
            icon = bundle / "Contents/Resources" / (icon_name if icon_name.endswith(".icns") else icon_name + ".icns")
            require(icon.read_bytes() == data, f"bundle ICNS differs: {bundle}")
            result["bundles"].append({"path": str(bundle), "CFBundleIdentifier": info["CFBundleIdentifier"], "CFBundleIconFile": icon_name})
        with tempfile.TemporaryDirectory() as directory:
            extracted = Path(directory) / "Anamorph.iconset"
            subprocess.run(["iconutil", "--convert", "iconset", "--output", str(extracted), str(generated / "AppIcon.icns")], check=True)
            pngs = list(extracted.glob("*.png"))
            require(pngs, "iconutil extracted no images")
            for path in pngs:
                subprocess.run(["sips", "-g", "pixelWidth", "-g", "pixelHeight", str(path)], check=True, stdout=subprocess.DEVNULL)
        result["icns_entries"] = {kind: png_size(data) for kind, data in images.items()}
        result["iconutil"] = "native extraction passed"
    else:
        require(not (generated / "icon.ico").exists() and not (generated / "AppIcon.icns").exists(), "unexpected Linux native icon container; stale build?")
        bundle = (staged / "Anamorph.vst3") if staged else (products / "VST3/Anamorph.vst3")
        targets = [bundle / "Contents/x86_64-linux/Anamorph.so"]
        if standalone:
            targets.append((staged / "Anamorph") if staged else (products / "Standalone/Anamorph"))
        for path in targets:
            with path.open("rb") as stream:
                header = stream.read(20)
            require(header[:5] == b"\x7fELF\x02" and struct.unpack_from("<H", header, 18)[0] == 62, f"expected x86-64 ELF: {path}")
        result["elf_outputs"] = [str(path) for path in targets]
        result["embedded_icon"] = "not supported by this Linux JUCE path"
    return result


def self_test():
    """Exercise real parsers and source checks with synthetic good/bad inputs."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xffffffff)

    def png(size):
        row = b"\0" + b"\0\x44\x88\xff" * size
        return PNG + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(row * size)) + chunk(b"IEND", b"")

    def rejects(function, *args):
        try:
            function(*args)
        except (ValueError, KeyError, IndexError, struct.error, zlib.error):
            return
        raise AssertionError("checker accepted seeded invalid input")

    small, big = png(32), png(1024)
    assert png_size(small) == 32 and png_size(big) == 1024
    rejects(png_size, small[:-1])
    rejects(png_size, small[:40] + bytes([small[40] ^ 1]) + small[41:])
    ico = struct.pack("<HHHBBBBHHII", 0, 1, 1, 32, 32, 0, 0, 1, 32, len(small), 22) + small
    assert set(icon_images(ico)) == {32}
    rejects(icon_images, ico[:21])
    rejects(icon_images, ico[:22] + small[:-1])
    pixels = b"\0\x44\x88\xff" * 255 + b"\0\0\0\0"
    dib = struct.pack("<IiiHHIIIIII", 40, 16, 32, 1, 32, 0, 1088, 0, 0, 0, 0) + pixels + bytes(64)
    dib_ico = struct.pack("<HHHBBBBHHII", 0, 1, 1, 16, 16, 0, 0, 1, 32, len(dib), 22) + dib
    assert set(icon_images(dib_ico)) == {16}
    rejects(icon_images, dib_ico[:-1])
    opaque_dib = dib[:40] + b"\0\x44\x88\xff" * 256 + bytes(64)
    rejects(icon_images, dib_ico[:22] + opaque_dib)
    body = b"icp5" + struct.pack(">I", len(small) + 8) + small
    icns = b"icns" + struct.pack(">I", len(body) + 8) + body
    assert set(icns_images(icns)) == {"icp5"}
    rejects(icns_images, icns[:-1])
    group = struct.pack("<HHHBBBBHHIH", 0, 1, 1, 32, 32, 0, 0, 1, 32, len(small), 101)
    assert group_entries(group) == [(32, len(small), 101)]
    rejects(group_entries, group[:-1])
    for platform, matches in [("windows", sys.platform == "win32"), ("macos", sys.platform == "darwin"), ("linux", sys.platform.startswith("linux"))]:
        if not matches:
            rejects(check_native, Path("nonexistent-self-test-build"), platform)
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "resources/icons").mkdir(parents=True)
        records = []
        cmake = "juce_add_plugin(Anamorph\n"
        for name, (size, argument) in INPUTS.items():
            data = big if size == 1024 else small
            destination = "resources/icons/" + name
            (root / destination).write_bytes(data)
            records.append({"destination": destination, "sha256": sha(data)})
            cmake += f'    {argument} "${{CMAKE_CURRENT_SOURCE_DIR}}/{destination}"\n'
        cmake += ")\n"
        (root / "CMakeLists.txt").write_text(cmake)
        provenance = {"source_repository": "https://github.com/skyRolly/icon-vector-reconstruction", "source_commit": "a" * 40, "selected_design": "B0", "assets": records}
        (root / "resources/icons/PROVENANCE.json").write_text(json.dumps(provenance))
        check_sources(root)
        provenance["assets"].append(records[0])
        (root / "resources/icons/PROVENANCE.json").write_text(json.dumps(provenance))
        rejects(check_sources, root)
        provenance["assets"].pop()
        (root / "resources/icons/PROVENANCE.json").write_text(json.dumps(provenance))
        (root / "resources/icons/anamorph-32.png").write_bytes(big)
        rejects(check_sources, root)
        (root / "resources/icons/anamorph-32.png").write_bytes(small)
        (root / "CMakeLists.txt").write_text(cmake.replace("ICON_SMALL", "WRONG_ARGUMENT"))
        rejects(check_sources, root)
    print("icon checker self-test passed (valid fixtures and seeded failures)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--platform", choices=["windows", "macos", "linux"])
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--staged-dir", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    result = {"provenance": check_sources(ROOT)}
    if args.platform:
        result["native"] = check_native(args.build_dir.resolve(), args.platform, args.staged_dir.resolve() if args.staged_dir else None)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n")
    print("icon sources/resources validated" + (f" on {args.platform}" if args.platform else ""))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, IndexError, OSError, struct.error, zlib.error, subprocess.CalledProcessError) as error:
        print(f"::error::icon validation: {error}", file=sys.stderr)
        sys.exit(1)
