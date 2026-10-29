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


class RoutingError(AssertionError):
    """A production routing assertion failed, rather than fixture parsing."""


def permissions(source, indent, inherited=None):
    """The explicit map subset used here; omitted scopes in a map mean none."""
    match = re.search(r'^' + ' ' * indent + r'permissions:([^\n]*)\n'
                      r'((?:' + ' ' * (indent + 2) + r'[^\n]*\n)*)', source, re.M)
    if not match:
        return dict(inherited or {})
    inline = match[1].split('#', 1)[0].strip()
    if inline == '{}': return {}
    if inline: raise RoutingError('review unsupported inline permissions: ' + inline)
    result = {}
    for line in match[2].splitlines():
        line = line.split('#', 1)[0].strip()
        if not line: continue
        scope, level = line.split(':', 1)
        level = level.strip()
        if level not in ('none', 'read', 'write'):
            raise RoutingError('review unsupported permission level: ' + level)
        result[scope.strip()] = level
    return result


def verify_reusable_permissions(build, release):
    caller = jobs(release)['build']
    granted = permissions(caller, 4, permissions(release.split('\njobs:\n', 1)[0], 0))
    defaults = permissions(build.split('\njobs:\n', 1)[0], 0)
    rank = {'none': 0, 'read': 1, 'write': 2}
    violations = []
    # GitHub validates every nested job before evaluating its if condition.
    for name, body in jobs(build).items():
        for scope, requested in permissions(body, 4, defaults).items():
            allowed = granted.get(scope, 'none')
            if rank[requested] > rank[allowed]:
                violations.append(f'{name} requests {scope}: {requested}, caller allows {allowed}')
    if violations: raise RoutingError('reusable permissions: ' + '; '.join(violations))
    return {'caller_permissions': granted, 'all_nested_jobs_checked': True}


def permission_self_test(build, release):
    verify_reusable_permissions(build, release)
    caller = jobs(release)['build']
    weakened = re.sub(r'^      actions: read[^\n]*\n', '', caller, flags=re.M)
    assert weakened != caller, 'review new Actions grant placement'
    broken = release.replace('  build:\n' + caller, '  build:\n' + weakened, 1)
    try: verify_reusable_permissions(build, broken)
    except RoutingError as error:
        assert 'macos-review-selections requests actions: read, caller allows none' in str(error)
        missing_grant = str(error)
    else: raise AssertionError('permission liveness: accepted the original missing grant')
    child = jobs(build)['macos-review-selections']
    stronger = child.replace('      actions: read', '      actions: write', 1)
    assert stronger != child
    stronger = re.sub(r'^    if:[^\n]*\n', '    if: false\n', stronger, count=1, flags=re.M)
    broken = build.replace('  macos-review-selections:\n' + child,
                           '  macos-review-selections:\n' + stronger, 1)
    try: verify_reusable_permissions(broken, release)
    except RoutingError as error:
        assert 'macos-review-selections requests actions: write, caller allows read' in str(error)
        disabled_write = str(error)
    else: raise AssertionError('permission liveness: skipped job escaped static validation')
    assert permissions('permissions:\n  actions: read\n', 0, {'contents': 'write'}) == {'actions': 'read'}
    return {'missing_grant_rejected': missing_grant,
            'disabled_write_request_rejected': disabled_write,
            'explicit_map_omitted_scopes_are_none': True}


def dependencies(body):
    return [value.strip() for value in field(body, 'needs').strip('[]').split(',') if value.strip()]


def selection(source, context, outcomes=None):
    # Only this repository's Boolean conditions and implicit success-of-needs
    # contract are modeled. Outcomes are inert fixture results, never job runs.
    data = jobs(source); ctx = dict(context); outcomes = outcomes or {}
    selected = []; states = {}
    while len(states) < len(data):
        before = len(states)
        for name, body in data.items():
            if name in states: continue
            needs = dependencies(body)
            if not all(n in states for n in needs): continue
            for need in needs: ctx['needs.'+need+'.result'] = states[need]
            permitted = all(states[n] == 'success' for n in needs)
            enabled = permitted and expression(field(body, 'if', 'true'), ctx)
            outcome = outcomes.get(name, 'success')
            assert outcome in ('success', 'failure', 'skipped'), outcome
            if enabled and outcome != 'skipped':
                selected.append(name); states[name] = outcome
            else: states[name] = 'skipped'
        assert len(states) > before, 'unknown/cyclic dependency'
    return selected


