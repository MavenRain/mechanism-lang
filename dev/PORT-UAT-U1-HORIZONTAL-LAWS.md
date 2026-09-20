# PORT-UAT U1: horizontal composition laws

Date: 2026-09-19. Base: dbaa955 (pointwise whiskering preservation laws).
This increment proves horizontal identity, congruence, naturality exchange
and vertical interchange. Stage C and U1 remain open on iterated
whiskering, horizontal associativity across four categories, equality of
whole transformation records and source-type parity.

## Contract

Load the following sources in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-nattrans.mech
prelude/cat/heterogeneous-whiskering.mech
prelude/cat/shared-nattrans.mech
prelude/cat/nattrans-laws.mech
prelude/cat/whiskering-laws.mech
prelude/cat/horizontal-laws.mech
```

`MechHorizontalLaws (u, v, w, z, p, q)` imports the preceding preservation
API as `W`. The six object and morphism universes remain independent.
A fresh instance creates three category families. All may be reused:

```text
specialize MechSharedNatTrans (0, 1, 2, 3, 4, 5) as N
specialize MechHorizontalLaws (0, 1, 2, 3, 4, 5) as L
  with (W_Ops_Source := N_Source, W_Ops_Middle := N_Middle, W_Ops_Target := N_Target)
```

Write `a ; b` for applying `a` then `b`, and `alpha * beta` for horizontal
composition. Its component is `H(alpha x) ; beta(G x)`.

| Member | Conclusion |
| --- | --- |
| idHcomp | An identity on the first edge gives precomposition of beta. |
| hcompId | An identity on the second edge gives postcomposition of alpha. |
| hcompIdId | Two identities give the identity of the composite functor. |
| hcompCongr | Supplied equalities on both edges give equal composites. |
| hcompExchange | `H(alpha x) ; beta(G x) = beta(F x) ; I(alpha x)`. |
| hcompVcomp | `(alpha ; gamma) * (beta ; delta) = (alpha * beta) ; (gamma * delta)` at x. |

All six start with `C D E c d e`. `idHcomp` then takes `F H I beta`;
`hcompId` takes `F G H alpha`; `hcompIdId` takes `F H`.
`hcompCongr` takes `F G H I alpha alpha2 beta beta2 first second`;
its premises are `W_First_NatTransEq` and `W_Second_NatTransEq`.
`hcompExchange` takes `F G H I alpha beta x`.
`hcompVcomp` takes `F G J H I K alpha gamma beta delta x`, where
`alpha : F -> G`, `gamma : G -> J`, `beta : H -> I` and `delta : I -> K`.

The first four conclusions use the existing `W_Composite_NatTransEq`,
whose object is erased. The last two quantify an ordinary `(x : C)` and
conclude `NatTransEqAt C E c e FH GI lhs rhs x`. Each component proposition
is in Prop; the function over ordinary objects need not be in Prop.
The naturality field can inspect its morphism, so applying it to
`alpha.1 x` or `gamma.1 x` requires the object to be available. These laws
do not claim conversion to the erased-object relation. A refusal test
checks that attempting that conversion reports a quantity error.

`targetMiddleFour` is the auxiliary target-category theorem that
reassociates four morphisms around a supplied middle square. Its six
object and six morphism arguments are erased; the square proof is
explicit. Interchange combines it with the postcomposing functor's
composition law and beta's naturality at gamma.

All definitions are checked source. The slice adds no axiom, primitive,
kernel form, vendor change, function extensionality or mapping verdict.

## Validation

The kernel suite checks parse/print stability, complete definition and
family inventories, unchanged initial globals, family certificates and
an empty axiom inventory. It specializes at `(0, 1, 2, 3, 4, 5)` and
`(5, 4, 3, 2, 1, 0)`, then shares a separate runtime API's categories at
`(0, 1, 0, 0, 1, 0)`. A third kernel-fixture specialization,
`MechCategoryCore (4, 5)` as `SeparateTarget`, gives the unrelated
target category. The nominal-target refusal coerces into it.

Refusals cover unequal components, a proof at only one object, each
missing congruence premise, an unrelated target category, incompatible
vertical endpoints and erasure of the object needed by naturality.

Fourteen exports compute both sides of the six equations at 37 and 41 on
the kernel, Node and Wasmtime. Each accepts its law through an erased
argument. The observer encodes pairs as `100 * first + second` and starts
with `(x + 13, 7)`. Alpha doubles and adds the object index; Gamma adds
five. Beta swaps coordinates; Delta moves the second coordinate to the
first and replaces the second with five. Reversing the first-edge order
or the second-edge order changes the observed interchange result.

Commands, measured results and source hashes are recorded in
`dev/validation/port-uat-u1-horizontal-laws/`. Existing gate budgets and
expectations are retained.

Both new gates use the existing 900-second CATEGORY tier. The initial
300-second kernel probe and 240-second runtime emission probe timed out.
The runtime driver allows 540 seconds overall, 480 seconds for checking,
evaluation and emission, and 30 seconds per host command, matching the
existing shared left Kan runtime limits. These are watchdog ceilings,
not performance claims.
