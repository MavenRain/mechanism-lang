# PORT-UAT U1: shared natural transformations

Date: 2026-09-16. Base: a168a09. This increment connects independent
functors, natural transformations and whiskering over three shared
category families. It adds horizontal composition as a checked source
definition. Stage C and U1 remain open on shared left Kan APIs and
source-type parity.

## Contract

Load these files in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-nattrans.mech
prelude/cat/heterogeneous-whiskering.mech
prelude/cat/shared-nattrans.mech
```

Then specialize the group:

```text
specialize MechSharedNatTrans (0, 1, 2, 3, 4, 5) as N
```

The arguments are the object and hom universe levels of Source,
Middle and Target. Exactly three families survive: `N_Source`,
`N_Middle` and `N_Target`. Each retains the category core members,
including `Category`, `Hom`, `id` and `comp`.

`N_First_`, `N_Second_` and `N_Composite_` expose the corresponding
natural transformation API: `NatTrans`, `natApp`, `naturality`,
`idNat` and `vcomp`. Their `Base_` members expose functors and their
accessors. `N_Compose_Base_compFunctor`, `N_Compose_whiskerRight`
and `N_Compose_whiskerLeft` accept these same functors and natural
transformations. The templates reuse the category certificates under
six independent symbolic universe parameters before specialization.

Independent functors can join through ordinary closed family reuse:

```text
specialize MechHeterogeneousFunctor (0, 1, 2, 3) as F
  with (Source := N_Source, Target := N_Middle)
```

`F_Functor C D c d` then works directly as a First functor. A separate
nominal category equality cannot substitute for a shared family.

For categories c, d, e on carriers C, D, E, functors F, G from C to D,
H, I from D to E, alpha from F to G, and beta from H to I:

```text
N_hcomp C D E c d e F G H I alpha beta
```

The result is a Composite transformation from H after F to I after G.
Its component at x is H's map of alpha(x), followed by beta(G(x)).
The implementation vertically composes `whiskerLeft H alpha` and
`whiskerRight beta G`. Their checked naturality proofs establish the
result's naturality. The middle functor is H after G in both terms.
No kernel rule, axiom, mapping verdict or denominator changes.

## Validation

PRELUDE-SHARED-NATTRANS checks the universal template, two mixed
universe instances and a concrete client. Each instance carries a
horizontal composition contract, so the reversed universe tuple of the
second instance has a checked client too. Its exact inventory has
591 definitions and 11 families: three category families per instance,
plus the runtime fixture's Point and Path. Imported definitions remain
ordinary definitions, with no new trusted entries or declared axioms.
The suite checks parse/print round trips and all family certificates.

Six refusal fixtures cover a horizontal endpoint, a vertical middle
functor, a missing naturality proof, nominal equality, an undersized
universe and an erased component object. Each pins its diagnostic.

The runtime client uses independently specialized constant functors.
Its first transformation maps n to 2*n+x. A second functor maps an
arrow onto the first coordinate of a pair, and the second natural
transformation swaps the coordinates. Horizontal composition at
(3, x) therefore returns (x, 6+x). Vertical composition then adds 5,
giving x+11. The identity transformation preserves x. This checks
arrow mapping and composition order with observable components.

PRELUDE-SHARED-NATTRANS-RUNTIME compares all four exports at payloads
37 and 41 on the kernel, Node and Wasmtime. Both payloads share one
checked set of definitions. The alternate exports replace every input
reference, including both references in the identity case. Each new
gate uses the existing SUITE watchdog tier; the runtime harness has a
290-second total budget, under the 300-second tier, so the tier expires
first and every harness expiry prints `timed out after`. The kernel emit
takes at most 240 seconds and each host run at most 30 seconds. Existing
tiers and predicates stay unchanged.

`python3 -I dev/shared-nattrans-mutations.py NEW-WORK-DIRECTORY`
copies only the required source fixtures outside the repository. It
uses the built suite to check six controls: omitted sharing, wrong
horizontal endpoint, reversed horizontal composition, changed
components, changed vertical computation and a vacuous refusal case.
Baseline and restored runs must pass. A control counts only on its
intended diagnostic, and source hashes must remain unchanged. No
compiler rebuild or canonical source mutation is involved.

Results, commands and hashes are retained in
`dev/validation/port-uat-u1-shared-nattrans/`. The full battery keeps
the inherited TRUSTED-LINES bound at kernel=4208/3000 and
encoder=246/900. Shared left Kan APIs, transformation equations and
source-type parity remain separate increments.
