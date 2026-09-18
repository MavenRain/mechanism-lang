# PORT-UAT U1: pointwise natural transformation laws

Date: 2026-09-18. Base: 59f48f1 (Veil kernel migration).
This increment supplies the vertical transformation equations that
follow the shared transformation and left Kan APIs. Stage C and U1
remain open on whiskering and horizontal equations, equality of whole
transformation records, and source-type parity.

## Contract

Load these sources in order:

```text
prelude/cat/category-core.mech
prelude/cat/heterogeneous-functor.mech
prelude/cat/heterogeneous-nattrans.mech
prelude/cat/nattrans-laws.mech
```

`MechNatTransLaws (u, v, w, z)` has independent source object/hom
and target object/hom universe parameters. A fresh specialization
introduces two category families through its `Ops` dependency.
Both may instead be reused from an existing API:

```text
specialize MechHeterogeneousNatTrans (0, 1, 2, 3) as N
specialize MechNatTransLaws (0, 1, 2, 3) as L
  with (Ops_Base_Source := N_Base_Source, Ops_Base_Target := N_Base_Target)
```

For functors F and G from c on C to d on D,
`L_NatTransEq C D c d F G alpha beta` means that for every erased
object x, the target category's equality family relates alpha(x)
and beta(x). It lives in Prop, including when the source object's
universe exceeds the target's. Its two transformations must have
the same functor endpoints and the same checked category families.

| Member | Conclusion |
| --- | --- |
| eqRefl | alpha equals itself pointwise |
| eqSymm | reverse a pointwise equality |
| eqTrans | compose pointwise equalities through the same transformation |
| idVcomp | identity followed by alpha equals alpha |
| vcompId | alpha followed by identity equals alpha |
| vcompAssoc | (alpha then beta) then gamma equals alpha then (beta then gamma) |
| vcompCongr | pointwise equal pairs have pointwise equal vertical composites |

The equivalence proofs use the target equality family. The identity
and associativity proofs use the target category's laws. Congruence
changes the first component with eqCongr, changes the second component
with eqCongr, then connects them with eqTrans.

All members are ordinary checked source definitions. The relation
compares components; it provides neither equality of transformation
records nor funext. The kernel, axioms, mapping inventory, pin and
frozen denominators are unchanged.

## Validation

PRELUDE-NATTRANS-LAWS checks the universal template and contracts
at (0, 1, 2, 3) and (3, 2, 1, 0). Its runtime client shares the
categories of an independently specialized transformation API at
(0, 1, 1, 0). A separate instance at the same tuple checks the nominal
category refusal. The exact inventory is 234 definitions and ten
families, including the existing fixture's Point and Path. The suite
checks parse/print round trips, family certificates, unchanged initial
globals, and the absence of new axioms or primitives.

Six refusals cover false component equality, a proof at only one
object, nominal categories, wrong functor endpoints, the wrong middle
transformation in transitivity, and a missing congruence premise.
Each requires its recorded diagnostic prefix. The suite refuses a
prefix of 40 characters or less, so a prefix cut back to the generic
sentence cannot pass. The wrong-endpoint prefix holds the functor
endpoints of the equation. The trans-middle fixture records the whole
message, because its first 240 characters do not separate the two
transitivity premises.

The concrete alpha component maps n to x. Beta maps n to n+x.
The identity applications therefore yield x, the two associations
yield 3*x, and the two congruent composites yield 2*x. The six
exported functions accept checked law proofs through an erased
argument. PRELUDE-NATTRANS-LAWS-RUNTIME compares them at 37 and 41
on the kernel, Node and Wasmtime. It uses the existing reachable
declaration selector after checking and erasing the whole fixture.
The harness computes the completion guard and the counts of the gate
row from the function inventory and the payload list.
The 290-second total budget stays within the existing 300-second
SUITE tier; emit and host limits are 240 and 30 seconds.

`python3 -I dev/nattrans-laws-mutations.py NEW-WORK-DIRECTORY` checks
seven source controls using the built suite and an isolated fixture
copy. A control counts only on exit 1 and its expected diagnostic. Each
expected diagnostic holds the first segment that separates the
recorded outputs of the controls, not the generic sentence alone.
Timeouts, crashes and unrelated diagnostics do not count. The baseline
and restored runs must pass, and original and restored hashes must
agree. It does not rebuild or change canonical sources.

Commands, results, limitations and source hashes are retained in
`dev/validation/port-uat-u1-nattrans-laws/`.
