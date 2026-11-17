# N2-06 wrapped-output oracle: first failed premise

The first receipt-controlled positive attempt used main `230da0e2`, the
N2-06-only candidate engine and specification `cde44ec3`. It observed actual
x4 wrapping, 114 terminal channel samples and zero candidate/twin mismatches,
but **zero** differences between the historical Mix expression and the twin.
Its 16-check probe therefore failed one sensitivity-premise assertion; the
runner rejected the missing passing marker and did not write a complete result
receipt. The partial outputs and log are retained here.

The selected +/-1e8 Drive input also exposes the independent uncorrected
N2-07 Drive cancellation in this N2-06-only source, so it cannot be assumed
that the Mix expression was tested sensitively. This is a test-premise failure,
not evidence that the corrected production Mix path failed. A revised stimulus
and fresh control identities are required.
