# PORT-UAT U1: heterogeneous functors

Date: 2026-09-13. Base: 75b835e. This increment supplies a public
functor template whose source and target have independent object and
morphism universe levels. It consumes checked template composition.
Stage C and U1 remain open.

## Contract

Load `prelude/cat/category-core.mech`, then
`prelude/cat/heterogeneous-functor.mech`, before a client:

```text
specialize MechHeterogeneousFunctor (0, 1, 2, 3) as Mixed
```

The arguments are source object, source morphism, target object and
target morphism levels, in that order. `Mixed_Source_Category` takes
objects in `Type 0` and supplies hom types in `Type 1`.
`Mixed_Target_Category` takes objects in `Type 2` and supplies hom
types in `Type 3`. The two equality families are nominally distinct,
including when their universe arguments coincide.

The group supplies `Functor`, `functorObj`, `functorMap`,
`functorMapId`, `functorMapComp` and `eqCongr`, all prefixed by the
specialization name. A functor takes the two object types and their
category records. Its dependent pair contains an object map, an
arrow map, an identity preservation proof and a composition
preservation proof. Both proofs use the target equality family.
Object maps remain runtime functions. Arrow endpoints and law
arguments are erased. The record lives in the maximum of the four
object and morphism type levels.

`eqCongr` maps source equality through a function into target
equality, even when their universes differ. Its proof eliminates
source equality and constructs target reflexivity. It is a source
proof; the nominal families remain definitionally distinct.

`MechCategoryCore` contains the existing category representation,
seven category operations and the equality helpers `eqTrans` and
`eqCongr`. It excludes the same-pair functor, natural-transformation
and left Kan members, so a composed pair checks only the category
foundation it consumes. Its source mirrors the corresponding prefix
of `MechCategory`, with the family renamed. Existing clients of
`category.mech` retain their API and nominal families.

## Validation

The kernel gate checks source round trips, no symbolic definitions
escaping into globals, positive checked families and no added trusted
entries. It checks three pairs: `(0, 1, 2, 3)`, `(0, 0, 0, 0)` and
`(0, 1, 1, 0)`. Generic signatures exercise both hom universes,
object maps, dependent endpoints, record universes and both laws.

Ten negative fixtures are refused with their intended error variants
and, for the nine type mismatches, their intended messages. They
cover erased object use, missing or invalid laws, incorrect objects
and endpoints, both hom universe mismatches, and the nominal
distinction between equal-level category and equality instances.
Quantity and universe arity refusals pin their messages. An exhausted
budget is also refused. Type mismatches are matched by error variant
and by one discriminating substring for each fixture, to avoid storing
expanded category records in diagnostic snapshots. The suite also
compares the first 70 lines of `category-core.mech`, with the family
renamed, against `category.mech`, so the two copies cannot drift.

The runtime fixture uses objects in `Type 0` and `Type 1`, while hom
types move from `Type 1` to `Type 0`. Its source arrows quantify over
an erased type argument; the functor specializes that argument.
Object, arrow and composed-arrow exports are compared on the kernel,
Node and Wasmtime at payloads 37 and 41. Their answers are respectively
`payload + 2`, `payload + 3`, and `(payload + 3) * 2`. Thus reversing
the order of the two source arrows changes the last answer.

Eight source mutation controls must fail at their named checks, with
pairwise distinct outputs, followed by a passing restored suite. Four
damage universe, category, equality or law signatures. Two retain
checked functor laws but change object or arrow computation,
demonstrating that the value assertions detect those changes. One
changes a sort pin of the generic fixture and one removes the intended
refusal of a negative fixture, which exercise the inventory and the
refusal corpus. The replay copies only its source fixtures and records
their hashes and the test executable hash. It does not rebuild or edit
the working tree. Both new gates use the SLOW (120-second) watchdog;
the first restored replay measured 30.791 seconds at the current load.

Measurements and validation evidence are recorded in
`dev/M0-BUILD-LOG.md` and `dev/validation/port-uat-u1-heterogeneous-functor/`.

## Remaining work

The pair owns its source and target category instances. Two independent
pairs do not share a middle category merely because its levels match.
General heterogeneous identity and composition APIs need shared
category instances. Heterogeneous natural transformations, left Kan
extensions and source-type parity remain due. This increment does
not close U1, Stage C, typed mapping or PRELUDE-CHECKED.

The kernel, encoder and vendor source bytes, trusted-line bounds,
mapping verdicts and corpus denominators are unchanged.
