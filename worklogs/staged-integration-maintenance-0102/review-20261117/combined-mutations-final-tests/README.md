# Combined mutation packet with final test headers

The production source in b182 and d5e is byte-identical. This packet selects
the exact d5e test-header bytes, including the warning-only local rename, and
the independently generated b182 engine-only diagnostic trees. Each mutation
uses the verified b182 fresh-build receipt and also runs two unaffected
selectors where applicable. It is source-equivalent production/test execution
under the b182 Git identity, not an exact final Git-head build. The separate
fresh d5e replay and hosted matrix have their own exact-source identities.

The initial settled-Mix mutation found a real cross-test dependency: the
N2-03 production-output oracle explicitly models corrected downstream Mix=1,
so the full selector reports 24 output mismatches even though its independent
Enable kernel, aligned-dry, reference and interior checks remain correct and
the Mix=0.4 production output matches. The original failed runner attempt is
retained. The settled and interior Mix mutations now **expect** the full N2-03
selector to fail at their respective downstream coefficients, and the
separately executed `validate_n203_dependency.py` checks all 48 printed lanes,
the 24 specific failure messages, zero stage mismatches and the unaffected
coefficient. The runner does not call this a passing full selector. The
validator passed on the observed settled output and failed when asked to
classify it as an interior-mutation output. Other selectors still require their
specified positive or negative exits.
