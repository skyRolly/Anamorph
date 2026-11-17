# H4 fade attribution probe — initial compile failure

The first receipt-controlled main `230da0e2` attempt used spec `138bae76` and
probe `4268750c`. GCC rejected the probe's namespace `access` because POSIX
already declares a global function of that name. No executable or numerical
result was produced. `compiler.log` preserves the diagnostic; the subsequent
revision renames the test-only namespace and requires a fresh output directory.
This was a verification-probe defect, not a product failure.
