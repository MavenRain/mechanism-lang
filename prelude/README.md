# Checked foundations

`init.mech` defines ten SMu families and sixteen functions without axioms or
primitives.  The test harness checks it in `Global.empty`.  This prevents
the driver's initial `Nat` axiom from supplying a hidden dependency.

The data families are `MechNat`, `MechBool`, `MechUnit`, `MechEmpty`,
`MechSum`, and `MechDecidable`.  They live at `Type 0`.  `MechTrue` and
`MechFalse` live at `Prop`.  Decidable has two data constructors with
erased proof fields.  It does not resolve instances or decide propositions.
`MechPi` and `MechSigma` expand to the existing function and dependent pair
forms.  These declarations are monomorphic at the stated universes.

`MechProofEq` is restricted to two proofs of the same proposition.
`mechProofRefl` and `mechProofJ` check for that family, and J has a
Prop-valued motive that depends on both endpoints and the equality proof.
This restricted family is not a mapping target for Lean's general `Eq`.
Proof irrelevance already identifies its endpoints.  Its constructor has
a runtime witness field, so the existing subsingleton check refuses
large elimination.

`MechEq` compares values of any carrier at `Type 0`.  It fixes the left
endpoint as an erased parameter and the right endpoint as an erased
index, with a nullary reflexivity constructor.  The checker now permits
erased data indices in Prop families.  Constructor-field bounds and the
subsingleton large-elimination criterion retain their existing behavior.
`mechJ` has a Type 0 motive depending on the right endpoint and the proof;
`mechTransport`, `mechSymm`, `mechTrans` and `mechCongr` are checked source
definitions alongside `mechRefl`.

`test/fixtures/prelude/equality.mech` checks dependent transport, J
computation, and a separate higher-carrier equality with a type-cast
example.  The `.mech` declarations remain monomorphic.  The separate
programmatic catalog below supplies polymorphic library cast.  The precise negatives
keep unequal endpoints, relevant indices, erased endpoint use and data
constructor fields outside the accepted language.

The source client in `test/fixtures/prelude/client.mech` checks after this
file.  It constructs values, exercises dependent fields and nested
constructors, and uses singleton indices to check recursor computation.
The harness also checks rejection diagnostics in `test/neg/prelude`.

Parameterized constructors use the local surface elaborator overlay.
The expected family type supplies its parameters.  Each field elaborates
at its declared type under the parameters and preceding fields.  The
kernel checks the complete constructor and its result indices.

`families.ml` supplies a separate programmatic catalog of universally
checked MechEq and MechSum templates through `Family_poly`.  MechEq takes
the carrier's Sort level, including Prop.  MechSum takes two Type levels;
the kernel levels of its carriers are their successors.  Closed instances
have fresh names and are rechecked before entering ordinary globals.
Constructor names are local to the expected family, so instances can share
`mechReflCtor`, `mechInl` and `mechInr` with the monomorphic declarations.

The PRELUDE-POLY gate installs five instances and checks
`test/fixtures/prelude/polymorphic.mech`.  Indexed witnesses force casts at
two carrier universes and mixed-universe sums to compute.  The test audits
both global entries and families, rejecting axioms, primitives, provisional
families and builtin families.  Textual universe binders and imported
source-type parity are not supplied by this catalog.

`equality.ml` supplies a separate catalog with checked member definitions.
Its MechEq takes the carrier and motive Sort levels and includes `refl`,
`transport`, `j`, `symm`, `trans` and `congr`.  Its MechTypeEq takes one
Type level and includes `refl`, `cast`, `symm` and `trans`.
Specialization installs each member with the instance name
and an underscore as a prefix, then rechecks its type and body in order.
The templates never enter ordinary globals.  PRELUDE-TRANSPORT checks
seven instances, dependent computation witnesses, four rejection
diagnostics, one scope control and the absence of trusted entries.  A
member named like another template is refused as a collision. Textual
family templates now use the same API, as described in
`dev/M0-STAGE-C-PRENEX-FAMILIES.md`. Textual member definitions and
source-type parity remain separate work.

The `j` motive can depend on both the right endpoint and the proof,
using erased binders for each.  Its carrier and motive Sort levels are
independent, including Prop.  `congr` uses the same carrier Sort level
for its domain and codomain.  The PRELUDE-EQUALITY-OPS gate checks ten
universe instances, generic contracts with variable endpoints, eleven
kernel normalization witnesses and eight misuse cases with accepted
controls.  The fixture includes a motive indexed by its equality proof.
See `dev/M0-STAGE-C-EQUALITY-OPS.md` for the operation signatures.

