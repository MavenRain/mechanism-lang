# PORT-UAT U1: heterogeneous whiskering

Date: 2026-09-15. Base: c7e95c3. This increment adds precomposition
and postcomposition of natural transformations across three shared
categories with six independent universe levels. Both naturality
proofs are checked source definitions. Stage C and U1 remain open.

## Contract

Load these files in order:

```text
prelude/cat/category-core.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-whiskering.mech
```

Then specialize the group:

```text
specialize MechHeterogeneousWhiskering (0, 1, 2, 3, 4, 5) as W
```

The arguments are the object and hom levels of Source, Middle and
Target, in that order. The group specializes MechComposableFunctors
once as Base. Its category records, functors, accessors and composition
are available through the `W_Base_` prefix.

The new records are `W_First_NatTrans`, `W_Second_NatTrans` and
`W_Composite_NatTrans`. Each has `natApp` and `naturality` accessors
under the same edge prefix. They mirror the corresponding definitions
in MechHeterogeneousNatTrans with explicit level and name substitution.
Their sorts at `(u, v, w, z, p, q)` are respectively
`Sort (max (succ u) (succ z))`, `Sort (max (succ w) (succ q))`
and `Sort (max (succ u) (succ q))`.

For category records `c`, `d`, `e` on carriers `C`, `D`, `E`:

- `W_whiskerRight C D E c d e F G alpha K` precomposes a
  Second transformation `alpha : F => G` by the First functor K.
  Its component at x is `alpha.1 (K.1 x)`.
- `W_whiskerLeft C D E c d e H F G alpha` postcomposes a
  First transformation `alpha : F => G` by the Second functor H.
  Its component at x is H's arrow map applied to `alpha.1 x`.

Both return a Composite natural transformation between the actual
`W_Base_compFunctor` results. Composite component and naturality
accessors therefore apply directly. All inputs use the same middle
category record. Changing that record changes the required types.

Precomposition uses alpha's naturality at K's mapped arrow.
Postcomposition uses H's composition law twice, alpha's naturality,
target equality symmetry and transitivity, and `Base_Second_eqCongr`
to cross from Middle equality to Target equality. No axiom or kernel
primitive supplies these proofs. Components retain their object at
runtime; naturality erases its endpoints and permits its source arrow
to be inspected during proof construction.

## Validation contract

PRELUDE-HETEROGENEOUS-WHISKERING checks parse/print round trips,
symbolic checking, exact declaration and family inventories, completed
positive families, unchanged builtins, and the absence of new axioms
or primitive entries. The prelude and clients are elaborated together
once because template registrations are local to one input. The exact
inventories also reject escaped symbolic declarations.

The suite compares all three natural transformation definitions and
accessors with their canonical template under explicit renaming.
Level substitution applies only after `succ`, preserving term binders.
Generic fixtures use `(0, 1, 2, 3, 4, 5)` and `(5, 4, 3, 2, 1, 0)`.
They pin all hom levels and both dominant component universes for
each edge, and check both operations with arbitrary category records
and distinct functors. An all-zero instance tests nominal separation.

Fourteen fixtures reject incorrect transformation sorts, hom levels,
erased component objects, a missing naturality proof, false naturality,
category and equality substitution, an unrelated middle record and
incorrect result endpoints for both operations. Their structural
diagnostic prefixes are pairwise distinct. A specialization with five
universe arguments where the group requires six is the fifteenth
refusal, and an exhausted checker budget is the sixteenth. The separate
CLI arity probe repeats the universe refusal through the executable.

The runtime fixture uses `(0, 1, 0, 0, 1, 0)`. Source arrows contain
an erased type argument; middle arrows are indexed Nat functions;
target arrows are pairs of Nat functions. K shifts the object by five.
H maps a middle arrow to a pair that holds that arrow in both
slots.
The four exports return `n + 5`, `n + 7`, `n + 9` and `n + 11`.
PRELUDE-HETEROGENEOUS-WHISKERING-RUNTIME compares them on the kernel,
Node and Wasmtime at payloads 37 and 41 within its 110-second budget.
Both new gates use the existing SLOW watchdog.

Nine mutation controls remove the right naturality proof, the left
naturality proof and a composition proof; change K's object map and
H's arrow map; replace a negative fixture with a valid declaration;
and alter the canonical, Second and Composite mirrors. The replay
requires distinct failures, restored source hashes and a passing
restored suite. Results live under
`dev/validation/port-uat-u1-heterogeneous-whiskering/`.

## Remaining work

Standalone category, functor and natural transformation specializations
retain distinct nominal families. General category-instance reuse,
general identity and repeated functor composition, heterogeneous left
Kan extensions and source-type parity remain due. This increment does
not close U1, Stage C, typed mapping or PRELUDE-CHECKED. The kernel,
surface checker, encoder, vendor pin, mapping verdicts, corpus
denominators and existing gate bounds retain their previous values.
