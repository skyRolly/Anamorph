#!/usr/bin/env python3
"""Local input guards and disposable Ninja rebuild controls; no audio evidence."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock
import replay_provenance as prov


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], stderr=subprocess.DEVNULL).decode().strip()


def repo(path):
    path.mkdir()
    subprocess.run(['git', 'init', '-q', str(path)], check=True)
    git(path, 'config', 'user.email', 'test@example.invalid')
    git(path, 'config', 'user.name', 'Replay test')
    (path/'src').mkdir()
    (path/'src/source.cpp').write_text('known source\n')
    (path/'.gitignore').write_text('src/ignored.h\n')
    git(path, 'add', '.')
    git(path, 'commit', '-qm', 'fixture')
    return git(path, 'rev-parse', 'HEAD')


class Provenance(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source, self.juce, self.build = (self.root/x for x in ('source','juce','build'))
        source_sha, juce_sha = repo(self.source), repo(self.juce)
        self.build.mkdir()
        self.patch1 = mock.patch.object(prov, 'BASE', source_sha); self.patch1.start(); self.addCleanup(self.patch1.stop)
        self.patch2 = mock.patch.object(prov, 'JUCE', juce_sha); self.patch2.start(); self.addCleanup(self.patch2.stop)
        self.cache(self.source, self.juce)

    def cache(self, source, juce):
        (self.build/'CMakeCache.txt').write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={source}\nANAMORPH_JUCE_PATH:STRING={juce}\n')

    def test_matching_checkout_and_build(self):
        self.assertEqual(prov.verify_inputs(self.source, self.build), self.juce)

    def test_dirty_tracked_untracked_and_ignored(self):
        for name in ('src/source.cpp', 'src/new.h', 'src/ignored.h'):
            path = self.source/name
            original = path.read_bytes() if path.exists() else None
            path.write_text('unexpected\n')
            with self.assertRaises(ValueError): prov.verify_inputs(self.source, self.build)
            if original is None: path.unlink()
            else: path.write_bytes(original)

    def test_build_from_another_checkout(self):
        self.cache(self.root/'elsewhere', self.juce)
        with self.assertRaises(ValueError): prov.verify_inputs(self.source, self.build)

    def test_wrong_and_dirty_dependency(self):
        with mock.patch.object(prov, 'JUCE', '0'*40):
            with self.assertRaises(ValueError): prov.verify_inputs(self.source, self.build)
        (self.juce/'src/source.cpp').write_text('changed\n')
        with self.assertRaises(ValueError): prov.verify_inputs(self.source, self.build)

    def test_archived_diagnostic_exact_bytes(self):
        archive = Path(__file__).resolve().parent/'original'
        source = archive/'engine-endpoint.cpp'
        prov.verify_archive_file(archive, source, source.name)
        altered = self.root/source.name; altered.write_bytes(source.read_bytes()+b'\n')
        with self.assertRaises(ValueError): prov.verify_archive_file(archive, altered, source.name)

    def test_diagnostic_exact_scope(self):
        import difflib
        engine = self.source/'src/dsp/AnamorphEngine.cpp'
        engine.parent.mkdir(parents=True)
        engine.write_text('line one\nline two\n')
        archive = self.root/'archive'; archive.mkdir()
        variant = archive/'engine-diagnostic.cpp'; variant.write_text('line one\nchanged\n')
        patch = archive/'diagnostic.patch'
        patch.write_text(''.join(difflib.unified_diff(engine.read_text().splitlines(keepends=True),
            variant.read_text().splitlines(keepends=True), fromfile='baseline/AnamorphEngine.cpp',
            tofile='diagnostic/AnamorphEngine.cpp')))
        with mock.patch.object(prov, 'verify_archive_file'):
            prov.verify_variant(self.source, archive, 'diagnostic')
            variant.write_text(variant.read_text()+'unintended\n')
            with self.assertRaises(ValueError): prov.verify_variant(self.source, archive, 'diagnostic')

    def test_unverifiable_build_is_never_reused(self):
        fresh = self.root/'fresh'
        fresh.mkdir()
        (fresh/'object').write_text('stale')
        with self.assertRaises(ValueError):
            prov.fresh_build(self.source, self.build, fresh, 'AnamorphTests')
        self.assertEqual((fresh/'object').read_text(), 'stale')

    def test_fresh_configure_and_build(self):
        fresh = self.root/'fresh'
        # A synthetic CMake fixture tests orchestration, not the product build.
        (self.source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.22)\nproject(replay NONE)\nadd_custom_target(AnamorphTests COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/object)\n')
        git(self.source, 'add', 'CMakeLists.txt'); git(self.source, 'commit', '-qm', 'cmake fixture')
        with mock.patch.object(prov, 'BASE', git(self.source, 'rev-parse', 'HEAD')):
            prov.fresh_build(self.source, self.build, fresh, 'AnamorphTests')
        self.assertTrue((fresh/'object').exists())


if __name__ == '__main__': unittest.main(verbosity=2)
