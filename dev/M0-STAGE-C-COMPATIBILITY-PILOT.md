# Stage C: typed compatibility pilot

Date: 2026-10-02.  Base: 18477c9.  The user requested the pilot for the
19 existing targets and the U1 record signatures, with all changes staged.

## Contract

The pilot imports the frozen UAT export through the production Bend CLI.
Its digest must equal the unchanged denominator.  The imported graph must
contain 3,202 declarations.  The map must contain the 19 existing
NAME_ONLY candidates.

Each witness has the translated source type as its annotation and the
candidate target as its body.  The renderer preserves the importer's
QMany binders, bound-variable scope and universe arguments.  Constructors
are saturated through checked adapters.  Parameterized constructor
arguments come from the expected family type.  The family parameters
are the leading constructor binders that the result type passes to the
family in order.  Index arguments stay in the adapter body.

Category, Functor, NatTrans and LeftKanExtension are checked inside
symbolic groups at their respective two, four, four and six universe
parameters.  Their dependencies use the matching symbolic category and
functor instances.  This is a universal signature check, rather than a
finite set of closed universe probes.

A matched signature is promoted only after its candidate dependencies
also match.  Unproved dependencies and dependency cycles remain blocked.
The report records source, map, fixture and checker hashes.  It checks
that the export, checker and consumed inputs stay unchanged during use.

## Results and boundary

All 19 candidate signatures now match, using explicit symbolic source
adapters for Eq, Eq.refl, Eq.ndrec, Eq.rec, Eq.symm, Eq.trans and congrArg.
The report retains the inventory target in `target` and records the checked
callable in `adapter`. It does not certify the original monomorphic equality
targets at arbitrary universes. `Not` is a checked support mapping, reported
separately from the 19 inventory candidates, and its success discharges the
dependency of Decidable.isFalse.

`prelude/compatibility/equality.mech` supplies a universally checked equality
family with reflexivity, symmetry and transitivity, a group with independent
motive and carrier Sort levels for nondependent and dependent elimination,
and a group with independent domain and codomain Sort levels for congruence.
The elimination group follows the export's motive-first universe order.
Binders inside each motive type keep the importer's QMany quantity. The
adapters erase their top-level binders, including the motive binder, where
the source has QMany. The witness eta-expands the adapter, so a match accepts
this erasure; the bare adapter does not have the source type. Source clients
can specialize these templates at Prop and at data universes.
`prelude/compatibility/not.mech` defines negation as `P -> MechFalse`.

The preceding pilot matched 11 candidates and all four U1 record signatures.
Its evidence remains in `dev/validation/prelude-compatibility-pilot/`.
The adapter increment builds on c4793ae; its fresh report and controls are
recorded in `dev/validation/prelude-compatibility-adapters/`. In the adapter
report, all four U1 record signatures match. The unchanged
CompCatTheory.LeftKanExtension fixture checks in about 64 seconds at load 12.
Earlier runs under heavier load reached the existing 180-second check limit;
their reports are retained under `attempts/`.

The NAME_AND_TYPE verdict in the pilot report covers the type signature
and its recorded mappings or adapters. It does not establish constructor
or recursor representation, accessor laws, whole-record equality, theorem
translation or runtime parity. The inventory TSV and NEVER ledger remain
unchanged. Stage C, U1 and PRELUDE-CHECKED remain open.

## Commands and controls

Run `make prelude-compatibility-test` to prepare the production CLI,
generate a fresh report and run the adversarial controls. The gate keeps
its evidence directory and prints its path. An existing prepared CLI
can be selected with `--mech` on `dev/prelude-compatibility-gates.py`.

The 19 controls include the preceding 13 controls for universe preservation,
distinct symbolic instances, missing mappings, projections, bound-variable
escape, rendering limits, duplicate nodes, dependency propagation and cycles,
binder-name injection, Nat.succ and a wrong Bool.false target. Added controls
require all 19 candidates and Not to match, reject swapped eliminator and
congruence universes, reject the wrong equality helper, require the Not
mapping, and check closed proof, data and type instances without postulates.

The gate's OK means the report and controls completed. The report retains
blocked record rows and their reasons. It is a scoped signature gate,
and PRELUDE-CHECKED remains the M0 exit gate.

## Category record adapter (2026-10-02)

The increment on d6ac149 adds `MechSignatureCategory` in
`prelude/compatibility/category.mech`. The exported constructor's stored
functions retain relevant object binders. `MechCategoryCore.Category` uses
erased object binders inside those functions, so the source constructor's nested
field type does not match it. No report checks the core accessors against the
source accessors. The explicit adapter keeps the source quantities of the nested
object binders and the source field order, with `comp_id` before `id_comp`. Its
outer `Obj` parameter stays erased, so each signature witness matches through
eta expansion at that binder.

The separate `PRELUDE-CATEGORY-COMPATIBILITY` gate imports the same frozen
3,202-declaration graph. It checks Category, Category.mk, Hom, id, comp,
comp_id, id_comp and assoc at independent symbolic object and morphism
universes. The report identifies the actual adapter for each source member.
Its Eq support row checks the imported equality signature at the successor
of the morphism universe, using the same nominal equality family as the
stored law fields. Dependency discharge requires that contextual support.

Generic reflexivity witnesses check that constructing a record and projecting
Hom, identity and composition recovers the supplied fields. The
`storedFieldOrder` witness projects the fourth stored component at the `comp_id`
law type. These witnesses are checked symbolically and at (0, 0), (1, 0) and (0,
1). `mech axioms` reports no axioms, and the empty-environment `prelude.exe
--audit` reports `PRELUDE-AXIOMS OK`. The eight controls require every signature
and the projection audit to pass. They reject a consistent stored-field reorder
through the stored-field order witness, a consistent nested-binder erasure
through the imported `Category.mk` signature, and swapped universe levels
against the imported Hom signature. The reordered and erased adapters check on
their own before they are rejected, and a twin at the original levels matches
the Hom signature. The controls also block missing equality support and
distinguish type signatures from computation. The last two controls introduce
correctly typed accessors that compose with extra identities. Their signatures
pass and the fixture checks with each helper added. Their projection computation
witnesses fail with kernel mismatches.

Run `make prelude-category-compatibility-test`, or reuse prepared production and
test builds with `python3 -P dev/prelude-category-compatibility.py --mech
_bend2/bin/mech.exe --out /absolute/path/to/new-evidence`. The report records
the export, imported graph, checker, input and fixture hashes. It marks
`gate_passed` only after all eight controls and the final immutability checks
pass. Evidence is retained in `dev/validation/prelude-category-compatibility/`.

These checks cover the adapter's constructor type and three data projection
reductions, plus the other field signatures. They do not establish Category
recursor representation, source theorem bodies, whole-record equality,
runtime parity or conversion to the existing erased core record. The prior
19-candidate pilot and the mapping and NEVER ledgers remain unchanged.
Stage C, U1 and PRELUDE-CHECKED remain open.
