# Concurrent relink attempts — rejected provenance premise

Two independent mutation relinks were started at the same time against the
same fresh receipt and JUCE checkout. Both verifiers call Git `write-tree` on
the dependency index. The original-clear attempt failed closed with an
`index.lock` collision and produced no successful result. The unsafe-retention
attempt exited with its expected negative-control status and retained a receipt,
but overlapped the other verifier's Git index write, so this result is not
accepted under the no-concurrent-writer premise. The logs and overlapping
receipt are preserved here without relabeling them as valid controls.

The separately executed positive relink passed before these attempts. The two
negative relinks must be rerun sequentially into new output directories. This
was a verification orchestration failure, not an established product defect or
an invalidation of the earlier fresh build; subsequent independent checks must
still verify source, dependency and object identities.
