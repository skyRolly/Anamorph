# Provisional H4 replay on intended integration 70bb

The fresh-build runner executed the reviewed consumption-aware diagnostic
patch on source head `70bbdc22acdfd6286d13b1a15cb445367525c5e5`, resulting
source tree `bd96b406db5de5b85d3c72d9db55165a36860bb9`, with specification
commit `66badc5a8b73dbb37528105f3f49d45a8b675eba`. The retained receipt,
patch, result streams, compressed original CSV and build log identify inputs,
commands, executable and artifact hashes. All three declared processes and the
outer runner exited zero. The candidate's full DSP suite reported **5614/0**;
the focused candidate probe reported **48/0**. The independently run
`verify_oracle.py` produced **8/8** at 20 and 50 Hz. The affected-output and
history comparisons reproduce the prior 128/122-sample injected observations.
This candidate remains nonshipping and these are not results at actual
post-merge main.

Trust-boundary qualification: while this run was executing, another worktree
in the same shared Git object database committed and pushed the separate
history-probe packet. The tested checkout, JUCE files and build were not edited,
but the stated no-concurrent-writer premise was not strictly isolated from the
shared repository metadata. Treat this run as provisional; a quiet fresh replay
on actual integrated main is still needed. The output is retained rather than
silently presenting it as fully established provenance.
