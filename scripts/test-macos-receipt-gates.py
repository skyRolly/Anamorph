#!/usr/bin/env python3
"""Replay fault outcomes against the production macOS Actions conditions.

No installer or artifact publication is performed. Temporary marker files stand
for execution, PKG upload and retained diagnostics; conditions come from build.yml.
"""
import argparse
import ast
import json
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PRODUCT = 'Anamorph'


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def condition_value(expression, outcomes, failed=False, cancelled=False):
    expression = expression.removeprefix('${{').removesuffix('}}').strip()
    status = bool(re.search(r'\b(?:always|cancelled|success|failure)\s*\(', expression))
    source = re.sub(r'steps\.([A-Za-z_][A-Za-z_0-9]*)\.outcome',
                    lambda m: 'outcomes[' + repr(m[1]) + ']', expression)
    source = source.replace('&&', ' and ').replace('||', ' or ')
    source = re.sub(r'!(?!=)', ' not ', source).strip()
    node = ast.parse(source, mode='eval')
    allowed = (ast.Expression, ast.BoolOp, ast.And, ast.Or, ast.UnaryOp, ast.Not,
               ast.Compare, ast.Eq, ast.NotEq, ast.Call, ast.Name, ast.Load,
               ast.Constant, ast.Subscript)
    for item in ast.walk(node):
        if not isinstance(item, allowed):
            raise ValueError('unsupported production expression: ' + expression)
        if isinstance(item, ast.Name) and item.id not in ('outcomes', 'always', 'cancelled', 'success', 'failure'):
            raise ValueError('unknown production expression name')
        if isinstance(item, ast.Call) and (not isinstance(item.func, ast.Name)
                or item.func.id == 'outcomes' or item.args or item.keywords):
            raise ValueError('unsupported production expression call')
    context = {'outcomes': outcomes, 'always': lambda: True, 'cancelled': lambda: cancelled,
               'success': lambda: not failed and not cancelled, 'failure': lambda: failed}
    value = bool(eval(compile(node, '<Actions condition>', 'eval'), {'__builtins__': {}}, context))
    # A status function, including !cancelled(), overrides implicit success().
    return value and (status or not failed) and (status or not cancelled)


def production_steps(path):
    text = path.read_text()
    names = ('Installed macOS receipt liveness', 'Install the real macOS package and inspect receipts',
             'Retain installed macOS receipt evidence', 'Upload macOS installer')
    found = {}
    for name in names:
        matches = list(re.finditer(r'^      - name: ' + re.escape(name) + r'\n(.*?)(?=^      - |\Z)', text, re.M | re.S))
        if len(matches) != 1:
            raise ValueError('expected one production step: ' + name)
        match = matches[0]; body = match[1]
        expression = re.search(r'^        if: (.*)$', body, re.M)
        identifier = re.search(r'^        id: (\w+)$', body, re.M)
        if not expression or 'continue-on-error:' in body:
            raise ValueError('missing condition or weakened production step: ' + name)
        value = expression[1]
        if value in ('>-', '>', '|-', '|'):
            continuation = body[expression.end():].splitlines()[1:]
            folded = []
            for line in continuation:
                if not line.startswith('          '):
                    break
                folded.append(line.strip())
            value = ' '.join(folded)
            if not value.startswith('${{') or not value.endswith('}}'):
                raise ValueError('incomplete folded production condition')
        found[name] = {'condition': value, 'id': identifier[1] if identifier else None,
                       'position': match.start(), 'body': body}
    live, install, diagnostics, upload = [found[name] for name in names]
    if live['id'] != 'receipt_liveness' or install['id'] != 'installed_receipts':
        raise ValueError('receipt liveness/installation stable IDs missing')
    if not live['position'] < install['position'] < diagnostics['position'] < upload['position']:
        raise ValueError('receipt step order changed')
    if 'scripts/test-macos-receipts.py --self-test' not in live['body']:
        raise ValueError('production liveness no longer runs the receipt self-test')
    return {'install': install['condition'], 'diagnostics': diagnostics['condition'],
            'upload': upload['condition']}


