# U1: left Kan cocone postcomposition congruence

`MechLeftKanLaws.postCoconeCongr` preserves supplied equalities in both
arguments of `postCocone`. Given cocones `alpha` and `beta` from `F` to
`G` composed with `K`, and transformations `tau` and `sigma` from `G` to
`I`, its premises are:

- `CoconeEq alpha beta`, evaluated at each source object `x`.
- Equality of `tau` and `sigma` at every middle object `y`.

The result is `CoconeEq (postCocone alpha tau) (postCocone beta sigma)`.
Composition applies the cocone component first, then the transformation
component at `K x`. The proof applies the target category's `eqCongr`
to each argument separately and combines the two equalities with `eqTrans`.
It introduces no axiom and works at six independent universe levels.

`WidePostCoconeCongrContract` pins this conclusion on the actual cocone
records at `(2, 0, 1, 3, 0, 2)`, with independently bound cocones,
transformations and equality premises. The runtime fixture specializes
the API at `(0, 1, 0, 0, 1, 0)` and consumes the proved equations through
`lanCertified`. Forward and reverse composition have different arithmetic
results, each compared with a separate reference export at inputs 37 and 41.
The runtime congruence pairs are reflexive instances, and
`WidePostCoconeCongrContract` carries the general kernel check.

The PRELUDE-LEFT-KAN-LAWS gate checks 1,002 entries, nine families,
20 computations and 18 refusals. Five new refusals cover either missing
equality argument, unequal cocones, unequal transformations, and a proof
from an unrelated equality family. The default runtime suite includes
20 exports. Its existing 480-second emission limit was reached, as in
the preceding slice. The focused `--cocone-congruence` mode passed all
four new exports at two inputs on the kernel, Node and Wasmtime, for
24 comparisons. The direct native kernel check passed in 1,867.876 seconds,
exceeding the manifest's unchanged 1,800-second allowance on this machine.
All runtime watchdogs remain unchanged.

This proves component equality. Equality of whole transformation records
and source-type parity remain open in Stage C and U1.

Validation output and source hashes are preserved in
`dev/validation/port-uat-u1-left-kan-cocone-congruence/`.
