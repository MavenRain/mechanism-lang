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
subsingleton criterion.  The source prelude supplies monomorphic equality;
the separate programmatic catalog supplies a polymorphic equality family.
That catalog also supplies a universally checked polymorphic library
transport and type cast, recorded in dev/M0-STAGE-C-TRANSPORT.md.  Dependent
elimination and equality composition are recorded in
dev/M0-STAGE-C-EQUALITY-OPS.md.  Textual family templates, ordered groups
and members are supported below.  Checked source-type parity remains open.

Mapping inventory verdicts are NAME_ONLY, UNMAPPED or NEVER.  NAME_ONLY
names a candidate, not checked source-type parity.  The inventory preserves
all referenced external constants, including ratified NEVER rows and names
used only in values.  Stage D must recompute NAME_AND_TYPE from the export
and checked prelude.  No new R0 former, rule or primitive is introduced.

## Stage C family templates

Family_poly stores universally checked family declarations outside
Global.t.  Its kernel judgment checks the header and constructors under a
prenex level scope and discards the temporary environment.  Ordinary family
declaration and constructor entry points keep their closed level scope.
The index, constructor-field and positivity rules are shared with those
entry points, including Prop's erased data-index exception.

A template result universe must be definitely zero or definitely positive.
Templates whose result can alternate between Prop and Type, mutual
templates and references between templates are not supported yet.
Instantiation requires exactly the declared number of closed levels and a
fresh name.  Every raw level occurrence and self-family reference is
substituted, including eliminator motive metadata.  Closed instances are
rechecked in the supplied globals, and a failed check publishes no entry.
All new traversal paths and empty-family checks honor the supplied budget.

The programmatic prelude catalog provides MechEq at an arbitrary carrier
Sort and MechSum at two Type levels.  The result sorts are Prop and the
maximum carrier level respectively.  These are kernel-checked declarations,
without postulates.  This catalog supports checked source clients through
closed instances; it does not extend textual syntax or certify any mapping
row's source-type parity.

The separate Equality catalog includes MechEq members `refl`, `transport`,
`j`, `symm`, `trans` and `congr`.  The carrier and motive Sort levels are
independent.  The `j` motive binds the right endpoint and equality proof
at quantity zero.  Congruence's domain and codomain share the carrier
Sort level; it does not instantiate another equality template.  MechTypeEq
includes `refl`, `cast`, `symm` and `trans` at one Type level.  All these
members are ordinary checked definitions using the existing elimination
rule.  Their templates and specialized instances add no trusted entries.

## Stage C dependent function and pair catalog

Dependent.catalog supplies six universally checked Poly definitions.
MechPi takes domain and codomain Sort levels u and v, including Prop,
and returns Sort (imax u v).  MechSigma takes two Type levels u and v
and returns Type (max u v).  Its fiber may depend on the first component.
mechSigmaMk constructs a pair, mechSigmaFst returns its first component,
and mechSigmaSnd returns a value of the fiber at that first component.
These three operations take the same two Type levels as MechSigma.

mechSigmaRec adds a third parameter, the motive's Sort level.  Its motive
depends on the whole pair, and may return either proofs or data.  On a
constructed pair it applies the supplied step to both components.
All carrier and fiber type arguments are erased; pair components and
the step are unrestricted.  The declarations use the existing Ran and
Lan at SPi, with Sec, In and Elim.  They add no kernel rule or primitive.
Closed instances are transparent ordinary definitions rechecked by Poly;
the templates do not reference one another or the monomorphic prelude.
Textual definition templates are supported as described below.  Imported
source-type parity remains open.

## Stage C ordered family groups and congruence

Family_poly.declare_group checks a nonempty list of families under one
prenex universe scope.  A family may refer to earlier families in that
group.  Forward references and mutual recursion between families remain
unsupported.  All members check in order after all families.  The first
family names the template; other families and members remain internal.
The temporary symbolic globals never enter the caller's environment.

Specialization renames the first family to the requested instance name N.
Each companion family F becomes N_F, and each member m becomes N_m.
Level substitution and renaming cover every raw term field, including
constructor types and elimination motives.  Every family, constructor
and member checks again in the supplied closed globals.  Collisions and
failed checks return an error without publishing any part of the group.

Congruence.catalog provides a two-Sort-level template named MechCongr.
At [u; v], instance N is equality over Sort u, N_Result is equality over
Sort v, and N_congr has the following signature (schematic Sort notation):

```text
(0 A : Sort u) -> (0 B : Sort v) -> (0 f : A -> B) ->
(0 x : A) -> (0 y : A) -> N A x y -> N_Result B (f x) (f y)
```