def verify_selection(build, product):
    branches = [('codex/macos-installed-receipts', 174 if product == 'Anamorph' else 54),
                ('codex/linux-desktop-native-icon', 175) if product == 'Anamorph' else ('codex/windows-uninstall-icon-correctness', 53)]
    records = []
    def check(label, event, ref, number=999, fork=False, caller='Build'):
        ctx = {'github.repository': 'skyRolly/'+product, 'github.event_name': event,
               'github.ref': ('refs/pull/'+str(number)+'/merge' if event=='pull_request' else ref),
               'github.event.pull_request.head.repo.full_name': ('fork/' if fork else 'skyRolly/')+product,
               'github.event.pull_request.base.ref': 'main', 'github.event.pull_request.head.ref': ref.removeprefix('refs/heads/'),
               'github.event.pull_request.number': number, 'needs.preflight.outputs.ready': 'true', 'inputs.integration-review': ''}
        selected = selection(build, ctx)
        expected = {'merge-check'} if event == 'pull_request' and not fork else set(jobs(build)) - {'merge-check', 'validation-route', 'full-validation-pending', 'macos-review-selections', 'macos-review-coinstallation'}
        if caller == 'Release' and not {'linux', 'windows', 'macos'} <= set(selected):
            raise RoutingError(label + ': Release rehearsal skipped the build matrix: ' + repr(selected))
        if set(selected) != expected:
            raise RoutingError(label + ': incorrect normal job selection: ' + repr(selected))
        assert not {'validation-route', 'full-validation-pending'} & set(selected), label
        records.append({'case': label, 'caller': caller, 'event': event, 'ref': ctx['github.ref'], 'head_ref': ctx['github.event.pull_request.head.ref'], 'fork': fork, 'selected_jobs': selected})
    for branch, number in branches:
        check('release-review-dispatch-'+str(number), 'workflow_dispatch', 'refs/heads/'+branch, caller='Release')
        for event in ('push', 'pull_request', 'workflow_dispatch'):
            check('direct-build-'+event+'-'+str(number), event, 'refs/heads/'+branch, number)
        check('fork-pr-'+str(number), 'pull_request', 'refs/heads/'+branch, number, True)
    for branch in ('main', 'codex/unrelated'):
        for event in ('push', 'pull_request', 'workflow_dispatch'):
            check('direct-'+event+'-'+branch, event, 'refs/heads/'+branch)
        check('release-dispatch-'+branch, 'workflow_dispatch', 'refs/heads/'+branch, caller='Release')
    return records, branches, check


def routing_self_test(build, product, verifier=verify_selection):
    # Use the SAME production assertions. Seed only the exact manual caller/ref
    # collision, not a parser error or a generic unrelated tag/main failure.
    verifier(build, product)
    branch = 'codex/macos-installed-receipts'
    body = jobs(build)['linux']
    condition = ' '.join(field(body, 'if', 'true').split())
    collision = "github.event_name == 'workflow_dispatch' && github.ref == 'refs/heads/"+branch+"'"
    changed = re.sub(r'^    if:[^\n]*\n(?:      [^\n]*\n)*', '', body, count=1, flags=re.M)
    changed = '    if: ${{ !('+collision+') && ('+condition+') }}\n'+changed
    broken = build.replace('  linux:\n'+body, '  linux:\n'+changed, 1)
    assert broken != build
    try: verifier(broken, product)
    except RoutingError as error:
        assert str(error).startswith('release-review-dispatch-') and ': Release rehearsal skipped the build matrix:' in str(error), str(error)
        rejection = str(error)
    else: raise AssertionError('routing liveness: verifier accepted the seeded skipped Release build')
    fixture = """name: inert
jobs:
  parent:
    if: github.event_name != 'pull_request' && inputs.enabled == 'yes'
  child:
    needs: parent
    if: needs.parent.outputs.ready == 'true'
  independent:
    if: true
"""
    ctx = {'github.event_name':'push', 'inputs.enabled':'yes', 'needs.parent.outputs.ready':'true'}
    assert selection(fixture,ctx) == ['parent','child','independent']
    for output in ('false',''):
        assert selection(fixture,dict(ctx, **{'needs.parent.outputs.ready':output})) == ['parent','independent']
    assert selection(fixture,dict(ctx, **{'github.event_name':'pull_request'})) == ['independent']
    assert selection(fixture,dict(ctx, **{'inputs.enabled':'no'})) == ['independent']
    assert selection(fixture,ctx,{'parent':'failure'}) == ['parent','independent']
    assert selection(fixture,ctx,{'parent':'skipped'}) == ['independent']
    assert expression("!(false || true && false) && ('yes' != 'no')", {}) is True
    assert expression("true && (false || 'x' == 'y')", {}) is False
    return {'valid_control':True, 'seeded_review_branch_release_rejection':rejection,
            'expression_and_needs_controls':True, 'runs_before_production_check':True}


