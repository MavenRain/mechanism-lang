# mechanism-lang specification

## R0 counts

The block below is inherited from kanon at PIN
(936a43a92dd59a04698648f24fa5ae94cdb532df, pin SPEC.md:176-183) and it
is copied byte for byte.  M0 changes no row of it.  `mech spec-count`
prints this block and dev/r0-count.sh diffs the two.  A count that grows
fails the R0-COUNT gate leg.

```
formers 2: Lan Ran
schema constructors 4: In Elim Sec Out
shapes declared 5: SPi SColl SPar SMu SNu
shapes admitted 3: SPi SColl SMu
named rules declared 3: proof-irrelevance subsingleton-large-elimination literal-fast-path
named rules present 3: proof-irrelevance subsingleton-large-elimination literal-fast-path
eta rows 3: Ran-SPi Lan-SPi Ran-SColl
no eta 3: Lan-SColl Ran-SMu Lan-SMu
```

Every number in the block is the length of the list printed after it.

The block moves at M2 on exactly two rows, shapes admitted 3 to 4 and no
eta 3 to 4, when SPar is admitted for Quot (M0-PLAN.md:108).  Any
earlier growth is an R0-COUNT failure.

## D3: prenex universe levels

Stage A replaces closed integer levels with zero, successor, maximum,
impredicative maximum and global universe variables.  The public Level.t
stays abstract.  Internally, a positive Zarith successor offset represents
repeated successor, so a large closed level does not allocate a unary
chain or wrap at the machine integer limit.  This representation adds no
term former, shape or named kernel rule.

Level equality and inequality are universal over natural valuations.
The decision procedure partitions each free variable into zero or
positive.  On each partition, imax a b is zero when b is zero, and max a b
otherwise.  Normal forms are maxima of constants and variable offsets.
Variable-arm domination and the minimum of the right-hand side decide
inequality; equality checks both directions.  This is a finite exact
procedure, not a numeric sample check.  It implements the plan's semantic
comparison, rather than copying Lean.Level.isEquiv, whose source describes
its structural normal-form comparison as incomplete.

The checker passes its budget through level comparisons in conversion
and the rule packs.  Partition search and normalization poll that budget
and return Budget_exhausted when it expires.  Unlimited standalone level
comparison remains exact and imposes no universe-arity cutoff.

Universe variables bind at a global template only.  Term binders never
increase that arity.  Ordinary kernel contexts have arity zero and refuse
unbound universe variables.  Template checking uses the declared arity
and the universal comparison procedure.  This clarifies the M0 plan:
checking a template is symbolic; an ordinary instance is closed after
explicit substitution.

A scope prepass inspects all raw term fields, including annotations on
introductions that a rule may otherwise ignore.  Internal descendants
share that validation through their context; a fresh public context
starts unchecked.  This avoids repeating the full walk at each node.

The surface Poly catalog retains checked templates and their arities
outside Global.t.  Their bare names cannot resolve as ordinary globals.
Instantiation requires exactly the declared number of closed universe
arguments and a fresh output name.  It substitutes through every term,
shape payload, address, motive and branch, then rechecks the result before
installing an ordinary global.  The existing Term.Global constructor
still holds one string.  Import syntax for references between templates
belongs to Stage B.  Family templates belong to the prelude work;
Stage A requires closed family universes.  The large-elimination rule
also requires universal positivity before treating a level as non-Prop.

## Overlay and trusted-source accounting

The vendor gitlink and its working tree remain at PIN.  Unchanged modules
are copied by Dune into mechanism's build tree and compiled beside the
physical overlays.  Local namespace aliases bind the inherited surface,
backend, driver and test code to mechanism's kernel.  The inherited suites
therefore exercise the overlay, not a separately compiled vendor kernel.

TRUSTED-LINES counts each of the ten trusted logical implementations once:
the active overlay replaces the corresponding pinned implementation.
Additional local kernel implementations and interfaces count once each.
This corrects Stage 0's double-counting of replaced files.  The complete
overlay sources and their differences remain visible in PIN-DELTA.md.
The template catalog is outside the kernel: every specialization is
rechecked, just as inherited surface elaboration is rechecked.

## Lean export import foundation