Both equalities live in Prop.  The definition eliminates the domain proof
and returns codomain reflexivity in the constructor branch.  Independent
Sort levels include Prop in either position.  These nominal equality
families are separate from Equality.catalog and its same-level congruence.
No coercion between equality instances is supplied.  The extension changes
no kernel rule, source syntax, vendor file or mapping verdict.

## Stage C textual prenex definitions

`poly (u, v) def NAME : TYPE := BODY` binds a nonempty list of distinct
universe names for one nonrecursive definition.  `Sort LEVEL` uses raw
Sort levels, with zero denoting Prop and one denoting Type 0.  Level
expressions are nonnegative numeric atoms, bound names, `(succ LEVEL)`,
`(max LEVEL LEVEL)`, `(imax LEVEL LEVEL)` or parenthesized levels.
Numeric atoms retain the inherited eighteen-digit and machine-int bounds.
Universe names have a separate scope from term names.  The printer uses
canonical binder names u0, u1 and so on and preserves level-expression
structure.  Sort, poly and specialize are reserved words.

The source elaborator uses the existing Poly API to check each template
universally.  Templates last for one program check, stay outside globals
and output rows, and cannot be used as bare terms.  References between
templates remain unsupported.  Ordinary declarations cannot reuse their
names within that check.

`specialize NAME (LEVEL, LEVEL) as FRESH` supplies exactly the template's
arity of closed levels.  A compound level keeps its own parentheses
inside that list, as in `specialize Box ((max 0 1)) as AgainBox`, because
the outer parentheses delimit the argument list.  The type and body are
substituted and checked again before the resulting definition joins
globals and output rows in source order.  The specialization shares the
caller's check budget and
rejects every occupied global, family, template or constructor name.  A
template name is refused on the same four kinds.  A plain `def` keeps the
inherited kernel allowance and can repeat a constructor name of an
earlier family.  An error returns no partial environment.  Check, axioms, emit and run consume the resulting
ordinary definitions.

This definition syntax does not add polymorphic axioms, recursive
definitions, implicit specialization, universe inference, a kernel
rule or a mapping verdict.  PRENEX and PRENEX-RUNTIME test it; the grammar
and validation scope are recorded in dev/M0-STAGE-C-PRENEX.md.

## Stage C textual polymorphic families

`poly (u, v) mu NAME ...` binds named Sort levels for one recursive
family's parameter telescope, indices, result universe and constructors.
The family body uses the ordinary mu grammar.  Ordered groups and member
definitions extend this syntax as specified below.  References to other
templates require an earlier explicit specialization.
The result universe must remain Prop or remain Type under every universe
assignment, as required by the existing Family_poly API.

The elaborator builds constructor self references in a temporary symbolic
environment. Family_poly.declare universally checks the family and stores
it in the source catalog. It enters neither global table nor output rows.
`specialize NAME (LEVELS) as FRESH` dispatches by template kind and rechecks
the closed family and constructors before installing it in the family
table. Constructor labels retain their spelling; their expected family
instance resolves them.  A family with no members adds no definition or
axiom row.

Definition and family template names share a reserved namespace for one
source check.  A later declaration cannot reuse a template name or an
instance name.  A mu group's own constructor labels join that check,
because the group installs them at once.  A family template's constructor
labels stay outside both catalogs until an instance installs them, so the
verdict does not depend on the declaration order.  A specialized family's
constructor labels are then checked against both template catalogs and
against the instance name before the resulting globals can escape.  The
labels also join the reservation of the caller globals:  a label cannot
take the name of an earlier instance, of an earlier generated member or
of another definition.  A name that the globals already hold as a
constructor label stays available, because one template installs its
labels again at every instance.  A constructor label equal to its own
family name is refused at the template.
Ordinary constructor/global name sharing retains the inherited kernel
behavior.
All stages share the caller's budget and return no environment on failure.

PRENEX-FAMILIES checks syntax round-trips, installed inventories, isolation,
normalization and precise refusals. PRENEX-FAMILIES-RUNTIME compares
specialized-family payloads and recursive lists on all three hosts.
See dev/M0-STAGE-C-PRENEX-FAMILIES.md. This adds no kernel rule, mapping
verdict, universe inference or implicit specialization.

## Stage C textual ordered groups and members

The source grammar extends the family template as follows:

```text
poly-family ::= 'poly' '(' universes ')' 'mu' family
                ('and' family)* ('where' member+ 'end')?
member      ::= 'def' NAME ':' term ':=' term
```

`where` is reserved.  Its definitions use the same universe binders as
the families.  The block requires at least one definition and a closing
`end`.  A member cannot be an axiom, recursive definition, nested template
or specialization.  An explicit `poly (...) mutual ... end` is refused.

