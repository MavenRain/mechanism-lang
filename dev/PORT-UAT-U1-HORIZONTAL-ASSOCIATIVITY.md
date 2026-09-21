# PORT-UAT U1: horizontal associativity

Date: 2026-09-21. Base: 747ab46 (iterated whiskering).

This increment proves horizontal associativity componentwise over four
categories. Stage C and U1 remain open on equality of whole transformation
records and source-type parity.

## Contract

Load these sources in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-nattrans.mech
prelude/cat/heterogeneous-whiskering.mech
prelude/cat/shared-nattrans.mech
prelude/cat/horizontal-associativity.mech
```

`MechHorizontalAssociativity (u, v, w, z, p, q, r, s)` creates the four
category families `Source`, `Middle1`, `Middle2`, and `Target`. Their
object/hom universe pairs are independent. The four existing shared
natural transformation APIs, `ABC`, `BCD`, `ACD`, and `ABD`, reuse the
corresponding roots explicitly, including their nominal equality families.

The law's full argument order is:

```text
hcompAssoc A B C D a b c d F G H I J K alpha beta gamma x
```

The four object types are erased. The four category values, six functors,
three transformations, and final object `(x : A)` are ordinary arguments.
The transformations have directions `alpha : F => G` from A to B,
`beta : H => I` from B to C, and `gamma : J => K` from C to D.
The conclusion uses `Target` equality in the hom from `J(H(F(x)))`
to `K(I(G(x)))`.

Writing `h(alpha, beta)` for the existing `hcomp` argument order,
the equality is:

```text
h(h(alpha, beta), gamma).app x = h(alpha, h(beta, gamma)).app x
```

Both endpoints are actual components of the existing horizontal operation.
The proof maps a composite with J, applies congruence to postcompose by
`gamma.app (I(G(x)))`, and uses target-category associativity. It neither
assumes function extensionality nor identifies whole transformation or
functor records. The result remains a Prop and can be erased by clients.

All four roots support explicit external reuse. Fresh specializations keep
four distinct families; the runtime fixture binds four distinct external
families at universe pairs `(0,1)`, `(0,0)`, `(1,0)`, and `(1,0)`.

## Validation

The kernel suite checks parse/print stability, exact definition and family
inventories, checked positivity certificates, unchanged initial globals,
and the absence of axioms. It specializes at both
`(0,1,2,3,4,5,6,7)` and `(7,6,5,4,3,2,1,0)`.

The runtime fixture's first component sends n to `2*n+x`. The second
transformation swaps coordinates and adds 5 to the new first coordinate.
This visible trace keeps naturality by `categoryRefl`. The last source
functor conjugates endomorphisms by a coordinate swap, and the last
transformation swaps coordinates again. Both associations send `(a,b)` to
`(a+5,2*b+x)`. Applying this to `(x+3,x+7)` and encoding a pair as
`100*first+second` gives the independently specified result `103*x+814`. At
37 the payload is `(40,44)` and the value is 4625.

The definition `assocLawAbstract` pins the statement of the law. Its
declared type states the two-sided equation over abstract parameters with
the same 0-quantity binders as the template. Its body is the instance
`hcompAssoc` on those binders.

Both exported functions consume the checked law through an erased proof
argument. Kernel evaluation and both external hosts check x at 0, 37, and
41. Seven refusals cover unequal results, an omitted middle transformation,
an omitted final transformation, an omitted postcomposition, an unrelated
target equality family, incompatible abstract transformation endpoints, and
a reflexivity control. The unequal-results refusal and the three omission
refusals state the equation at x equal to 37 and the closed payload
`(40,44)` through `pairCode`. The law value 4625 and the wrong value then
print inside the excerpt. The wrong values are 4626 for unequal results,
12540 with no middle transformation, 12545 with no final transformation, and
11749 with no postcomposition. The unrelated-family refusal shows
`SeparateTarget` inside the excerpt. The incompatible-endpoints refusal
applies the instance `hcompAssoc` to abstract parameters whose last
transformation goes between the wrong pair of functors. The reflexivity
control is a gated negative. It states the two-sided equation over abstract
parameters with the body `categoryRefl`, and the kernel rejects it with a
constructor-index mismatch. Refusal prefixes and digests pin the same
256-character diagnostic excerpt. The suite requires pairwise distinct
digests.

The new gates retain the 900-second CATEGORY watchdog and the existing
540/480/30-second runtime driver budgets. No existing gate expectation,
kernel rule, vendor pin, or frozen denominator changes.
`dev/validation/port-uat-u1-horizontal-associativity/` records final results
and development attempts. The full battery is outside this slice's
validation. The inherited trusted-line bound remains 3,000/900.
