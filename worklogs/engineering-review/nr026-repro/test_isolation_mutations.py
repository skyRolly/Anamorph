#!/usr/bin/env python3
"""Prove the cleanup and interruption controls reject two unsafe mutations."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True, help='new evidence directory')
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(mode=0o700)  # Refuse an existing output directory; never delete it.
    here = Path(__file__).resolve().parent
    source = (here / 'isolation.py').read_text()
    mutations = (
        ('root-identity', 'or (info.st_dev, info.st_ino) != (self.identity.st_dev, self.identity.st_ino)):',
         'or False):', 'FAIL: test_cleanup_refuses_a_replaced_root'),
        ('process-group', 'os.killpg(child.pid, signal.SIGKILL)',
         'os.kill(child.pid, signal.SIGKILL)', 'FAIL: test_interruption_stops_child_and_preserves_external_output'),
    )
    with tempfile.TemporaryDirectory(prefix='nr026-mutations-', dir='/tmp') as raw:
        fixture = Path(raw)
        shutil.copyfile(here / 'test_isolation.py', fixture / 'test_isolation.py')
        for label, before, after, expected in mutations:
            if source.count(before) != 1:
                raise SystemExit(f'{label}: mutation no longer applies exactly once')
            (fixture / 'isolation.py').write_text(source.replace(before, after))
            shutil.rmtree(fixture / '__pycache__', ignore_errors=True)
            result = subprocess.run([sys.executable, str(fixture / 'test_isolation.py'), '--probe', str(probe)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            (out / (label + '.stdout')).write_bytes(result.stdout)
            (out / (label + '.stderr')).write_bytes(result.stderr)
            if result.returncode == 0 or expected.encode() not in result.stderr:
                raise SystemExit(f'{label}: unsafe mutation was not rejected; inspect {out}')
            print(f'{label}: unsafe mutation rejected (exit {result.returncode})')


if __name__ == '__main__':
    main()
