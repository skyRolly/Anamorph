# Preserved rejected shipping-search attempt

The first attempted unpatched `70bbdc22` replay exited 1 before configuring or
building because the verifier detected ignored inputs in the reused checkout.
The observed ignored paths were `scripts/__pycache__/` and
`worklogs/dsp-numerical-audit-0102/__pycache__/`. The guard correctly rejected
the checkout; no executable or result receipt was produced. The captured error
and selected specification are retained. A fresh checkout is used for the
separately identified retry; this failure is not a product-test result.
