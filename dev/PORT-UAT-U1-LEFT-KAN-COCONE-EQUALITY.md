# U1: left Kan cocone equality operations

`MechLeftKanLaws` supplies `coconeEqRefl`, `coconeEqSymm` and
`coconeEqTrans` for `CoconeEq`. Reflexivity compares a cocone with itself.
Symmetry reverses a supplied component equality. Transitivity chains two
supplied equalities whose middle cocone agrees. The proofs use the target
category's checked equality operations and introduce no axiom.

The group retains six independent universe levels. The three
`WideCoconeEq*Contract` declarations specialize it at `(2, 0, 1, 3, 0, 2)`.
Their conclusions explicitly compare the component projections of separately
bound cocone records, so the contracts check reversal and chaining against
the actual components.

The runtime fixture specializes the API at `(0, 1, 0, 0, 1, 0)`.
Reflexivity uses the alternative cocone. Symmetry reverses the identity
postcomposition equality, and transitivity chains two identity
postcompositions. `lanCertified` consumes each equation, and separate
reference exports provide the expected components. The arithmetic oracles
at inputs 37 and 41 distinguish the alternative cocone from the original.

`PRELUDE-LEFT-KAN-COCONE-EQUALITY` checks all three symbolic contracts,
six computations and five refusals: reflexivity between unequal cocones,
symmetry without a premise, an unrelated equality family, transitivity
without its second premise, and a mismatched middle cocone.
`PRELUDE-LEFT-KAN-COCONE-EQUALITY-RUNTIME` compares the six exports
on the kernel, Node and Wasmtime at both inputs, for 36 comparisons.
The full left Kan laws suite includes the same cases and, with the cocone
action methods, expects 1,033 entries, nine families, 26 computations and
23 refusals.

The focused extractor reads only the harness's declared template inventory.
It preserves retained definitions verbatim, records source hashes and can
include the symbolic contracts. The kernel gate regenerates the focused
fixture before checking it and isolates the five refusals in a temporary
fixture so the exact directory inventory check applies. The driver can print
candidate refusal oracles
with `--record-cocone-equality-oracles`; validation uses the checked-in
oracles and never regenerates them.

Equality of whole transformation records and source-type parity remain open
in Stage C and U1. Validation evidence is recorded under
`dev/validation/port-uat-u1-left-kan-cocone-equality/`.