`dependent.ml` supplies a separate Poly catalog with six definitions:
`MechPi`, `MechSigma`, `mechSigmaMk`, `mechSigmaFst`, `mechSigmaSnd` and
`mechSigmaRec`.  Pi takes two Sort levels, including Prop.  The pair
definitions take two Type levels, with a third motive Sort level for
the recursor.  Fibers depend on the first component, and recursor motives
depend on the whole pair.  Each definition can be instantiated separately
under a fresh name because its body uses the existing kernel forms
directly.  Equal specializations are transparent aliases of those forms.

PRELUDE-DEPENDENT checks 29 specializations, generic contracts, twelve
independent normal forms and six invalid uses with accepted controls.
The source fixture `test/fixtures/prelude/dependent.mech` exercises a
Boolean-dependent fiber, a pair containing a type and a value of that
type, a type-valued second field, and proof-valued functions and
elimination.  The catalog checks from `Global.empty`, and the installed
environment is audited for axioms, primitives and unchecked families.
The catalog must also refuse an exhausted caller budget.  The reported
specialization count is measured in the installed environment.

`congruence.ml` supplies the MechCongr ordered family-group template.
Its two parameters are independent domain and codomain Sort levels,
including Prop.  Specializing under N installs equality N over the domain,
equality N_Result over the codomain, and N_congr.  For a function f from
A to B, N_congr takes N A x y and returns N_Result B (f x) (f y).
These nominal families are separate from Equality.catalog instances.

PRELUDE-CONGRUENCE checks eight specializations, generic contracts with
distinct bound endpoints, four observable computations, five misuse cases
with accepted controls, budget refusal and the absence of trusted entries.
FAMILY-GROUPS covers ordered dependencies, companion universe substitution,
name collisions, scope, closed rechecking, template references from a
member, and a refusal after which the caller globals stay usable.  See
`dev/M0-STAGE-C-CONGRUENCE.md` for the API and reproducible controls.

## Textual ordered groups

`poly (...) mu ... and ... where def ... end` declares ordered family
templates and their nonrecursive members.  Each explicit specialization
renames and rechecks the complete group.  The executable clients in
`test/fixtures/prelude/prenex-groups.mech` cover data, types and proofs
from an empty environment.  See `dev/M0-STAGE-C-PRENEX-GROUPS.md` for
grammar, naming and validation.

Symbolic members elaborate and check in one ordered pass through the
family catalog.  Closed specialization still rechecks every member.
See `dev/M0-STAGE-C-TEMPLATE-CHECKING.md` for the callback contract
and declaration cost measurements.

## Categories

`cat/category.mech` declares the MechCategory group with two Type-level
parameters: objects at `Type u` and morphisms at `Type v`. Specializing
as C installs equality C and forty members.  The category
members are
C_Category, C_Hom, C_id, C_comp, C_idComp, C_compId and C_assoc.
The category record contains its
hom family, identity, composition and three proofs. Object arguments are
explicit and erased. Proof fields are erased by their Prop types.

Composition takes an arrow from x to y followed by an arrow from y to z.
The function-category examples therefore compute `g (f value)`. A caller
constructs a category with nested pairs and supplies every law. The
library declares no axioms and has no automatic instance resolution.

PRELUDE-CATEGORY checks four universe specializations, heterogeneous
composition, generic law projections, pair and function eta, captured
elimination quotation, exact refusals and the caller budget.  Concrete
record projections and generic accessor calls run on all three hosts.
PRELUDE-CATEGORY-ACCESSORS checks generic identity and composition.
DEPENDENT-CLOSURE-RUNTIME checks dependent function results and calls
whose source arguments all erase.  See `dev/M0-STAGE-C-CLOSURES.md`.

The nine functor members are C_eqTrans, C_eqCongr, C_Functor,
C_functorObj, C_functorMap, C_functorMapId, C_functorMapComp,
C_idFunctor and C_compFunctor.  A functor takes two categories from
the same group instance.  Its object map and dependent arrow map
compute, and its identity and composition laws are propositions.
Composition takes F then G.  Its checked proofs use equality
congruence and transitivity.

PRELUDE-FUNCTOR checks four universe instances, generic endpoints,
identity, both composition orders and nested composition.  Its seven
exact misuse cases include a map that preserves identity but fails
composition.  PRELUDE-FUNCTOR-RUNTIME exercises object and arrow maps
on all three hosts, with two payloads.  See `dev/PORT-UAT-U1.md`.
Natural transformations add C_eqSymm, C_NatTrans, C_natApp,
C_naturality, C_idNat, C_vcompLaw, C_vcomp, C_whiskerRight and
C_whiskerLeft.  Components can inspect their objects.  Naturality
proofs can inspect their morphisms and erase as complete Prop fields.
Vertical composition takes alpha then beta.  UAT's whiskerRight
precomposes and whiskerLeft postcomposes.  PRELUDE-NATTRANS checks
their contracts; PRELUDE-NATTRANS-RUNTIME checks eight exports with
two payloads on three hosts.  See `dev/PORT-UAT-U1-NATTRANS.md`.