def replay(conditions, liveness='success', installation='success', cancelled=False):
    identifiers = set(re.findall(r'steps\.(\w+)\.outcome', ' '.join(conditions.values())))
    outcomes = {name: 'success' for name in identifiers}
    outcomes.update(receipt_liveness=liveness, installed_receipts='skipped')
    failed = liveness == 'failure'
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        if condition_value(conditions['install'], outcomes, failed, cancelled):
            (root / 'installer-executed').touch()
            (root / 'available-diagnostics').touch()
            outcomes['installed_receipts'] = installation
            failed |= installation == 'failure'
        if condition_value(conditions['upload'], outcomes, failed, cancelled):
            (root / 'pkg-artifact').touch()
        if (root / 'available-diagnostics').exists() and condition_value(conditions['diagnostics'], outcomes, failed, cancelled):
            (root / 'retained-diagnostics').touch()
        return {name: (root / filename).exists() for name, filename in
                [('installer', 'installer-executed'), ('pkg', 'pkg-artifact'),
                 ('diagnostics', 'retained-diagnostics')]}


def prove(conditions):
    check(replay(conditions) == {'installer': True, 'pkg': True, 'diagnostics': True}, 'valid installation was suppressed')
    results = []
    for live in ('failure', 'skipped'):
        result = replay(conditions, live)
        check(not result['installer'], 'installer ran after ' + live + ' liveness')
        check(not result['pkg'], 'PKG artifact after ' + live + ' liveness')
        # Independently prove the publication boundary, even if a preceding
        # native outcome were accidentally changed to success in a future edit.
        ids = set(re.findall(r'steps\.(\w+)\.outcome', conditions['upload']))
        outcomes = {name: 'success' for name in ids}; outcomes['receipt_liveness'] = live
        check(not condition_value(conditions['upload'], outcomes, failed=live == 'failure'), 'PKG upload ignores failed/skipped liveness')
        results.append({'liveness': live, **result})
    failed = replay(conditions, installation='failure')
    check(failed == {'installer': True, 'pkg': False, 'diagnostics': True}, 'failed installation publication/diagnostics boundary changed')
    check(not replay(conditions, cancelled=True)['installer'], 'cancelled installation executed')
    results.append({'installation': 'failure', **failed})
    return results


def self_test():
    good = {'install': "${{ !cancelled() && steps.package_macos_pkg.outcome == 'success' && steps.receipt_liveness.outcome == 'success' }}",
            'upload': "${{ !cancelled() && steps.package_macos_pkg.outcome == 'success' && steps.receipt_liveness.outcome == 'success' && steps.installed_receipts.outcome == 'success' }}",
            'diagnostics': "${{ always() && steps.installed_receipts.outcome != 'skipped' }}"}
    prove(good)
    check(condition_value("!cancelled() && steps.package_macos_pkg.outcome == 'success'", {'package_macos_pkg': 'success'}, failed=True), '!cancelled status semantics changed')
    check(not condition_value("steps.package_macos_pkg.outcome == 'success'", {'package_macos_pkg': 'success'}, failed=True), 'implicit success semantics changed')
    for key, message in [('install', 'installer ran after'), ('upload', 'PKG upload ignores')]:
        mutant = dict(good); mutant[key] = mutant[key].replace(" && steps.receipt_liveness.outcome == 'success'", '')
        try:
            prove(mutant)
        except AssertionError as error:
            check(message in str(error), 'scheduling mutant failed for an unrelated reason')
        else:
            raise AssertionError('scheduling dependency mutant survived: ' + key)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workflow', type=Path, default=ROOT / '.github/workflows/build.yml')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    self_test()
    result = {'source': str(args.workflow), 'faults': prove(production_steps(args.workflow)),
              'scope': 'production-condition replay with harmless temporary markers; no native installer or publication'}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n')
    print('receipt scheduling contract verified; dependency mutants rejected')


if __name__ == '__main__':
    main()