The source format is lean4export NDJSON 3.1.0.  The first record must be
metadata with that format version.  Names and levels have an implicit
zero entry.  Expressions have no implicit entry.  Every referenced table
entry must already exist, including references in deferred values and
recursor rules.  The reader checks record fields and their JSON types,
enumerations, duplicate keys and duplicate table IDs.  In an inductive
group the reader also checks that each constructor is the one its inductive
type lists at that index, and that a recursor rule whose constructor
belongs to the group has that constructor's field count.  A rule of a
nested inductive names a constructor of an earlier group.  Such a name
stays a reference check only.
Index fields must fit a nonnegative OCaml integer.  Natural literals remain
decimal strings and retain arbitrary precision.  JSON nesting is limited
to 512.  Semantic and lowering traversals limit depth to 1,024; lowering
also limits node visits to 100,000 and supplies a default kernel budget of
100,000 polls.  Exhaustion is an explicit error, never a successful type.

All eight named declaration kinds contribute to the count table: axiom,
definition, theorem, opaque, quotient, inductive type, constructor and
recursor.  The UAT denominator distinguishes 2,477 referenced external
constants from 2,543 declared external constants and 3,202 declarations.
The in-house prefixes are UnifiedAggregation, ArrowCat and CompCatTheory,
matching the frozen corpus census.  A declaration is never omitted because
its type lacks a mapping.

Translation first resolves a shared source-type DAG.  It validates closed
term scope, prenex universe scope and constant universe arity at each
declaration type.  Named parameters retain their per-declaration order.
Applications retain both children; projections retain their type name,
field index and structure; metadata retains its data object.
Structural name identities are canonicalized independently of their
display text.  Numeric components and string components remain distinct.
The artifact includes the raw name table and the source parameter IDs.

Translate.lower_type takes checked globals and an explicit resolver.
The resolver receives the exact universe arguments.  Each returned term
must infer in the supplied globals without local term binders.  Application
domains come from kernel inference, and the final result must check as a
type.  The importer installs no source signature as an unchecked global.
Lean binderInfo is retained separately from quantity.  Lowering uses the
unrestricted quantity and checks in erased type mode; it does not infer
erasure from implicitness.

The CLI starts with the kernel's initial globals and no prelude mappings.
KERNEL_TYPE records successful type checking, not source-to-target parity.
Missing mappings remain DEFERRED.  Projection lowering and String type
expressions are explicit refusals.  The complete kernel-type translation
required by the Stage B plan therefore remains open with the prelude
integration.  diff-parity currently reports counts and these statuses;
Stage D owns NAME_AND_TYPE comparisons and the NEVER ledger.
An import command returns success when it produces the complete report,
even if a row reports KERNEL_ERROR.  That status stays in the published
table.  Grammar or scope errors fail the command before publication.

types.ndjson is a versioned intermediate artifact, not executable input.
The manifest declares that mappings and values have not been checked.
Output is prepared in a sibling temporary directory and published only
after all files have been written.  The output path may end with a path
separator.  The temporary directory comes from the parent directory and
the last component, so it stays a sibling.  Existing output is refused.  A narrow
import/io.ml boundary converts named host file errors into Result errors.
## Stage C prelude foundation

The prelude's data families and eliminators are ordinary checked source.
The AXIOMS gate starts from an empty global environment and rejects every
postulate and primitive entry.  The inherited driver's initial Nat axiom
is therefore outside the checked prelude's dependency environment.
Parameterized constructors use expected family parameters in the surface
elaborator.  The constructor record comes from the family the expected type
names, so a constructor name that two families declare resolves inside the
expected family, as the kernel introduction rule resolves it.  Dependent
field types use those parameters and earlier fields; result indices come
from the constructor declaration and actual fields.
The kernel rechecks the resulting terms.

The first data universe is supported by these source declarations.
MechProofEq relates proofs of a proposition and has a Prop-valued dependent
eliminator.  MechEq relates values of a carrier at Type 0.  The carrier and
left endpoint are erased parameters, the right endpoint is an erased
index, and reflexivity is a nullary constructor.  Its dependent J admits
a Type 0 motive and derives transport; symmetry, transitivity and
congruence are checked source definitions.

An SMu family at Prop may have erased indices from any well-formed
universe.  Type-valued families retain the existing index universe bound.
All index binders remain at quantity zero; constructor fields retain their
family-universe bound.  Large elimination still requires the existing
subsingleton criterion.  The prelude supplies monomorphic equality;
universe-polymorphic equality and general type cast remain outstanding.

Mapping inventory verdicts are NAME_ONLY, UNMAPPED or NEVER.  NAME_ONLY
names a candidate, not checked source-type parity.  The inventory preserves
all referenced external constants, including ratified NEVER rows and names
used only in values.  Stage D must recompute NAME_AND_TYPE from the export
and checked prelude.  No new R0 former, rule or primitive is introduced.
