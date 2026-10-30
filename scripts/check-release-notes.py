#!/usr/bin/env python3
"""Keep internal release-readiness records out of the current customer notes.

Use the production awk extractor; leave its Markdown and metadata contracts intact.
Historical entries and Unreleased are outside this narrowly scoped content check.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


PATTERNS = (
    r'\bdraft[ -]*(?:release|readiness|ready|status|only)\b',
    r'\b(?:KI|RISK)-[0-9]+\b',
    r'\bworklogs/',
    r'\b(?:owner|human|manual)[ -]+(?:approval|approved|attestation|attested)\b',
    r'\bapproved by (?:the )?(?:owner|maintainer)\b',
    r'\barchitecture review\b',
    r'\b(?:automated|release|CI)[ -]+(?:approval|gates?)\b',
    r'\b(?:manual|human) validations?\b',
)


def findings(notes):
    folded = ' '.join(notes.split())
    return [pattern for pattern in PATTERNS if re.search(pattern, folded, re.I)]


def extract(root, changelog, version):
    return subprocess.run(
        ['awk', '-v', 'ver=' + version, '-f', str(root / 'scripts/changelog-section.awk'),
         str(changelog)], check=True, capture_output=True, text=True).stdout


def self_test(root):
    good = ('## [0.10.0] — 2026-10-30\n\n### Fixed\n'
            '- Session recall keeps the selected preset. Evidence: PR #168; ADR-0036. [Verified]\n'
            '- Windows installers are unsigned; macOS packages are not notarized.\n')
    assert not findings(good)
    bad = ['Draft release.', 'Draft-readiness status: ready.', 'KI-029 remains open.',
           'RISK-009', 'worklogs/RELEASE_READINESS_0.10.0.md', 'Owner approval complete.',
           'Human approved', 'Manual attestation', 'Approved by the owner',
           'Architecture Review pending', 'Automated gates passed', 'CI approval bookkeeping',
           'Release gates remain required', 'Human validations complete',
           'Required manual\nvalidations are approved by the owner.']
    for phrase in bad:
        assert findings(good + '\n' + phrase), phrase
    # Internal text elsewhere must not contaminate this version's extracted body.
    document = ('## [Unreleased]\n\nDraft readiness KI-029\n\n' + good
                + '\n## [0.9.9] — 2026-10-22\n\nHistorical owner approval\n')
    with tempfile.TemporaryDirectory() as temporary:
        path = Path(temporary) / 'CHANGELOG.md'
        path.write_text(document, encoding='utf-8')
        notes = extract(root, path, '0.10.0')
        assert notes.strip() == good.strip()
        assert not findings(notes)
        path.write_text(document.replace('### Fixed', 'Draft release.\n\n### Fixed'), encoding='utf-8')
        assert findings(extract(root, path, '0.10.0'))
    print('Release-note content self-test: valid notes, forbidden prose and extracted-section controls passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--notes', type=Path, help='Check an already extracted/downloaded notes body')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    if args.self_test:
        self_test(root)
        return 0
    if args.notes:
        notes = args.notes.read_text(encoding='utf-8')
    else:
        match = re.search(r'^project\(Anamorph VERSION ([0-9]+\.[0-9]+\.[0-9]+) ',
                          (root / 'CMakeLists.txt').read_text(encoding='utf-8'), re.M)
        if not match:
            parser.error('could not read the CMake project version')
        notes = extract(root, root / 'CHANGELOG.md', match[1])
    if not notes.strip():
        print('::error::empty release notes')
        return 1
    hits = findings(notes)
    for hit in hits:
        print('::error::internal readiness prose in customer release notes: ' + hit)
    return int(bool(hits))


if __name__ == '__main__':
    raise SystemExit(main())
