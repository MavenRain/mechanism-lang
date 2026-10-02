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

Eleven candidate signatures match.  All four U1 record signatures match.
Eight candidates remain blocked:

- Eq, Eq.ndrec, Eq.rec, Eq.refl, Eq.symm, Eq.trans and congrArg require
  an explicit symbolic adapter for the existing equality templates.
- Decidable.isFalse references Not, which has no checked pilot mapping.

The NAME_AND_TYPE verdict in the pilot report covers the type signature
and its recorded mappings.  It does not establish the source record's
constructor or recursor representation, accessor laws, whole-record
equality, theorem translation, or runtime parity.  The inventory TSV and
NEVER ledger remain unchanged.  Stage C, U1 and PRELUDE-CHECKED remain
open.

## Commands and controls

Run `make prelude-compatibility-test` to prepare the production CLI,
generate a fresh report and run the adversarial controls.  The gate keeps
its evidence directory and prints its path.  An existing prepared CLI
can be selected with `--mech` on `dev/prelude-compatibility-gates.py`.

The 13 controls cover universe preservation, distinct symbolic instances,
missing mappings, projections, bound-variable escape, rendering limits,
duplicate nodes, dependency propagation and cycles, and binder-name
injection.  Kernel controls check Nat.succ and reject the wrong target
mechZero for Bool.false.

The gate's OK means that the report and controls completed.  Its report
retains blocked rows.  It is a scoped pilot gate, not the M0 exit gate.
Evidence is in `dev/validation/prelude-compatibility-pilot/`.
