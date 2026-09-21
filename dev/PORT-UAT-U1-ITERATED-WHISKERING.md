# PORT-UAT U1: iterated whiskering

Date: 2026-09-20. Base: aaeb4bd (horizontal composition laws).

This increment checks the compatibility of precomposition and
postcomposition with composite functors over four categories.
Stage C and U1 remain open on horizontal associativity, equality of whole
transformation records, and source-type parity.

## Contract

Load these sources in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-whiskering.mech
prelude/cat/iterated-whiskering.mech
```

`MechIteratedWhiskering (u, v, w, z, p, q, r, s)` creates four category
families, `Source`, `Middle1`, `Middle2`, and `Target`.
The parameters are their object/hom universe pairs, in that order.
All eight universes are independent.

The four existing whiskering APIs are instantiated as `ABC`, `BCD`,
`ACD`, and `ABD`, sharing the corresponding category families explicitly.
Each exposes its existing `Base_` composable-functor API and its first,
second, and composite natural transformations. No triangle creates an
extra category family.

All laws start with `A B C D a b c d`, where the four object types are
erased and the four category values are ordinary arguments:

| Member | Remaining arguments | Equation at an erased object x |
| --- | --- | --- |
| whiskerRightComp | K L F G alpha | Precompose alpha by L then K, or by the composite of K then L. |
| whiskerLeftComp | F G H K alpha | Postcompose alpha by H then K, or by their composite. |
| whiskerCommute | K F G H alpha | Precompose by K and postcompose by H in either order. |

For `whiskerRightComp`, K maps A to B, L maps B to C, and alpha goes
between F and G from C to D. For `whiskerLeftComp`, alpha goes between
F and G from A to B, H maps B to C, and K maps C to D.
For `whiskerCommute`, K maps A to B, alpha goes between F and G from
B to C, and H maps C to D.

Each conclusion is a `Target` equality between the two component morphisms
in the same `Target_Hom`. The final binder is `(0 x : A)`.
Both component expressions reduce to the same term, so all three proofs
are `categoryRefl`. They neither require function extensionality nor
identify whole natural transformation or functor records.

External categories bind to the four root names. The runtime fixture
binds four distinct categories:

```text
specialize MechCategoryCore (0, 0) as Source
specialize MechCategoryCore (0, 0) as Middle1
specialize MechCategoryCore (0, 0) as Middle2
specialize MechCategoryCore (0, 0) as Target
specialize MechIteratedWhiskering (0, 0, 0, 0, 0, 0, 0, 0) as Run
  with (Source := Source, Middle1 := Middle1, Middle2 := Middle2, Target := Target)
```

Each root has its own equality family. One category can also fill
several roots. Fresh specializations retain four distinct families.

## Validation

The kernel suite checks parse/print stability, exact definition and family
inventories, checked positivity certificates, unchanged initial globals,
and the absence of axioms. It specializes at both
`(0, 1, 2, 3, 4, 5, 6, 7)` and `(7, 6, 5, 4, 3, 2, 1, 0)`.
The runtime specialization binds a distinct external category to each
of the four roots. The result is 902 definitions, 14 families, 18
computations, and six refusals with pinned diagnostic prefixes and
digests of the same 256-character excerpt.

The right-whiskering fixture shifts objects by three and then doubles
them. Its transformation component at x maps n to 2*n+x. Reversing the
two object maps changes the observed result.
The left-whiskering fixture first maps one coordinate of a pair, then
conjugates the map by a coordinate swap. The mixed fixture combines the
object shift with mapping the other coordinate.
Pair observations encode `100*first+second`.

The six exported functions compute both sides of the three laws at
0, 37, and 41. Every export supplies its checked law through an erased
proof argument. Expected results are `4*x+32`, `101*x+1314`, and
`101*x+1317`, respectively. The kernel, Node, and Wasmtime agree on all
18 evaluations of the six exports, including 36 comparisons against
external hosts.

Refusals cover unequal components, reversed precomposition order, an
omitted postcomposition, an omitted precomposition, an unrelated target
family, and incompatible abstract transformation endpoints.

Both new gates use the existing 900-second CATEGORY tier. The runtime
driver retains the established 540-second overall budget, 480-second
kernel/emission budget, and 30-second per-host budget. Existing gates,
budgets, frozen denominators, the compiler, and the vendor pin are unchanged.

`dev/validation/port-uat-u1-iterated-whiskering/` records the commands,
captures, executable hashes, source hashes, and development attempts.
Validation is scoped to this increment; the full battery was not rerun.
The inherited TRUSTED-LINES failure remains
`kernel=5475/3000 encoder=246/900`.
