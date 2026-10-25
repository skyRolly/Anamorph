#!/usr/bin/env python3
"""Check production Build/Release selection with inert event and command fixtures.

This is a small evaluator for the Boolean job-condition subset used here, not an
Actions emulator. It runs Release metadata and TSan environment bodies in private fixtures;
no installer, compiler, workflow dispatch, tag or release is executed/created.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

TOKEN = re.compile(r"\s*(\|\||&&|==|!=|!|\(|\)|'[^']*'|\d+|[A-Za-z_][A-Za-z0-9_.-]*)")


def expression(source, context):
    source = source.strip()
    if source.startswith('${{'): source = source[3:-2].strip()
    tokens = []; cursor = 0
    while cursor < len(source):
        match = TOKEN.match(source, cursor)
        if not match: raise ValueError('unsupported expression: ' + source[cursor:])
        tokens.append(match[1]); cursor = match.end()
    index = 0
    def primary():
        nonlocal index
        token = tokens[index]; index += 1
        if token == '!': return not primary()
        if token == '(':
            value = either(); assert tokens[index] == ')'; index += 1; return value
        if token.startswith("'"): return token[1:-1]
        if token.isdigit(): return int(token)
        if token in ('true', 'false'): return token == 'true'
        return context.get(token, '')
    def equal():
        nonlocal index
        value = primary()
        if index < len(tokens) and tokens[index] in ('==', '!='):
            op = tokens[index]; index += 1; right = primary()
            value = value == right if op == '==' else value != right
        return value
    def both():
        nonlocal index
        value = equal()
        while index < len(tokens) and tokens[index] == '&&':
            index += 1; right = equal(); value = bool(value and right)
        return value
    def either():
        nonlocal index
        value = both()
        while index < len(tokens) and tokens[index] == '||':
            index += 1; right = both(); value = bool(value or right)
        return value
    value = either(); assert index == len(tokens), source
    return value


def jobs(source):
    return {m[1]: m[2] for m in re.finditer(
        r'^  ([a-z][a-z0-9-]*):\n(.*?)(?=^  [a-z][a-z0-9-]*:\n|\Z)',
        source.split('\njobs:\n', 1)[1], re.M | re.S)}


def field(body, name, default=''):
    match = re.search(r'^    ' + name + r':([^\n]*)\n((?:      [^\n]*\n)*)', body, re.M)
    if not match: return default
    return match[2].strip() if match[1].strip() in ('>-', '|') else match[1].strip()


def selection(source, context):
    data = jobs(source); ctx = dict(context)
    # Model the old production router to prove the review-branch Release bug.
    if 'validation-route' in data:
        route = re.search(r'CONTAINED: (\$\{\{ .* \}\})', data['validation-route'])[1]
        ctx['needs.validation-route.outputs.contained'] = str(bool(expression(route, ctx))).lower()
    selected = []; done = set()
    while len(done) < len(data):
        before = len(done)
        for name, body in data.items():
            if name in done: continue
            need = field(body, 'needs').strip('[]')
            needs = [v.strip() for v in need.split(',') if v.strip()]
            if not all(n in done for n in needs): continue
            done.add(name)
            if all(n in selected for n in needs) and expression(field(body, 'if', 'true'), ctx): selected.append(name)
        assert len(done) > before, 'unknown/cyclic dependency'
    return selected


def verify_tsan_workspace(build):
    body = jobs(build)['tsan']
    match = re.search(r'      - name: Resolve TSan suppressions inside the container checkout\n        run: \|\n((?:          [^\n]*\n)+)', body)
    assert match, 'container TSan must resolve its suppression file from the real checkout'
    command = '\n'.join(line[10:] for line in match[1].splitlines())+'\n'
    with tempfile.TemporaryDirectory(prefix='tsan-workspace-') as directory:
        fixture = Path(directory)/'checkout with spaces'; (fixture/'tests').mkdir(parents=True)
        suppression = fixture/'tests/tsan-suppressions.txt'
        suppression.write_text('deadlock:inert_fixture\n')
        output = fixture/'environment'
        env = dict(os.environ, GITHUB_ENV=str(output), GITHUB_WORKSPACE='/unavailable/host/checkout',
                   TSAN_OPTIONS='suppressions=/unavailable/host/checkout/tests/tsan-suppressions.txt')
        run = subprocess.run(['bash','-e','-o','pipefail','-c',command], cwd=fixture, env=env, capture_output=True, text=True)
        assert run.returncode == 0, run.stdout+run.stderr
        expected = 'halt_on_error=1:exitcode=66:report_bugs=1:suppressions='+str(suppression)+':print_suppressions=1'
        assert output.read_text() == 'TSAN_OPTIONS='+expected+'\n'
        return {'production_environment_body_executed': True, 'checkout_path_contains_spaces': True,
                'unavailable_host_path_rejected': True, 'sanitizer_or_compiler_executed': False,
                'sanitizer_flags_and_suppression_rules_changed': False}


def verify(build_path, release_path, root):
    build = build_path.read_text(); release = release_path.read_text()
    product = 'Anabasis' if re.search(r'^name:.*Anabasis', (root/'CMakeLists.txt').read_text(), re.M) else None
    product = product or ('Anabasis' if 'ANABASIS_' in build else 'Anamorph')
    branches = [('codex/macos-installed-receipts', 174 if product == 'Anamorph' else 54),
                ('codex/linux-desktop-native-icon', 175) if product == 'Anamorph' else ('codex/windows-uninstall-icon-correctness', 53)]
    records = []
    def check(label, event, ref, number=999, fork=False, caller='Build'):
        ctx = {'github.repository': 'skyRolly/'+product, 'github.event_name': event,
               'github.ref': ('refs/pull/'+str(number)+'/merge' if event=='pull_request' else ref), 'github.event.pull_request.head.repo.full_name': ('fork/' if fork else 'skyRolly/')+product,
               'github.event.pull_request.base.ref': 'main', 'github.event.pull_request.head.ref': ref.removeprefix('refs/heads/'),
               'github.event.pull_request.number': number, 'needs.preflight.outputs.ready': 'true'}
        selected = selection(build, ctx)
        expected = {'merge-check'} if event == 'pull_request' and not fork else set(jobs(build)) - {'merge-check', 'validation-route', 'full-validation-pending'}
        # In the broken implementation, Release dispatch selects the pending
        # marker instead of linux/windows/macos. Keep this assertion first.
        if caller == 'Release':
            assert {'linux', 'windows', 'macos'} <= set(selected), label + ': Release rehearsal skipped the build matrix: ' + repr(selected)
        assert set(selected) == expected, label + ': incorrect normal job selection: ' + repr(selected)
        assert not {'validation-route', 'full-validation-pending'} & set(selected), label
        records.append({'case': label, 'caller': caller, 'event': event, 'ref': ctx['github.ref'], 'head_ref': ctx['github.event.pull_request.head.ref'], 'fork': fork, 'selected_jobs': selected})
    # Test the exact collision before generic main/tag cases.
    for branch, number in branches:
        check('release-review-dispatch-'+str(number), 'workflow_dispatch', 'refs/heads/'+branch, caller='Release')
        for event in ('push', 'pull_request', 'workflow_dispatch'):
            check('direct-build-'+event+'-'+str(number), event, 'refs/heads/'+branch, number)
        check('fork-pr-'+str(number), 'pull_request', 'refs/heads/'+branch, number, True)
    for branch in ('main', 'codex/unrelated'):
        for event in ('push', 'pull_request', 'workflow_dispatch'):
            check('direct-'+event+'-'+branch, event, 'refs/heads/'+branch)
        check('release-dispatch-'+branch, 'workflow_dispatch', 'refs/heads/'+branch, caller='Release')
    # Use the repository's actual release trigger and CMake version. Anamorph
    # uses bare version tags (ADR-0059); the sibling uses a v prefix.
    tag_glob = re.search(r'^    tags:\n      - "([^"]+)"', release, re.M)[1]
    assert tag_glob in ('[0-9]+.[0-9]+.[0-9]+', 'v[0-9]+.[0-9]+.[0-9]+'), 'review new release tag grammar'
    version = re.search(r'^project\(\S+ VERSION ([0-9]+\.[0-9]+\.[0-9]+)', (root/'CMakeLists.txt').read_text(), re.M)[1]
    tag = ('v' if tag_glob.startswith('v') else '') + version
    for event in ('push', 'workflow_dispatch'):
        check('release-tag-'+event, event, 'refs/tags/'+tag, caller='Release')
    assert re.search(r'^  workflow_call:\s*(?:#.*)?$', build, re.M), 'no reusable Build trigger'
    # No invocation inputs/defaults or nested Build calls remain in this design.
    assert not re.search(r'^    inputs:', build.split('\npermissions:',1)[0], re.M)
    assert not re.search(r'^    uses:', build, re.M), 'review new nested reusable calls'
    release_jobs = jobs(release)
    assert field(release_jobs['build'], 'uses') == './.github/workflows/build.yml'
    assert not re.search(r'^    with:', release_jobs['build'], re.M), 'review new caller inputs'
    assert field(release_jobs['draft-release'], 'if') == "needs.validate.outputs.is-release == 'true'"
    assert "permissions:\n  contents: read" in build and "permissions:\n  contents: read" in release
    assert 'SOURCE_PUBLICATION_ONLY' not in (root/'.github/workflows/codeql.yml').read_text()
    assert 'validation-route' not in build and 'full-validation-pending' not in build
    assert not re.search(r'\bsysctl\s+(?:-\S+\s+)*-w\b', build), 'workflow must not change kernel settings'
    meta = re.search(r'^        run: \|\n((?:          [^\n]*\n|\n)+)', release_jobs['validate'], re.M)[1]
    meta = '\n'.join(line[10:] if line.startswith('          ') else line for line in meta.splitlines())+'\n'
    assert 'GITHUB_EVENT_NAME' in meta and '== push' in meta
    rehearsals = []
    with tempfile.TemporaryDirectory(prefix='release-routing-') as directory:
        fixture = Path(directory); (fixture/'scripts').mkdir()
        for file in ('CMakeLists.txt','CHANGELOG.md','scripts/changelog-section.awk'):
            if (root/file).is_file(): (fixture/file).write_bytes((root/file).read_bytes())
        for ref in [*(f'refs/heads/{b}' for b,_ in branches),'refs/heads/main','refs/heads/codex/unrelated','refs/tags/'+tag]:
            output = fixture/'outputs'; output.unlink(missing_ok=True)
            env = dict(os.environ, GITHUB_EVENT_NAME='workflow_dispatch', GITHUB_REF=ref,
                       GITHUB_OUTPUT=str(output), RUNNER_TEMP=str(fixture), GITHUB_SHA='fixture')
            run = subprocess.run(['bash','-e','-o','pipefail','-c',meta],cwd=fixture,env=env,capture_output=True,text=True)
            assert run.returncode == 0, run.stdout+run.stderr
            values = dict(line.split('=',1) for line in output.read_text().splitlines())
            assert values['is-release'] == 'false', 'rehearsal could reach draft-release'
            rehearsals.append({'ref':ref,'metadata_outputs':values,'draft_release_selected':False})
    return {'context_cases':records, 'real_metadata_body_fixtures':rehearsals,
            'release_tag_fixture': {'production_trigger_glob': tag_glob, 'CMake_version': version, 'tag': tag},
            'tsan_container_workspace_fixture': verify_tsan_workspace(build) if product == 'Anamorph' else None,
            'inputs_or_defaults':None, 'nested_calls':'Release -> local Build only',
            'workflow_digests':{str(p.relative_to(root)) if p.is_relative_to(root) else str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (build_path,release_path)}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path)
    parser.add_argument('--release',type=Path)
    parser.add_argument('--report',type=Path)
    args = parser.parse_args(); root = Path(__file__).resolve().parent.parent
    result = verify(args.build or root/'.github/workflows/build.yml',args.release or root/'.github/workflows/release.yml',root)
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(result,indent=2)+'\n')

if __name__ == '__main__': main()
