# PORT-UAT U1: identity whiskering and horizontal units

Date: 2026-09-21. Base: 1e4f371 (horizontal associativity).

This increment supplies four componentwise unit laws. Stage C and U1
remain open on equality of whole transformation records and source-type
parity.

## Contract

Load these sources in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-nattrans.mech
prelude/cat/heterogeneous-whiskering.mech
prelude/cat/shared-nattrans.mech
prelude/cat/identity-functor.mech
prelude/cat/nattrans-units.mech
```

`MechNatTransUnits (u, v, w, z)` creates `Source` and `Target` category
families, with independent object/hom universe pairs. `SourceIdentity`
and `TargetIdentity` reuse those families. The shared natural
transformation API `Before` reuses `Source` in both its source and middle
positions. `After` reuses `Target` in its middle and target positions.
Consequently both triangles and identity functors use the same two
nominal equality families. External reuse is supported; reused equality
families retain their external names rather than creating new aliases.

Each law takes `C D c d F G alpha x`, with `alpha : F => G` from C to D.
The two object types are erased. Category values, functors, the
transformation, and the object `(x : C)` are ordinary arguments.
Writing `h(alpha, beta)` for the existing horizontal composition order:

| Law | Component equation |
| --- | --- |
| `whiskerRightIdFunctor` | `(alpha whiskered by id_C).app x = alpha.app x` |
| `whiskerLeftIdFunctor` | `(id_D whiskered by alpha).app x = alpha.app x` |
| `hcompIdFunctorLeft` | `h(idNat(id_C), alpha).app x = alpha.app x` |
| `hcompIdFunctorRight` | `h(alpha, idNat(id_D)).app x = alpha.app x` |

The first two laws reduce by identity computation. The horizontal left
unit uses functor preservation of identity, congruence, and the target
left unit law. The horizontal right unit uses the target right unit law.
Each conclusion is a Prop in the target hom equality family. No kernel
rule, axiom, functor-record equality, or transformation-record equality
is introduced.

## Validation contract

The kernel suite checks parse/print stability, exact definition and family
inventories, checked positivity, unchanged initial globals, and absence of
axioms. Specializations at `(0,1,2,3)` and `(3,2,1,0)` exercise opposite
universe orders. Abstract clients pin both horizontal statements over
arbitrary categories and distinct functor endpoints.

The runtime client reuses two distinct external category families at
`(0,1)` and `(1,0)`. It reuses the existing indexed natural transformation
fixture, whose Beta component at x is the pair of closures `(n+x, x)`.
Each of four unit operations and the reference component is applied to
`x+7` and `x+11`. Encoding the two results as `101*first+second` gives
`203*x+707`, independently specified in the test driver. Each operation
consumes its law through an erased proof argument. The kernel, Node, and
Wasmtime check x at 0, 37, and 41.

Six refusals cover wrong components, a wrong object index, an unrelated
target equality family, reversed transformation endpoints, and reflexivity
substituted for either abstract horizontal proof. The suite pins a
diagnostic prefix and a digest for each refusal, pairwise distinct: the
two reflexivity refusals digest the whole diagnostic, the other four the
first 256 characters. Four abstract clients restate the two whiskering
laws and the two horizontal unit laws at abstract categories. A syntactic
guard pins the head of each runtime unit term and the unit and law names
of each runtime case.

The new gates retain the existing CATEGORY watchdog and runtime budgets.
Validation results and development attempts are recorded under
`dev/validation/port-uat-u1-nattrans-units/`. The inherited trusted-line
bound remains 3,000/900. Existing gate expectations and vendor sources
are unchanged.

Recorded results: the build has zero errors and warnings. The kernel gate
passes with 1,281 definitions, nine checked families, 15 computations,
and six refusals. The runtime gate passes with five exports, three hosts,
three payloads, and 30 external-host comparisons. Pin-delta and frozen
denominators pass. TRUSTED-LINES retains the inherited failure at
`kernel=5475/3000 encoder=246/900`. The full battery was not rerun.