Families check in order.  Each family can use itself and preceding
families, but cannot use a later family.  After all families check, members
check in order under that symbolic environment.  A member can use all
group families and earlier members.  Members cannot refer to themselves
or later members.  `Family_poly.declare_group` checks the complete schema
universally before it enters the catalog.  Only the first family names
the catalog entry.  All symbolic globals stay outside the caller's
environment and output rows.

`specialize FIRST (LEVELS) as FRESH` installs the first family as `FRESH`,
each companion as `FRESH_NAME`, and each member as `FRESH_NAME`.  Family
and member references are renamed together.  The closed instance checks
again before any environment is returned.  Member definitions join the
ordinary output rows in source order, so check, axioms, emit and run see
them.  Generated names must be fresh against global entries, families,
constructors and both template catalogs.  Every installed constructor
label is checked against both catalogs and every generated name.

Symbolic family and member names cannot reuse a name in either source
catalog.  Member names also cannot reuse a symbolic constructor label.
Constructor labels can repeat across families and resolve by expected
family.  A failure returns no partial environment.  Declaration,
specialization and rechecking share the caller's budget.

PRENEX-GROUPS checks round-trips, exact inventories, normalization,
isolation, member order and precise refusals.  PRENEX-GROUPS-RUNTIME checks
member calls on the kernel, Node and Wasmtime, with a changed payload.
See `dev/M0-STAGE-C-PRENEX-GROUPS.md`.

### Checked categories

`prelude/cat/category.mech` provides a textual MechCategory template over
independent object and morphism Type levels. A specialization C installs
the equality family C and members C_Category, C_Hom, C_id, C_comp,
C_idComp, C_compId and C_assoc. C_Category takes its object type and
packages a hom family, identity, composition and three Prop-valued laws
as nested dependent pairs. Object arguments are explicit and erased.
Composition orders its arguments from x to y and then y to z.

Frozen elimination quotation reopens motives, branch bodies and addresses
in their captured environment before reification. Dependent projection
motives retain a typed first projection, including in the pair eta rule.
Constructor indices use typed conversion under the family index telescope,
starting from parameters and extending with each expected index. Existing
eta and proof irrelevance apply through this conversion. No new former,
schema constructor or elimination rule is introduced.

Concrete category projections and generic accessor calls compute on the
kernel and both WASM hosts.  See `dev/M0-STAGE-C-CATEGORY.md` for the
checked category contracts.

Indirect WASM calls dispatch on the arity stored in the closure.  A
dependent result can expose more parameters after specialization, so a
function representation does not prove the exact code-pointer arity.
An abstract nullary function dispatches with zero runtime arguments.
The helper invokes a stored nullary closure or returns a partial
application when its stored arity is positive.  That result has the
generic value representation.  Known global calls retain their declared
signatures.  See `dev/M0-STAGE-C-CLOSURES.md` for the runtime tests.
The group also provides eqTrans, eqCongr, Functor, functorObj,
functorMap, functorMapId, functorMapComp, idFunctor and compFunctor.
Functor takes two object types and their category records, all from
one specialization.  It packages an object map, a dependent arrow
map, and Prop-valued preservation laws for identity and composition.
compFunctor takes F then G and computes G after F on both maps.
Its laws follow by equality congruence and transitivity.

Object maps are runtime functions; arrow endpoints and the two law
fields erase.  Both categories share the specialization's object
level and morphism level.  See `dev/PORT-UAT-U1.md` for validation.
The group adds eqSymm, NatTrans, natApp, naturality, idNat, vcompLaw,
vcomp, whiskerRight and whiskerLeft.  NatTrans is a dependent pair
of an object-dependent component and a Prop-valued naturality law.
The law's morphism argument is available for proof construction;
the complete law field erases.  vcomp takes alpha then beta.
whiskerRight precomposes with an object and arrow map; whiskerLeft
maps components through a functor.  Their result types expand the
NatTrans fields for the composed functors.  The square-composition
lemma vcompLaw, category laws and functor laws prove naturality.
See `dev/PORT-UAT-U1-NATTRANS.md` for the checked contracts.
LeftKanExtension stores an extended functor, a unit and a solver
that returns each mediator with its factorization and uniqueness
proofs.  LanCocone expands the composed functor's maps.  LanFactor
states unit followed by the mediator at K's object.  LanSolution
packages the mediator and both laws.  lanFunctor and lanUnit read
the candidate; lanSolve supplies a solution.  lanDesc, lanFac and
lanUniq read that solution.  desc_unique derives pointwise
equality of two mediators that factor the same cocone, by
symmetry and transitivity.
See `dev/PORT-UAT-U1-LEFT-KAN.md` for the representation and tests.
Functors across separate universe pairs and source-type parity
remain open.
