# PORT-UAT U1: pointwise whiskering preservation laws

Date: 2026-09-18. Base: c7b99fe (pointwise natural transformation laws).
This increment proves that both whiskering operations preserve identity
transformations, vertical composition and pointwise equality. Stage C
and U1 remain open on iterated whiskering, horizontal equations, equality
of whole transformation records and source-type parity.

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
```

`MechWhiskeringLaws (u, v, w, z, p, q)` keeps all six object and
morphism universe parameters independent. `Ops` imports the shared
three-category transformation API. `First`, `Second` and `Composite`
import the preceding pointwise law API at the corresponding edges and
reuse the categories from `Ops`. A fresh instance has three families.
An existing shared API can supply all three:

```text
specialize MechSharedNatTrans (0, 1, 2, 3, 4, 5) as N
specialize MechWhiskeringLaws (0, 1, 2, 3, 4, 5) as L
  with (Ops_Source := N_Source, Ops_Middle := N_Middle, Ops_Target := N_Target)
```

Every new conclusion uses `L_Composite_NatTransEq`. Its objects are
erased, and it lives in Prop even when the source object universe
exceeds the target hom universe.

| Member | Pointwise conclusion |
| --- | --- |
| whiskerRightId | Precomposition of an identity transformation is identity. |
| whiskerRightVcomp | Precomposition preserves ordered vertical composition. |
| whiskerRightCongr | Precomposition preserves a supplied second-edge equality. |
| whiskerLeftId | Postcomposition of an identity transformation is identity. |
| whiskerLeftVcomp | Postcomposition preserves ordered vertical composition. |
| whiskerLeftCongr | Postcomposition preserves a supplied first-edge equality. |

All six begin with `C D E c d e`. Right whiskering next takes its
precomposing functor `K`; left whiskering next takes its postcomposing
functor `H`. Identity takes `F`. Composition takes `F G J alpha beta`,
with `alpha : F -> G` and `beta : G -> J`. Congruence takes
`F G alpha beta proof`, with both transformations from `F` to `G`.

Right identity and composition reduce to reflexivity; right congruence
evaluates the supplied equality at `K.obj x`. Left identity and
composition use the postcomposing functor's checked law fields. Left
congruence crosses from middle equality to target equality using the
existing heterogeneous equality congruence operation.

The laws are checked source definitions. They add no axiom, primitive,
kernel form, vendor change or mapping verdict. Whole-record equality
and function extensionality are outside this contract.

## Validation

The kernel suite checks parse/print stability, complete entry and
family inventories, unchanged initial globals, checked family
certificates and an empty axiom inventory. It specializes the template
at `(0, 1, 2, 3, 4, 5)` and `(5, 4, 3, 2, 1, 0)` and shares the concrete
runtime API's three categories at `(0, 1, 0, 0, 1, 0)`.

Six refusal fixtures cover unequal components, a proof at only one
object, each missing congruence premise, an unrelated target category
and incompatible transformation endpoints. Each pins a diagnostic
prefix longer than the generic mismatch sentence.

Twelve exports compute both sides of the six equations. All accept
their law proof through an erased argument. The runtime driver compares
each export on the kernel, Node and Wasmtime at 37 and 41. Component
pairs are encoded as `100 * first + second` to observe both coordinates. The probe carrier is `x + 13`, so the
object index and the first carrier coordinate differ, a review-round-1
fix that stayed undocumented. The precomposition half whiskers along
`Embed`, whose morphism map reads the `Nat -> Nat` part of an IndexedEnd
morphism, and every export first maps the doubling endomorphism through
its source functor, so the precomposing functor changes the observed
value.
For left composition, doubling and adding an object precedes adding
five; reversing the order changes the result. For right composition,
a swap precedes a map that replaces the second coordinate by five;
the reverse order changes which input coordinate survives.

Commands, measured results, mutation controls and source hashes live
in `dev/validation/port-uat-u1-whiskering-laws/`.
