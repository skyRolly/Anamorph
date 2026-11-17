# Combined mutation packet with final test headers

The production source in b182 and d5e is byte-identical. This packet selects
the exact d5e test-header bytes, including the warning-only local rename, and
the independently generated b182 engine-only diagnostic trees. Each mutation
uses the verified b182 fresh-build receipt and also runs two unaffected
selectors where applicable. It is source-equivalent production/test execution
under the b182 Git identity, not an exact d5e Git-head build. The separate
fresh d5e replay and hosted matrix supply exact-source validation.
