# Combinatorial hand cases

Run either JSON input through `mech-cuopt export` and `solve --backend reference`
as described in [the project guide](../../GPU-AUCTION.md). The `.expected.json`
files contain manually derived settlement projections used by the tests.

## Three bidders for eight slices

A offers 12 for all eight slices. B offers 9 for the first four. C offers 7 for
the last four. All requests occupy the same one-hour interval.

| Scenario | Selected offers | Welfare |
| --- | --- | ---: |
| Full auction | B-first, C-last | 16 |
| Remove A | B-first, C-last | 16 |
| Remove B | A-all | 12 |
| Remove C | A-all | 12 |

B pays `12 - 7 = 5`, C pays `12 - 9 = 3`, and A pays zero. Revenue is 8.
The bid of 12 for an indivisible whole bundle is compared here with
the combined value of two compatible partial bundles. For a single indivisible
bundle auction, see [the original second-price example](../gpu-auction/README.md).

## Alternatives over time

All tenants request the same slice. A offers 18 for hours `[0, 2)` or 6 for
`[2, 3)`, with XOR semantics. B offers 8 for `[0, 1)` and C offers 7 for `[1, 2)`.

| Scenario | Selected offers | Welfare |
| --- | --- | ---: |
| Full auction | A-late, B-early, C-middle | 21 |
| Remove A | B-early, C-middle | 15 |
| Remove B | A-early | 18 |
| Remove C | A-early | 18 |

A pays `15 - (21 - 6) = 0`, B pays `18 - (21 - 8) = 5`, and C pays
`18 - (21 - 7) = 4`. Revenue is 9. Removing A removes both alternatives.
The selected intervals meet at their endpoints, so they do not overlap.
