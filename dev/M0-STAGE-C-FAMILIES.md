# M0 Stage C: universe-polymorphic family templates

Date: 2026-09-08.  Base: 0563da5, the committed data equality increment.
The user requested continued development and staging of all changes.
This slice extends the programmatic prenex API to individual recursive
families and provides checked prelude templates for equality and sums.
Stage C and PRELUDE-CHECKED remain open.

## Checking and specialization

`Family_poly.declare` checks a family header and its constructors with a
prenex universe scope.  The checker uses its existing parameter, index,
constructor-field and strict-positivity rules.  It returns a unit judgment
from the temporary symbolic environment.  The surface catalog stores the
declarations; ordinary globals do not contain templates.

`Family_poly.instantiate` requires an exact list of closed universe
arguments and a fresh instance name.  It substitutes levels throughout
the raw declarations and renames self references, including nested shape
payloads and eliminator motive metadata.  Constructor names remain local
to their family.  The ordinary closed checker rechecks the specialized
family and constructors before returning the updated environment.
Any failure leaves the caller's immutable globals and catalog unchanged.
As with the existing raw kernel APIs, callers of kernel internals supply
checked environments.  Family_poly is the interface that keeps symbolic
families out of ordinary globals and rechecks every closed instance.

The kernel overlay factors family checking through contexts with an
explicit universe arity.  Ordinary declaration entry points retain arity
zero.  Empty family paths now poll the supplied check budget too.
No index, field, proof-irrelevance, elimination or erasure rule changes.

Templates must have a result sort that is definitely Prop or definitely
positive under every universe substitution.  A result sort that can
switch between Prop and Type is outside this slice.  Mutual templates and
references between templates are also outside its scope.

## Prelude integration

`Mechanism_prelude.Families.catalog` checks two programmatic templates:
MechEq has one carrier Sort level and a Prop result; MechSum has two Type
levels and a result at their maximum.  Neither is a postulate.  The
PRELUDE-POLY gate installs closed instances alongside `prelude/init.mech`,
then checks source clients that use the instances through the existing
surface elaborator.  Casts at two carrier universes and sums with unequal
carrier universes compute through ordinary eliminators.

This extends the existing OCaml template interface.  Textual universe
binders, a universally checked library of polymorphic eliminators and cast,
category targets, and importer resolver integration remain future work.
The monomorphic `.mech` prelude and its NAME_ONLY inventory stay accurate
at their written universes.  No NAME_AND_TYPE claim follows from this API.

## Validation and integration

The FAMILY-POLY gate exercises symbolic checking, scope failures, recursive
renaming, collisions, budgets and closed rechecking.  PRELUDE-POLY checks
both entries and families for hidden trusted dependencies, requires
indexed witnesses of computation, and rejects unequal endpoints and
cross-instance conversion.  Independent review and targeted mutation
controls accompany the inherited gate battery.

The build copy is `/Users/oobi/Documents/gpt6/mechanism-lang`.
Integration verifies canonical HEAD and original file contents before
copying the validated changes to `/Users/oobi/Documents/mechanism-lang`
and staging all changes.  No commit is created.  The trusted-line bounds
remain 3,000/900; their existing failure is reported with the new count.
