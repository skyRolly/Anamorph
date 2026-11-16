# Preliminary diagnostic syntax checks

The nonshipping fade-collateral probe initially named a namespace `access`,
which collided with the POSIX function and caused the first syntax-only
compile to fail. The second local syntax-only compile renamed the namespace
and succeeded, with the retained exact-float-comparison warnings. These are
probe-development checks, not a source-bound replay or an H4 result. The
committed experiment spec is the separate fresh-build control.
