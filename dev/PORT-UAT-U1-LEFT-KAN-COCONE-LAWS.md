# U1: left Kan cocone postcomposition laws

`MechLeftKanLaws` now provides `postCoconeId` and `postCoconeVcomp` at
six independent object and morphism universe levels. The existing
`postCocone` operation applies a cocone component first, then the component
of a natural transformation at the image of the source object under `K`.

`postCoconeId` proves that postcomposition by `idNat G` preserves each
cocone component. Its proof is the target category's right identity law.
`postCoconeVcomp` proves that postcomposition by `tau`, then `sigma`,
agrees pointwise with postcomposition by `vcomp tau sigma`. Its proof is
the target category's associativity law.

The conclusion types spell out the component compositions directly.
The independent `WidePostCoconeIdContract` and
`WidePostCoconeVcompContract` fixtures check that these conclusions match
`CoconeEq` on the actual postcomposed transformation records at levels
`(2, 0, 1, 3, 0, 2)`. Runtime fixtures instantiate levels
`(0, 1, 0, 0, 1, 0)` and use the same proofs to certify component applications.

The laws establish component equality. Equality between whole
transformation records and source-type parity remain open. This slice
adds no kernel rule, axiom, vendor pin or mapping inventory change.

The expanded PRELUDE-LEFT-KAN-LAWS gate requires 989 checked entries,
nine equality families, 16 computations and 13 refusals. The three new
refusals cover a reversed composition conclusion, an independent nominal
equality family and a missing transformation argument. Existing refusals
retain their diagnostic oracles.

The runtime gate requires 16 exports on the kernel, Node and Wasmtime
at payloads 37 and 41, for 96 comparisons. Besides identity, the new
exports exercise both orders of noncommuting transformations. Their
independent arithmetic oracles are `103*n + 11`, `105*n + 1126` and
`105*n + 15` for identity, forward composition and reverse composition.

Validation evidence belongs in
`dev/validation/port-uat-u1-left-kan-cocone-laws/`.

The standard native suite passed all 989 entries, both symbolic
contracts, the 16 computations and all 13 refusal cases. The
expanded runtime command exceeded its existing 480-second deadline.
The unchanged base's ten-export runtime test also exceeded that
deadline using the identical native driver. Full runtime acceptance
remains unverified; the watchdog settings are unchanged.
