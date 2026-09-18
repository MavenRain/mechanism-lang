# PORT-UAT U1: shared left Kan extensions

Date: 2026-09-17. Base: bbebbff. This increment connects the universal
property of left Kan extensions to the shared natural transformation
APIs. Stage C and U1 remain open on transformation equations and
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
prelude/cat/heterogeneous-left-kan.mech
prelude/cat/shared-left-kan.mech
```

Then specialize the group:

```text
specialize MechSharedLeftKan (0, 1, 2, 3, 4, 5) as L
```

The six arguments give the object and hom universe levels of the
source J, middle C and target D. Exactly three category families
survive: `L_Base_Source`, `L_Base_Middle` and `L_Base_Target`.
`L_Base_` retains the entire shared natural transformation API,
including `First_`, `Second_`, `Composite_`, `Compose_` and `hcomp`.
`L_Lan_` retains the heterogeneous left Kan API over those families.
No axiom, kernel rule, mapping verdict or frozen denominator changes.

For K from J to C and F from J to D, the type
`L_Lan_LeftKanExtension J C D j c d K F` stores a candidate H from
C to D, its unit, and the universal solver. `L_Lan_lanFunctor` reads
H. `L_unitNat J C D j c d K F extension` reads the unit as a
`L_Base_Composite_NatTrans` from F to H after K.

`L_Lan_lanSolve` accepts a cocone to G after K and returns a
`L_Lan_LanSolution`. `L_descNat` reads its mediator as a
`L_Base_Second_NatTrans` from H to G. The mediator can be used with
the shared identity, vertical composition, whiskering and horizontal
composition operations. `L_Lan_lanFac` and `L_Lan_lanUniq` read its
proofs; `L_Lan_desc_unique` compares any two mediators that factor
the same cocone, pointwise in target equality.

Independent functors can participate through ordinary checked reuse:

```text
specialize MechHeterogeneousFunctor (2, 3, 4, 5) as External
  with (Source := L_Base_Middle, Target := L_Base_Target)
specialize MechHeterogeneousNatTrans (2, 3, 4, 5) as ExternalNat
  with (Base_Source := L_Base_Middle, Base_Target := L_Base_Target)
```

Their types agree directly with the shared Second edge. Separate
nominal category families remain distinct. Neither an equal universe
tuple nor a structural resemblance supplies the required sharing.

## Validation

PRELUDE-SHARED-LEFT-KAN checks the symbolic template, the mixed tuple
above, its reversed tuple, and a concrete runtime client. Its exact
inventory is 944 definitions and 13 checked families. It verifies
parse/print round trips, family certificates, unchanged initial
entries, no new trusted entries, and no declared axioms. Seven
refusals cover cocone endpoints, extension universes, missing
factorization, missing uniqueness, nominal categories, uniqueness
endpoints and incorrect factorization. Each refusal text must be longer
than 64 characters and longer than the head that all seven refusals
share, so a shortened text cannot accept a different refusal.

The runtime client independently specializes three functor APIs and
one transformation API. Its source homs contain an erased type
argument; its target homs are pairs of functions. A forgetful functor
has a universal extension whose solver works for every target functor.
Two distinct cocones supply different mediators. A factorization and
uniqueness witness accept an independently specialized transformation
as the competing mediator. Two mixed-universe contracts quantify both
competing mediators and their factorization proofs.

The six exports check the unit, the candidate's arrow map, two
mediator components, and both components of their vertical
composition. For input n the results are n+1, n+3, n+5, n+13,
n+11 and 2*n+6. The first coordinate of vertical composition changes
from n+11 to n if its order is reversed.

PRELUDE-SHARED-LEFT-KAN-RUNTIME compares all six exports at n=37 and
n=41 on the kernel, Node and Wasmtime. Both inputs share one checked
source program. The harness refuses a repeated export name and, with a
different message, a source that already holds the alternate name of an
export. Each new gate uses the existing CATEGORY tier. A
diagnostic run measured 278.820 seconds to check, erase, evaluate and
emit the twelve exports, exceeding the initial 240-second emit limit.
The runtime harness has a 540-second total budget, a 480-second emit
budget and a 30-second limit per host run, below the 900-second tier.
Existing tiers and gate predicates remain unchanged. The initial
timeout and the successful diagnostic run are retained in the evidence,
along with two subsequent 480-second timeouts before function selection.

The runtime harness now opts into `prelude_runtime --reachable`. The
whole source still checks and erases first. For each export, the helper
retains the transitive closure of global calls and lifted closures,
including captures and every branch. It preserves all type groups and
postulates and refuses duplicate erased function names. The default
helper mode is unchanged. A self-test covers every erased term form,
recursive references, lifted functions, metadata and duplicate refusal.
A closure fixture compares both modes on all three hosts. Only the new
shared left Kan harness enables this option.

`python3 -I dev/shared-left-kan-mutations.py NEW-WORK-DIRECTORY`
copies only the required fixtures to a fresh directory outside the
repository. Five controls break sharing, the unit accessor, cocone
components, vertical order and a refusal fixture. Each must fail for
its intended reason. Baseline and restored runs must pass, and
canonical and copied source hashes must remain unchanged.

Commands, outputs, mutation results and source hashes are retained
in `dev/validation/port-uat-u1-shared-left-kan/`.