def verify_preflight(build):
    data = jobs(build)
    if 'preflight' not in data: return None
    body = data['preflight']
    assert 'ready: ${{ steps.check.outputs.ready }}' in body
    match = re.search(r'      - id: check\n        shell: bash\n        run: \|\n((?:          [^\n]*\n)+)', body)
    assert match, 'review changed preflight command contract'
    command = '\n'.join(line[10:] for line in match[1].splitlines())+'\n'
    records=[]
    with tempfile.TemporaryDirectory(prefix='preflight-routing-') as directory:
        root=Path(directory)
        for present in (True,False):
            cmake=root/'CMakeLists.txt'; cmake.unlink(missing_ok=True)
            if present: cmake.write_text('# inert presence fixture\n')
            output=root/'outputs';output.unlink(missing_ok=True)
            run=subprocess.run(['bash','-e','-o','pipefail','-c',command],cwd=root,
                               env=dict(os.environ,GITHUB_OUTPUT=str(output)),capture_output=True,text=True)
            assert run.returncode==0, run.stderr
            expected='true' if present else 'false'
            assert output.read_text()=='ready='+expected+'\n'
            records.append({'CMakeLists_present':present,'ready':expected,'actual_command_executed':True})
    ctx={'github.event_name':'push','needs.preflight.outputs.ready':'true','inputs.integration-review':''}
    normal=set(selection(build,ctx)); gated={name for name,job in data.items() if 'preflight' in dependencies(job)}
    assert {'linux','windows','macos'} <= gated
    cases=[]
    for ready,outcome in [('false','success'),(None,'success'),('true','failure'),('true','skipped')]:
        changed=dict(ctx); changed.pop('needs.preflight.outputs.ready')
        if ready is not None:changed['needs.preflight.outputs.ready']=ready
        selected=set(selection(build,changed,{'preflight':outcome}))
        assert not selected & gated, (ready,outcome,selected)
        expected=normal-gated-({'preflight'} if outcome=='skipped' else set())
        assert selected==expected,(ready,outcome,selected,expected)
        cases.append({'ready':ready,'preflight_outcome':outcome,'selected_jobs':sorted(selected)})
    return {'production_command_controls':records,'downstream_withheld_controls':cases}


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
    permission_contract = verify_reusable_permissions(build, release)
    product = 'Anabasis' if re.search(r'^name:.*Anabasis', (root/'CMakeLists.txt').read_text(), re.M) else None
    product = product or ('Anabasis' if 'ANABASIS_' in build else 'Anamorph')
    records, branches, check = verify_selection(build, product)
    # Use the repository's actual release trigger and CMake version. Anamorph
    # uses bare version tags (ADR-0059); the sibling uses a v prefix.
    tag_glob = re.search(r'^    tags:\n      - "([^"]+)"', release, re.M)[1]
    assert tag_glob in ('[0-9]+.[0-9]+.[0-9]+', 'v[0-9]+.[0-9]+.[0-9]+'), 'review new release tag grammar'
    version = re.search(r'^project\(\S+ VERSION ([0-9]+\.[0-9]+\.[0-9]+)', (root/'CMakeLists.txt').read_text(), re.M)[1]
    tag = ('v' if tag_glob.startswith('v') else '') + version
    for event in ('push', 'workflow_dispatch'):
        check('release-tag-'+event, event, 'refs/tags/'+tag, caller='Release')
    assert re.search(r'^  workflow_call:\s*(?:#.*)?$', build, re.M), 'no reusable Build trigger'
    # Optional artifact consumers exist only on the macOS review branches.
    # Reusable Release calls declare/pass no inputs on any branch.
    call_block=build.split('  workflow_call:',1)[1].split('\npermissions:',1)[0]
    assert not re.search(r'^    (inputs|secrets):',call_block,re.M)
    optional='macos-review-selections' in jobs(build)
    if optional:
        assert "        default: ''" in build.split('\npermissions:',1)[0]
        for branch,_ in branches:
            ctx={'github.event_name':'workflow_dispatch','github.ref':'refs/heads/'+branch,
                 'inputs.integration-review':'fixture-manifest','needs.preflight.outputs.ready':'true'}
            selected=selection(build,ctx)
            assert 'macos-review-selections' in selected
            assert {'linux','windows','macos'} <= set(selected)
            if product=='Anamorph': assert 'macos-review-coinstallation' in selected
    else:
        assert not re.search(r'^    inputs:',build.split('\npermissions:',1)[0],re.M)

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
    return {'context_cases':records, 'reusable_permission_contract':permission_contract,
            'preflight_contract':verify_preflight(build), 'real_metadata_body_fixtures':rehearsals,
            'release_tag_fixture': {'production_trigger_glob': tag_glob, 'CMake_version': version, 'tag': tag},
            'tsan_container_workspace_fixture': verify_tsan_workspace(build) if product == 'Anamorph' else None,
            'inputs_or_defaults':{'direct_dispatch_integration_review_default':'' if optional else None,'workflow_call_inputs':None,'release_inputs_passed':None}, 'nested_calls':'Release -> local Build only',
            'workflow_digests':{str(p.relative_to(root)) if p.is_relative_to(root) else str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (build_path,release_path)}}


