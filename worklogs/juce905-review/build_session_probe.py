"""Build the session comparison entry point using this build's actual state-target commands.
Usage: python3 build_session_probe.py BUILD_DIRECTORY OUTPUT_EXECUTABLE
Use a separate verified JUCE checkout/build for each pin; never mix headers and objects.
"""
from pathlib import Path
import shlex
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
repo = Path(__file__).resolve().parents[2]
commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands',
                                    'AnamorphStateTests'], text=True).splitlines()
source = next(c for c in commands if ' -c ' + str(repo / 'tests/state_tests.cpp') in c)
args = shlex.split(source)
args[args.index('-o') + 1] = str(output) + '.o'
args[args.index('-c') + 1] = str(Path(__file__).with_name('session_probe.cpp'))
if '-MF' in args:
    args[args.index('-MF') + 1] = str(output) + '.d'
subprocess.run(args, cwd=build, check=True)
link = shlex.split(commands[-1])
link = link[link.index('&&') + 1:]
link = link[:link.index('&&')]
link = [str(output) + '.o' if a.endswith('/tests/state_tests.cpp.o') else a for a in link]
link = [a for a in link if not a.startswith('-Wl,--dependency-file=')]
link[link.index('-o') + 1] = str(output)
subprocess.run(link, cwd=build, check=True)
