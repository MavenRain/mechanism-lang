# PORT-UAT U1: heterogeneous natural transformations

Date: 2026-09-14. Base: d89fe51. This increment adds natural
transformations between functors whose source and target categories
have four independent universe levels. It supplies identity and
vertical composition with checked source proofs. Stage C and U1
remain open.

## Contract

Load these files in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/heterogeneous-nattrans.mech
```

Then specialize the group:

```text
specialize MechHeterogeneousNatTrans (0, 1, 2, 3) as N
```

The arguments are source object, source hom, target object and target
hom levels. The group specializes MechHeterogeneousFunctor once as
Base. Its existing category and functor APIs are available through
`N_Base_Source`, `N_Base_Target`, `N_Base_Functor` and their members.
No copy of the functor definitions is needed.

For `c : N_Base_Source_Category C`, `d : N_Base_Target_Category D`
and `F, G : N_Base_Functor C D c d`, the new API supplies:

- `N_NatTrans C D c d F G`: components and a naturality proof.
- `N_natApp C D c d F G alpha x`: the component at object x.
- `N_naturality C D c d F G alpha x y f`: the naturality square.
- `N_idNat C D c d F`: the identity transformation on F.
- `N_vcomp C D c d F G H alpha beta`: alpha followed by beta.

Components retain their object argument at runtime. Naturality erases
the two endpoints but permits inspection of the source arrow while
constructing the proof. The equality in that proof is always
`N_Base_Target`. Source and target equality remain separate nominal
families, even when all four universe arguments are zero.

At symbolic levels `(u, v, w, z)`, NatTrans inhabits
`Sort (max (succ u) (succ z))`. Its component function determines
this sort. The naturality field is a proposition. Source hom and
target object levels remain independent parameters of the categories
and functors without raising this record's sort.

Identity uses the target category's left and right identity laws.
Vertical composition uses both naturality proofs, target associativity,
congruence, symmetry and transitivity. The helper `targetEqSymm`
eliminates target equality, and `vcompLaw` composes two abstract
naturality squares. Both are checked definitions.

## Validation contract

PRELUDE-HETEROGENEOUS-NATTRANS checks parse/print round trips,
symbolic checking without escaped globals, the definition and family
inventories, completed positive families and unchanged builtins.
It rejects new axioms and primitive entries.

Generic signatures specialize `(0, 1, 2, 3)` and `(4, 3, 2, 1)`.
They pin both hom levels and make each component universe dominate
once. Generic identity, vertical composition and naturality retain
arbitrary category records and functors. An all-zero specialization
tests nominal separation independently of universe differences.

Negative fixtures reject erased component objects, a Nat value in place of the naturality law,
false naturality, an unrelated target record, an incorrect middle
functor, an incorrect result endpoint, source/target category and
equality substitution, and three incorrect universe signatures.
Each refusal must match its structural diagnostic prefix. Universe
arity and exhausted-budget checks complete the refusal suite.

The runtime fixture uses `(0, 1, 1, 0)`. Its source arrows contain an
erased type argument; its target arrows are pairs of ordinary Nat
functions. Components inspect their source object. The five exports
exercise a component, identity, forward composition, reverse
composition and nested composition. At payload n their results are
`n`, `n`, `2 * n`, `n` and `3 * n`. Forward and reverse use the same
component projection and detect an incorrect composition order.

PRELUDE-HETEROGENEOUS-NATTRANS-RUNTIME compares those exports on
the kernel, Node and Wasmtime at payloads 37 and 41. The new gates
use the existing SLOW watchdog. Mutation controls remove identity and
naturality proofs, change composition order and component behavior,
alter a universe signature and replace a negative fixture with a
valid declaration. The restored suite must pass. Actual results and
source hashes are recorded under
`dev/validation/port-uat-u1-heterogeneous-nattrans/`.

## Remaining work

This group shares categories internally. Separate specializations,
including standalone MechHeterogeneousFunctor and composable-functor
instances, retain distinct nominal families. General category-instance
reuse, general identity and repeated functor composition, heterogeneous
whiskering and left Kan extensions, and source-type parity remain due.
This increment does not close U1, Stage C, typed mapping or
PRELUDE-CHECKED.

The kernel, surface checker, encoder and vendor sources are unchanged.
Existing gate bounds, mapping verdicts and corpus denominators retain
their previous values.

## Review fixes, round 1 (2026-09-14)

The two vertical-composition negatives use distinct binder names, so their
recorded refusals diverge at offset 57. Each negative `.err` file holds a
prefix that no other negative prefix matches, and the suite checks that
condition before it checks the refusals. The missing-law prefix names the
expected naturality component type. The runtime fixture feeds the identity
natural transformation a non-constant input, so identityValue is 42 and no
longer equals componentValue. The new test stanza drops the unused
mechanism_prelude library. The replay binds five controls to recorded
diagnostics and replaces the duplicate component-universe control by
source-hom-universe.
