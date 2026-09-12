# M0 Stage C: checked prelude foundation

Date: 2026-09-07.  Base: 77eda36, Stage B.  The user requested continued
development and staging of all changes.  The M0 plan sections 4, 7, 9,
10 and 11 define the intended prelude.  This increment supplies the
checked data foundation, parameterized constructor elaboration, the
external-name inventory and axiom gates.  It does not close Stage C or M0.

## Deliverables

`prelude/init.mech` supplies data families and eliminators checked from
`Global.empty`.  No inherited Nat axiom or primitive is available to this
check.  Init data is declared through SMu.  The prelude has no postulates.
Parameterized constructors obtain their parameters from their expected
family type, and the kernel checks the result.  The surface change is an
overlay, with the pinned vendor tree unchanged.

The map inventory contains one row per referenced external name in the
frozen UAT export.  The candidates are NAME_ONLY, because a checked target
is not evidence of equality with an imported Lean type.  NEVER contains
only the exact Lean.Omega namespace, covered by R-V4 omega reflection.
The 2,477-name denominator includes those exceptions.  Other unavailable
targets stay UNMAPPED.  Stage D must recompute each type judgment.

## Equality blocker

The foundation encountered the following blocker, resolved for data
equality by the 2026-09-08 increment in `dev/M0-STAGE-C-EQUALITY.md`.
The plan assumes that generic Eq is expressible as an indexed SMu at
Prop without changing the kernel.  At the foundation checkpoint,
`Check.index_rules` required an index's type universe to be at or below
the family's universe.  A data endpoint in
`Eq (A : Type 0) (x : A) : A -> Prop` violated that bound.  Moving it to
a constructor field hits `Check.check_ctor` instead.  The
constructor-field fixture remains a checked negative.  The data-index
fixture was replaced by the checked positive equality declarations.
These are implementation blockers, not extra NEVER exceptions.

The source may provide equality between proofs in a proposition, named
MechProofEq.  That restricted family does not map Lean Eq or implement
data transport.  The equality increment shipped generic Eq, its J and a
data cast.  The family-template increment shipped the programmatic
Family_poly templates and the MechEq and MechSum catalog; see
dev/M0-STAGE-C-FAMILIES.md.  The member-definition increment adds
polymorphic library transport and cast; see dev/M0-STAGE-C-TRANSPORT.md.
The equality-operations increment adds dependent J, symmetry, transitivity
and congruence within one carrier Sort level, plus type-equality symmetry
and transitivity; see dev/M0-STAGE-C-EQUALITY-OPS.md.
The dependent-prelude increment adds polymorphic Pi and Sigma definitions,
pair construction, projections and dependent elimination; see
dev/M0-STAGE-C-DEPENDENT.md.
The congruence increment adds ordered family groups and congruence between
independent carrier universes; see dev/M0-STAGE-C-CONGRUENCE.md.
The textual-prenex increment adds universe binders and closed specialization
for nonrecursive definitions; see dev/M0-STAGE-C-PRENEX.md. The textual
family increment adds single recursive family templates and closed
instances; see dev/M0-STAGE-C-PRENEX-FAMILIES.md.  The textual group
increment adds ordered families and member definitions; see
dev/M0-STAGE-C-PRENEX-GROUPS.md. The category increment adds checked
category records and dependent-record kernel fixes; see
dev/M0-STAGE-C-CATEGORY.md.  The dependent-closure increment adds WASM
parity for generic category accessors; see dev/M0-STAGE-C-CLOSURES.md.
The first U1 increment adds functors, identity and composition within
one category group instance; see dev/PORT-UAT-U1.md.  The next U1
increment adds natural transformations, identity, vertical composition
and whiskering; see dev/PORT-UAT-U1-NATTRANS.md.  Functors across
separate universe pairs, LeftKanExtension and source-type parity
remain open.
A kernel change requires
soundness analysis of proof irrelevance and large elimination, since the
existing criterion admits singleton families with erased fields.

The inherited TRUSTED-LINES failure remains at the unchanged 3,000/900
bounds.  Neither this increment nor its tests move that limit.

## Workflow and validation

1.  Verify the clean committed base and build a separate workspace copy.
2.  Build prelude and inventory with separate ownership.  Independently
    verify the equality blocker while constructor integration proceeds.
3.  Run meaningful positive clients, precise negative fixtures, the empty
    environment axiom audit, and reproducible corpus inventory checks.
4.  Run inherited kernel, levels, surface, WASM, import, pin, R0 and
    denominator gates.  Record the unchanged trusted-line failure.
5.  Review the final diff, fix findings, and record mutation controls.
6.  Apply validated files only if the canonical base and files are still
    unchanged.  Stage all mechanism-lang changes.  Do not commit.

Build copy: `/Users/oobi/Documents/gpt16/mechanism-lang`.
Canonical repository: `/Users/oobi/Documents/mechanism-lang`.
