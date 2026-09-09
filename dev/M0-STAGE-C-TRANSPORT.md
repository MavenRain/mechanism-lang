# M0 Stage C: polymorphic library transport and cast

Date: 2026-09-09.  Base: 43ce948, the committed family-template increment.
The user requested continued development and staging of all changes.
This slice adds universally checked definitions to family templates and
uses them for the prelude's polymorphic equality transport and type cast.
Stage C and PRELUDE-CHECKED remain open.

## Family definitions

`Family_poly.declare` accepts an optional `members` list.  Each member must
be a definition with a body.  The family and constructors check first.
Members then check in declaration order with the family's universe arity,
under that symbolic family and the previously checked members.  This uses
the existing kernel entry points.  A member may refer to ordinary input
globals, its own family and earlier members; separate templates remain
unavailable.  Duplicate names, self references and forward references
are refused by the name checks and ordinary definition checker.  A member
named like another template is refused as a collision before its type and
body are mapped, so a name clash never prints the reference diagnostic.

The catalog retains raw declarations, not symbolic global entries.
Specialization substitutes levels throughout the member types and bodies,
including raw shape and motive metadata.  It renames the family and every
member reference.  A member named `cast` in an instance named `CastData`
becomes `CastData_cast`.  `Family_poly.members` reports the member names
in declaration order.  Every specialized definition checks again in the
caller's closed environment before it is installed.  Collisions with
entries, families or catalog names are errors.  Input globals and catalogs
are immutable, so a failure publishes no partial family or member group.

## Prelude operations

`Mechanism_prelude.Equality.catalog Global.empty` checks two templates:

| Template | Universe arguments | Members |
| --- | --- | --- |
| MechEq | Carrier Sort level and motive Sort level | refl, transport |
| MechTypeEq | Type level of both endpoints | refl, cast |

Transport accepts a motive depending on the equality endpoints.  Carrier
and motive universes are independent and may be Prop.  Cast uses a
specialized equality over types and returns a value at the right endpoint.
Both operations compute by the existing singleton equality eliminator.
Neither template uses a postulate or an inherited builtin family.

For example, specializing `MechTypeEq` at `[Level.zero]` as `CastData`
installs a Type 0 equality family, `CastData_refl`, and `CastData_cast`.
The source client can then write:

```text
def castOne : MechNat :=
  CastData_cast MechNat MechNat (CastData_refl MechNat) (mechSucc mechZero)
```

This is a separate programmatic catalog from `Families.catalog`, whose
existing MechEq and MechSum arities stay intact.  The monomorphic `.mech`
prelude and its mapping candidates retain their written universe levels.
No NAME_ONLY row becomes NAME_AND_TYPE.  Textual universe binders, further
polymorphic equality operations, category targets and checked importer
resolution remain future work.  The existing equality soundness proof
obligations and M0 acceptance conditions remain open.

## Validation

FAMILY-MEMBERS checks 19 cases for ordered definitions, renamed references,
scope, collisions, budgets and closed rechecking.  Each budget case measures
the polls of the members-free run, then requires the exact poll count of the
same run with one member, so a dropped poll on the member path fails the
case.  PRELUDE-TRANSPORT checks seven universe combinations, audits all
installed definitions and families, and checks four precise negative
diagnostics and one scope control.  Each negative names its own scope, and
the OK line prints the measured template, instance and negative counts.
Three indexed computation witnesses require transported values and casts to
reduce to the expected value.  Two type annotations are the computation
witnesses of the Type-level cast and transport: they force `castUniverse`
and `transportType` to convert to `MechNat`.  A dependent motive and mixed
Prop/Type combinations are included.

The full gate battery retains the kernel, surface, WASM, import, corpus,
axiom, map, R0, pin and denominator checks.  The only red leg remains
TRUSTED-LINES at kernel 4182/3000 and encoder 246/900.  This slice adds no
kernel lines and changes no bound.

`python3 -I dev/transport-mutations.py NEW_WORK_DIRECTORY` copies the
repository into a new sibling workspace and runs ten isolated controls.
Each mutant must build cleanly before its selected test can count as a
rejection.  The restored source must pass again.  The exact replacements,
diagnostics and captures remain in that workspace.  See
`dev/MUTATION-LOG.md` and `dev/M0-BUILD-LOG.md` for the recorded run.

The build copy is `/Users/oobi/Documents/gpt1/mechanism-m0-c-transport`.
Integration verifies the canonical base and original file hashes before
copying validated changes to `/Users/oobi/Documents/mechanism-lang` and
staging them.  No commit is created.