Left Kan extensions add C_recordFirst, C_recordSecond, C_LanCocone,
C_LanFactor, C_LanSolution, C_LanTail, C_LeftKanExtension,
C_lanFunctor, C_lanTail, C_lanUnit, C_lanSolve, C_lanDesc, C_lanFac,
C_lanUniq and C_desc_unique.  lanSolve supplies the checked solution
that lanDesc, lanFac and lanUniq read.  Generic pair projections and
LanTail support the record accessors.  The mediator has factorization
and pointwise uniqueness proofs.  PRELUDE-LEFT-KAN checks these
contracts and misuse cases.  PRELUDE-LEFT-KAN-RUNTIME checks six
exports on three hosts.  See `dev/PORT-UAT-U1-LEFT-KAN.md`.

Functors across separate universe pairs and source-type parity
remain open.

Template composition can import category templates under separate
universe arguments.  The composition fixture checks a functor shape
between those categories.  This is a source-layer prerequisite;
the public category prelude still uses its existing universe pair.
See `dev/M0-STAGE-C-COMPOSITION.md`.

## Functors across independent universe pairs

Load `cat/category-core.mech`, then `cat/heterogeneous-functor.mech`.
`specialize MechHeterogeneousFunctor (0, 1, 2, 3) as Mixed` installs
`Mixed_Source` and `Mixed_Target` equality families, their category
operations, and `Mixed_Functor`, `Mixed_functorObj`, `Mixed_functorMap`,
`Mixed_functorMapId`, `Mixed_functorMapComp` and `Mixed_eqCongr`.
The four arguments are the source object and hom levels followed by
the target object and hom levels. Both preservation laws use target
equality. Congruence eliminates source equality and constructs target
equality without identifying the two nominal families.

`MechCategoryCore` has the same category representation and first nine
operations as `MechCategory`, with its equality family renamed. It
omits the same-pair functor, natural-transformation and left Kan API,
so composed pairs check only the category foundation they consume.
The kernel gate compares the first 70 lines of the two files, with the
family renamed, so the copies cannot drift.
The existing `cat/category.mech` API remains available. Independent
pair instances still need a shared middle category for general
heterogeneous functor composition. See
`dev/PORT-UAT-U1-HETEROGENEOUS-FUNCTOR.md` for the contract and examples.

## Heterogeneous functor composition

Load `cat/category-core.mech`, then `cat/composable-functors.mech`:

```text
specialize MechComposableFunctors (0, 1, 2, 3, 4, 5) as Chain
```

The three object/hom pairs belong to Source, Middle and Target.
`Chain_First_Functor C D c d` and `Chain_Second_Functor D E d e`
share the middle category record. Apply
`Chain_compFunctor C D E c d e F G` to obtain
`Chain_Composite_Functor C E c e`. Each of the three functor prefixes
has object and arrow accessors, both law accessors and congruence.
The laws transport the first functor's preservation proofs through
the second functor before composing proofs in Target equality.

Separate template specializations still have distinct nominal
families. This API shares instances within a three-category group;
general reuse between groups and repeated composition remain due.
See `dev/PORT-UAT-U1-COMPOSABLE-FUNCTORS.md` for the full contract,
generic signatures, negative fixtures and runtime checks.

## Heterogeneous natural transformations

Load `cat/category-core.mech`, `cat/heterogeneous-functor.mech`, then
`cat/heterogeneous-nattrans.mech`:

```text
specialize MechHeterogeneousNatTrans (0, 1, 2, 3) as N
```

`N_Base_Functor C D c d` uses `N_Base_Source_Category C` and
`N_Base_Target_Category D`. For two such functors F and G,
`N_NatTrans C D c d F G` pairs runtime components with naturality
in target equality. `N_natApp` and `N_naturality` project those fields.
`N_idNat C D c d F` supplies identity. Apply
`N_vcomp C D c d F G H alpha beta` for alpha followed by beta.
Nested vertical composition uses these same functor types.

The template reuses MechHeterogeneousFunctor through one Base
specialization. Separate group instances retain distinct nominal
families. Heterogeneous whiskering and left Kan extensions remain due.
See `dev/PORT-UAT-U1-HETEROGENEOUS-NATTRANS.md` for the signatures,
universe rule and validation contract.