def release_create_self_test(release):
    """Run the real draft-creation body against a recording gh, never GitHub."""
    block = jobs(release)['draft-release'].split('      - name: Create DRAFT GitHub Release\n', 1)[1]
    body = re.search(r'^        run: \|\n((?:          [^\n]*\n|\n)+)', block, re.M)[1]
    body = '\n'.join(line[10:] if line.startswith('          ') else line for line in body.splitlines())+'\n'
    body = body.replace('${{ needs.validate.outputs.version }}', '0.10.0')
    source_sha = '1234567890abcdef1234567890abcdef12345678'  # inert fixture, not a repository object
    expected_assets = ['dist/Anamorph-0.10.0-' + suffix for suffix in
                       ('Linux.zip', 'Windows.zip', 'macOS.zip', 'Windows-Installer.exe',
                        'macOS.pkg', 'UserManual.md', 'NOTICE.txt', 'THIRD_PARTY_LICENSES.md',
                        'SUPPORT.md')]
    expected_assets += ['dist/SHA256SUMS.txt', 'dist/RELEASE_MANIFEST.txt']

    def verify_arguments(args):
        assert args[:3] == ['release', 'create', '0.10.0']
        assert '--draft' in args and '--verify-tag' in args
        for flag, value in (('--target', source_sha), ('--title', 'Anamorph 0.10.0'),
                            ('--notes-file', 'notes.md')):
            assert flag in args and args[args.index(flag)+1] == value, flag
        assert args[-11:] == expected_assets

    with tempfile.TemporaryDirectory(prefix='release-create-') as directory:
        fixture = Path(directory); capture = fixture/'arguments.json'
        shim = fixture/'gh'
        shim.write_text('#!/usr/bin/env python3\nimport json, os, sys\n'
                        'assert sys.argv[1:3] == ["release", "create"]\n'
                        'with open(os.environ["CAPTURE_PATH"], "w") as out: json.dump(sys.argv[1:], out)\n')
        shim.chmod(0o700)
        env = dict(os.environ, PATH=str(fixture)+os.pathsep+os.environ['PATH'],
                   CAPTURE_PATH=str(capture), GITHUB_SHA=source_sha, GH_TOKEN='inert-fixture')

        def recorded(script):
            capture.unlink(missing_ok=True)
            run = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', script], cwd=fixture,
                                 env=env, capture_output=True, text=True)
            assert run.returncode == 0, run.stderr
            return json.loads(capture.read_text())

        verify_arguments(recorded(body))
        controls = []
        for name, before, after in (
                ('missing target', '--target "${GITHUB_SHA}"', ''),
                ('mutable target', '--target "${GITHUB_SHA}"', '--target main'),
                ('missing draft', '--draft', ''),
                ('missing verified tag', '--verify-tag', '')):
            mutant = body.replace(before, after)
            assert mutant != body, name + ' control failed to modify production body'
            try:
                verify_arguments(recorded(mutant))
            except AssertionError:
                controls.append(name)
            else:
                raise AssertionError(name + ' escaped the creation contract')
    return {'production_body_recorded': True, 'negative_controls_rejected': controls,
            'source_target': 'GITHUB_SHA', 'draft': True, 'asset_count': 11}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--build',type=Path)
    parser.add_argument('--release',type=Path)
    parser.add_argument('--report',type=Path)
    args = parser.parse_args(); root = Path(__file__).resolve().parent.parent
    build_path=args.build or root/'.github/workflows/build.yml'
    product='Anabasis' if 'ANABASIS_' in build_path.read_text() else 'Anamorph'
    liveness=routing_self_test(build_path.read_text(),product)
    permission_liveness=permission_self_test(build_path.read_text(),
                                            (args.release or root/'.github/workflows/release.yml').read_text())
    release_create_liveness = (release_create_self_test((args.release or root/'.github/workflows/release.yml').read_text())
                               if product == 'Anamorph' else None)
    if args.self_test: return
    result = verify(args.build or root/'.github/workflows/build.yml',args.release or root/'.github/workflows/release.yml',root)
    result['same_run_liveness']=liveness
    result['same_run_permission_liveness']=permission_liveness
    result['same_run_draft_creation_liveness']=release_create_liveness
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(result,indent=2)+'\n')

if __name__ == '__main__': main()
