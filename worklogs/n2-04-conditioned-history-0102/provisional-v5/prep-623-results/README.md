# Fresh source-bound N2-04 preparation replay

The shared fresh-build runner exited 0 for preparation head
`623d3c33b6e381a34d09dd00fff5a59a6b3838ab`, tree
`8460a3d0b16d26e2810fd2b71b81580eefb00734`, spec commit
`d833706fcf8b0feb2e5a02a1723700e6dc319fa2`, GCC 14.2, Linux/Ninja Release and
JUCE `d60b7a78030661574a615b586d9325ba7a7fa7dd`. This head is based on the
integration rehearsal, not actual final main. The receipt binds the checkout,
dependency, build, executable, commands and raw output identities within the
accepted trust model. Full stdout streams are compressed losslessly here;
the receipt's hashes refer to their uncompressed bytes.

All three actual runs exited 0 as expected: N2-04 selector **109/0**, full DSP
**5723/0**, and full State under `xvfb-run -a` **16555/0**. These results cover
the v5 test at this exact preparation head. A later v6 development test adds
repeated-recovery controls and requires separate source identity and execution.
Receipt-controlled positive and negative relinks will be recorded separately;
the full hosted matrix and actual-final-main PR remain outstanding.
