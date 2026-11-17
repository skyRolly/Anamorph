#!/usr/bin/env bash
# Run only after the fresh combined receipt and all selected specs are committed.
# This owns a disposable diagnostic checkout; relinks are deliberately serial.
set -euo pipefail
if [ "$#" -ne 3 ]; then
  printf 'usage: %s RECEIPT_COMMIT RECEIPT_PATH OUTPUT_ROOT\n' "$0" >&2
  exit 2
fi
receipt_commit=$1
receipt_path=$2
output_root=$3
source_root=/workspace/numerical-combined-b182-isolated-source
diagnostic_root=/workspace/numerical-combined-mutant
juce_root=/workspace/JUCE-905-numerical-combined
build_root=/workspace/numerical-combined-fresh-b182/fresh-build
packet_root=/workspace/Anamorph/worklogs/staged-integration-maintenance-0102/review-20261117/combined-mutations-final-tests
spec_commit=ed7560e5cc5c982a8a94622e9e4db8a80484a911
spec_root=worklogs/staged-integration-maintenance-0102/review-20261117/combined-mutations-final-tests
expected=b182cc0a6d68b105a0226cfcffd9fe6cfe767003
[ "$(git -C "$diagnostic_root" rev-parse HEAD)" = "$expected" ]
[ -z "$(git -C "$diagnostic_root" status --short --ignored)" ]
[ ! -e "$output_root" ]
mkdir -p "$output_root"
cleanup() { git -C "$diagnostic_root" restore --worktree src/dsp/AnamorphEngine.cpp; }
trap cleanup EXIT
for name in n204-clear n204-retain n206-settled n206-smoothing n206-unguarded n206-interior \
            n207-settled n207-smoothing n207-held n207-right-settled n207-right-smoothing n207-right-held \
            n207-unguarded n207-interior n203-wet n202-ordinary n202-forced; do
  printf 'starting %s\n' "$name"
  [ -z "$(git -C "$diagnostic_root" status --short --ignored)" ]
  git -C "$diagnostic_root" apply --unidiff-zero "$packet_root/$name.patch"
  docker run --rm --cpus=2 --user "$(id -u):$(id -g)" -e PYTHONDONTWRITEBYTECODE=1 \
    -v /workspace:/workspace -w "$source_root" codex-rolly-native:trixie-gcc14-v1 \
    python3 -B worklogs/dsp-numerical-audit-0102/relink_verified.py \
    --checkout "$source_root" --juce "$juce_root" --build "$build_root" \
    --out "$output_root/$name" --receipt-commit "$receipt_commit" --receipt-path "$receipt_path" \
    --spec-commit "$spec_commit" --spec-path "$spec_root/$name-spec.json" \
    --diagnostic-checkout "$diagnostic_root" > "$output_root/$name.log" 2>&1
  cleanup
  printf 'completed %s\n' "$name"
done
