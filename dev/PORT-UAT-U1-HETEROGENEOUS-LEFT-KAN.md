# PORT-UAT U1: heterogeneous left Kan extensions

Date: 2026-09-15. Base: 171513f. This increment supplies left Kan
extensions across one shared category triple at six independent
universe levels. Stage C and U1 remain open.

## Contract

Load these files in order:

```text
prelude/cat/category-core.mech
prelude/cat/composable-functors.mech
prelude/cat/heterogeneous-whiskering.mech
prelude/cat/heterogeneous-left-kan.mech
```

Then specialize `MechHeterogeneousLeftKan (u, v, w, z, p, q) as L`.
The levels are the object/hom pairs of J, C and D. The group imports
one MechHeterogeneousWhiskering as Base, so all category records and
functors share `L_Base_Base_` and natural transformations share
`L_Base_`. It adds seventeen definitions and no equality family.

K is a First functor J to C. F is a Composite functor J to D.
H and G are Second functors C to D. LanCocone spells out the
Composite natural transformation from F to compFunctor K G. Its
record expands the composite maps, avoiding repeated checks of an
intermediate composed functor. The generic fixtures check conversion
to the existing Composite transformation type and its accessors.

LanFactor states that eta at x followed by beta at K.obj x equals
alpha at x in target equality. LanSolution pairs a mediator beta
with that factorization and a proof that every other mediator
factoring alpha agrees with beta at each object of C.
LanTail pairs the unit with a solver for every G and cocone alpha.
LeftKanExtension pairs H with its LanTail.

lanFunctor, lanTail and lanUnit read the candidate. lanSolve applies
its solver. lanDesc, lanFac and lanUniq read the resulting solution.
desc_unique applies lanUniq to two mediators and their factorization
proofs, then uses target equality symmetry and transitivity.
Its conclusion is pointwise equality of components; it does not
require function extensionality or equality of entire records.

## Universe levels

The declared sorts are:

- LanCocone: Sort (max (succ u) (succ q)).
- LanFactor: Prop.
- LanSolution: Sort (max (succ w) (succ q)).
- LanTail and LeftKanExtension: Sort at the maximum of succ u,
  succ w, succ z, succ p and succ q.

The source hom level v occurs in input functor and naturality
types. It does not raise these record sorts: K and F are parameters,
and naturality is a proposition. LanTail quantifies over G, so its
sort includes both middle and target functor universe pairs.
recordFirst/recordSecond use the Second functor carrier level;
coconeFirst/coconeSecond use the Composite component level. Their
dependent fields use the larger LanTail level. The checker requires
these exact levels; one upper-bound projection cannot serve both.

## Validation contract

PRELUDE-HETEROGENEOUS-LEFT-KAN checks a parse/print round trip,
symbolic templates, exact entry and family inventories, unchanged
builtins, positive completed families and absence of new axioms or
primitive entries. Three left Kan specializations, two category cores
and the fixtures contribute 294 definitions.
The generic fixtures use (0, 1, 2, 3, 4, 5) and (5, 4, 3, 2, 1, 0).
They pin all three hom levels and all four record sorts, accept
existing Composite transformations as cocones, compare factorization
with whiskerRight, and check desc_unique for arbitrary mediators.
Two lightweight all-zero category cores test nominal separation.
They avoid instantiating a fourth unused universal-property API.

Thirteen negative fixtures reject wrong levels, wrong endpoints,
missing factorization or uniqueness proofs, a factorization proof
for another cocone, and category substitution. Their diagnostic
prefixes are distinct. The suite also rejects five universe
arguments where six are required and an exhausted checker budget.
Trace and failure messages are bounded; diagnostic comparison uses
the full checker message.

The runtime fixture uses (0, 1, 0, 0, 1, 0). Its uniqueWitness
instantiates the uniqueness rule with one mediator twice, so that
witness pins well-typedness of the instantiation only; a comment
above it names MixedUniqueContract and WideUniqueContract as the
pins of the two-mediator statement. Source arrows accept an
erased type before returning an indexed Nat function. Middle arrows
are indexed Nat functions; target arrows are pairs of Nat functions.
Forget evaluates a source arrow at Nat and preserves its object.
Every middle arrow lifts to a constant polymorphic source arrow,
whose Forget image is the original arrow by computation.
forgetLan constructs an extension for every target category D
and Second functor H, keeping the target abstract in its proof.
Its solver uses the supplied cocone, with naturality obtained from
that arrow lift. Both universal-property proofs check.

Four exports read the unit, the extension's nonconstant arrow map,
and mediators for two different cocones. Their results are n + 1,
n + 3, n + 5 and n + 13. The runtime gate compares payloads 37 and
41 on the kernel, Node and Wasmtime. The kernel gate uses the
existing 300-second SUITE watchdog, as the original left Kan suite
does. Its measured standalone run takes about 96 seconds, and the
120-second tier expired in the full battery. The runtime gate uses
the 120-second SLOW watchdog and a 110-second internal budget.
The kernel recheck records the result under its final SUITE tier.

Nine mutation controls break the uniqueness proof, factor and unit
projections, solution sort, runtime arrow map, mediator component,
cocone selection, negative corpus and the Wide solution-sort pin of
the generic fixture. A replay requires all controls
to fail, distinct diagnostics, restored hashes and a passing restored
suite. Evidence lives under
`dev/validation/port-uat-u1-heterogeneous-left-kan/`.

## Remaining work

Separate specializations keep distinct nominal families. General
instance reuse, identity and repeated composition APIs, source-type
parity, typed mapping and PRELUDE-CHECKED remain open. This increment
adds source definitions without changing the kernel, surface checker,
WASM backend, vendor pin, mapping verdicts or corpus denominators.
