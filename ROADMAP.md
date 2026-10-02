# mechanism-lang roadmap

This file is the roadmap of mechanism-lang.  It records the ratified
milestone ladder, the position of the repository on that ladder, and
the three port tracks.  The design verdict of 2026-09-07 ratified the
ladder and the port order.  The verdict and the M0 plan live outside
the repository, in `~/Documents/mechanism-lang-design-verdict.md` and
`~/Documents/mechanism-lang-m0/M0-PLAN.md`.  This file repeats the
parts of them that the repository must answer to, so the repository
carries its own roadmap.

A roadmap line is a plan, not a result.  A result lives in
`dev/M0-BUILD-LOG.md` and in the gate battery `dev/gates.sh`.  A stage
changes this file only through its design document in `dev/` and the
commit that the user runs.

## Milestones

Each milestone has one measurable gate.  The gate prints its count
beside the NEVER count, so a percentage is always read with the number
of names that mechanism-lang refuses to map (R-V5).

- M0 PRELUDE-CHECKED.  Each of the 2,477 external constants of the UAT
  closure has a prelude target that checks against its translated
  export type, or one line in `map/NEVER.tsv`.  UNMAPPED and NAME_ONLY
  fail the gate.  D3 prenex levels land here.
- M1 PARITY-UAT-90 with R3 gate (a).  The translator re-checks at
  least 90 percent of the 3,202 declarations of the UAT closure, by
  kind. The warm compilation time per kloc is at most 1.000 of the matched
  Bend 2 denominator, measured on a quiet window. The ratio is UNMEASURED.
- M2 PARITY-UAT-100 with PORT-DAO.  The translator re-checks 3,202
  minus the ratified NEVER declarations and prints the residue by
  kind.  The DAO verdict runs on the Z2 witness on both hosts.
- M3 PORT-AUCTION with R3 gate (b).  FIXTURES-OK 5 of 5 against the
  frozen `ports/auction-cat/fixtures.json`, with no wasm_decide on the
  oracle path. The complete corpus compilation time is at most 1.000 of
  matched Bend 2 compilation, using equivalent workloads and cache conditions
  on a quiet window. The ratio is UNMEASURED.

The user ruling of 2026-09-21 replaces both OCaml compilation targets with
Bend 2. Compilation includes parsing, elaboration, kernel checking, erasure,
Wasm emission and validation. The design and preparation log are recorded in
`dev/BEND2-GATES.md`. Historical OCaml denominators remain frozen in
`dev/denominators.json`; they do not establish either current R3 gate.

Out of M0, each with its milestone: Auto and instance resolution,
decide, omega, rewrite, wasm_decide with trustedWasm, Rat, Fin and
Int, the five Extern globals, String literals, well-founded recursion
and the runtime package at M1; Quot with SPar and Quot.sound at M2;
SNu, macro, syntax, elab and Array literals NEVER through M3.

## Position on 2026-10-02

The base is 18477c9. The typed compatibility pilot checks 11 of the 19
existing candidate signatures and all four U1 record signatures at symbolic
universes. Seven equality targets need symbolic adapters, and
Decidable.isFalse needs a checked Not mapping. These are signature results;
constructor and recursor representation, whole-record equality and full
typed mapping remain open. The inventory and NEVER ledger are unchanged.
See `dev/M0-STAGE-C-COMPATIBILITY-PILOT.md`.

The preceding increment sends general value comparisons at universe types
directly to structural conversion. Universe types have no value eta or
subsingleton rule, and their proposition classification is already false.
The conversion budget suite compares this route with type conversion and
checks both sides of the exact poll boundary. Comparisons at other types
retain the existing proof irrelevance and eta rules. The poll reproducer
compares this dispatch with the committed predecessor and records unchanged
poll counts. All 20 kernel and 14 frontend unit groups pass.
See `dev/validation/universe-dispatch/`.

The preceding increment skips proposition probes for universe type arguments
to general value conversion. A universe inhabits a successor universe and
cannot be a proposition. Other types retain the quoting and inference probe,
including its budget-exhaustion propagation. Four regressions distinguish
and identify opaque types in both Prop and Type. All 20 kernel and 14 frontend
unit groups pass. A comparison of an opaque proposition type with itself
uses zero checker polls, versus two through the preceding probe. This
measures one conversion, not whole-program runtime.
See `dev/validation/universe-conversion/`.

The preceding increment sends type comparisons directly to structural
conversion. Types inhabit a universe, so the proof-irrelevance,
subsingleton and value-eta probes in general conversion cannot apply.
The value conversion path is unchanged. Kernel and frontend unit suites
cover proposition and universe distinctions, binder renaming, proof
irrelevance and budget boundaries.
See `dev/validation/type-conversion/`.

The preceding runtime increment checks the complete source and erases
the shared dependency closure of requested exports. Its dedicated gate
is PRELUDE-RUNTIME-REACHABLE. See `dev/validation/runtime-reachable/`.
The preceding prelude increment transports universal solutions to pointwise equal
cocones and units, retaining their mediators and rebuilding factorization
and uniqueness. Its dedicated gates are PRELUDE-LEFT-KAN-SOLUTION-TRANSPORT
and PRELUDE-LEFT-KAN-SOLUTION-TRANSPORT-FOCUS. Its optional runtime mode
remains open after reaching the unchanged 480-second emission limit.
See `dev/PORT-UAT-U1-LEFT-KAN-SOLUTION-TRANSPORT.md`.
The preceding increment characterizes cocone equality by pointwise mediator
equality and proves that mediator components are independent of the chosen
universal solution. Its dedicated gates are
PRELUDE-LEFT-KAN-MEDIATOR-EQUALITY,
PRELUDE-LEFT-KAN-MEDIATOR-EQUALITY-RUNTIME and
PRELUDE-LEFT-KAN-MEDIATOR-EQUALITY-FOCUS.
See `dev/PORT-UAT-U1-LEFT-KAN-MEDIATOR-EQUALITY.md`.
The preceding increment proves both mediator round trips: reconstruction gives
an equal cocone, and choosing a mediator for a postcomposed unit recovers the
original transformation componentwise. Its dedicated gates are
PRELUDE-LEFT-KAN-ROUNDTRIP, PRELUDE-LEFT-KAN-ROUNDTRIP-RUNTIME and
PRELUDE-LEFT-KAN-ROUNDTRIP-FOCUS.
See `dev/PORT-UAT-U1-LEFT-KAN-ROUNDTRIP.md`.
The preceding increment supplies postcomposition identity, vertical composition
and congruence directly in the cocone equality relation, at six independent
universe levels. Its dedicated gates are PRELUDE-LEFT-KAN-COCONE-ACTION and
PRELUDE-LEFT-KAN-COCONE-ACTION-RUNTIME. See
`dev/PORT-UAT-U1-LEFT-KAN-COCONE-ACTION.md`.
Reflexivity, symmetry and transitivity for pointwise cocone equality use the
dedicated kernel and runtime gates PRELUDE-LEFT-KAN-COCONE-EQUALITY and
PRELUDE-LEFT-KAN-COCONE-EQUALITY-RUNTIME. See
`dev/PORT-UAT-U1-LEFT-KAN-COCONE-EQUALITY.md`.
The preceding congruence laws are described in
`dev/PORT-UAT-U1-LEFT-KAN-COCONE-CONGRUENCE.md`.
The identity and vertical-composition laws are described in
`dev/PORT-UAT-U1-LEFT-KAN-COCONE-LAWS.md`.
The mediator identity, congruence and postcomposition laws are described in
`dev/PORT-UAT-U1-LEFT-KAN-LAWS.md`.
Dependency export clauses inside `poly group` let chosen local member names
survive nested composition,
family reuse and closed specialization. PRENEX-DEPENDENCY-EXPORTS and
its runtime gate check the surface contract and execution on three hosts.
See `dev/M0-STAGE-C-DEPENDENCY-EXPORTS.md`. Stage 0, Stage
A (prenex levels) and Stage B (import foundation) are committed.
Stage C has landed its data foundation, data equality and transport,
family templates, polymorphic transport and cast, equality operations,
dependent functions and pairs, ordered family groups and congruence,
textual prenex definitions, families and groups, checked categories,
and dependent closure calls.  The first U1 increment adds checked
functors, identity and composition within one category group instance.
Natural transformations now supply identity, vertical composition
and both whiskering operations in that group.  Left Kan extensions
add universal mediators and pointwise uniqueness. Stage C remains
open on equality of
whole transformation records, and source-type parity.
Stage D (typed mapping) and Stage E (M0-EXIT) have not started. The map
inventory holds 19 NAME_ONLY, 2,273 UNMAPPED and 185 NEVER rows of
2,477. The
gate battery includes PRELUDE-LEFT-KAN and PRELUDE-LEFT-KAN-RUNTIME.
It also includes TEMPLATE-COMPOSITION, PRELUDE-HETEROGENEOUS-FUNCTOR
and their runtime gates. PRELUDE-COMPOSABLE-FUNCTORS and its runtime
gate check composition within a shared three-category group. The
PRELUDE-HETEROGENEOUS-NATTRANS gates check natural transformations
within a shared source/target pair and compare their runtime results.
The PRELUDE-HETEROGENEOUS-WHISKERING gates check both whiskering
operations over a shared three-category group and compare four exports
on the kernel, Node and Wasmtime at two payloads. The
PRELUDE-HETEROGENEOUS-LEFT-KAN gates check universal mediators over
that shared triple and compare four exports at two payloads.
TEMPLATE-REUSE checks closed family sharing and mixed-universe functor
contracts; TEMPLATE-REUSE-RUNTIME compares repeated composition and
identity on three hosts at two payloads. TEMPLATE-SYMBOLIC-REUSE checks
symbolic sharing and mixed-universe category contracts, and its runtime
gate repeats the four exports through one shared group. The
PRELUDE-SHARED-NATTRANS gates check the shared transformation APIs,
horizontal composition and six refusals, and compare four exports on
three hosts at two payloads. PRELUDE-SHARED-LEFT-KAN checks shared
unit and mediator APIs, independent specializations and seven refusals.
Its runtime gate compares six exports on three hosts at two payloads.
PRELUDE-NATTRANS-LAWS checks pointwise vertical laws at independent
universes and six refusals. Its runtime gate compares six certified
component applications on three hosts at two payloads.
PRELUDE-WHISKERING-LAWS checks preservation of identity, vertical
composition and equality by both whiskering operations. Its runtime
gate compares twelve exports on three hosts at two payloads.
PRELUDE-HORIZONTAL-LAWS checks horizontal identity, congruence,
naturality exchange and vertical interchange with seven refusals. Its
runtime gate compares fourteen exports on three hosts at two payloads.
After the Veil kernel migration, TRUSTED-LINES recorded
kernel=5475/3000 and encoder=246/900. On 2026-09-26 the user
approved a 9000-line Bend kernel limit; the encoder limit remains 900.

## Port tracks

The user ratified the port order UAT, DAO, auction-cat on 2026-09-07
(question Q6), each under `ports/<repo>`.  Each port has two legs
(Q2).  Leg 1 is parity: the lean4export closure of the source
repository re-checks in the mechanism-lang kernel.  Leg 2 is
execution: frozen numeric fixtures evaluate to the same bytes on the
kernel evaluator, Node and Wasmtime.  Leg 2 runs auction-cat first,
because it holds the only confirmed numeric fixtures (verdict
question 3).

Each port has two tiers.  Tier A is the mechanical translation of the
export closure by `mech import`.  It supplies the leg 1 number.  Tier
B is the idiomatic port: `.mech` source in the P2 library shape, with
one bridge theorem per root declaration that ties the idiomatic
declaration to its tier A translation.

### Track 1: unified-aggregation-theory, PORT-UAT

The ledger is `ports/unified-aggregation-theory/README.md`.  It pins
the source at f9d2bc2 on Lean v4.31.0, the frozen export by SHA-256,
and the denominators: 570 roots (365 UnifiedAggregation, 113 ArrowCat,
92 CompCatTheory), 3,202 closure declarations and 2,477 referenced
external constants.

Tier A is the ratified parity leg.  Its gate is PARITY-UAT: 90 percent
at M1 and 100 percent minus NEVER at M2.  The M0 gate PRELUDE-CHECKED
is its precondition, because every external constant of the closure
must have a checked target first.

Tier B is the source port.  It is new in this roadmap.  It lands in
six stages in dependency order.  Each stage is one workflow on the
user's opt-in, with a design document `dev/PORT-UAT-U<n>.md`, a
section in `dev/M0-BUILD-LOG.md` and a commit that the user runs.
The reusable framework lands in `prelude/aggregate/` and the bridges
stay under `ports/` (D-UAT-3).

- U1, M0.  The category prelude completes.  `prelude/cat/` gains
  Functor, idFunctor, comp, NatTrans, idNat, vcomp, whiskerRight,
  whiskerLeft, LeftKanExtension and desc_unique, the shapes of
  comp-cat-theory's Foundation/Category.lean (126 lines) and
  Primitive/KanExtension.lean (145 lines).  This is the open Stage C
  item.  The port track consumes it.  Gate: PRELUDE-CATEGORY extends
  by the new records.
  The first increment is specified in `dev/PORT-UAT-U1.md`: Functor,
  idFunctor and compFunctor within one universe pair.  It adds the
  PRELUDE-FUNCTOR and PRELUDE-FUNCTOR-RUNTIME gates.  The next
  increment, `dev/PORT-UAT-U1-NATTRANS.md`, adds NatTrans, idNat,
  vcomp, whiskerRight and whiskerLeft with kernel and runtime gates.
  `dev/PORT-UAT-U1-LEFT-KAN.md` adds LeftKanExtension, its accessors
  and desc_unique, with kernel and runtime gates. The heterogeneous
  functor increment adds independent source and target universe pairs,
  accessors and checked laws over a lightweight category core; see
  `dev/PORT-UAT-U1-HETEROGENEOUS-FUNCTOR.md`. Composition over a shared
  three-category group is specified in
  `dev/PORT-UAT-U1-COMPOSABLE-FUNCTORS.md`. Natural transformations
  at independent universe pairs are specified in
  `dev/PORT-UAT-U1-HETEROGENEOUS-NATTRANS.md`. Both heterogeneous
  whiskering operations over a shared three-category group are specified
  in `dev/PORT-UAT-U1-HETEROGENEOUS-WHISKERING.md`. Heterogeneous left
  Kan extensions over that shared group are specified in
  `dev/PORT-UAT-U1-HETEROGENEOUS-LEFT-KAN.md`. Closed family sharing,
  identity and repeated composition between independent functors are
  specified in `dev/M0-STAGE-C-REUSE.md`. Symbolic sharing inside template
  groups follows in `dev/M0-STAGE-C-SYMBOLIC-REUSE.md`. Shared natural
  transformations and horizontal composition follow in
  `dev/PORT-UAT-U1-SHARED-NATTRANS.md`. Shared left Kan extensions
  follow in `dev/PORT-UAT-U1-SHARED-LEFT-KAN.md`. Pointwise vertical
  transformation laws follow in `dev/PORT-UAT-U1-NATTRANS-LAWS.md`.
  Pointwise preservation of identities, vertical composition and equality
  by whiskering follows in `dev/PORT-UAT-U1-WHISKERING-LAWS.md`.
  Horizontal identity, congruence, exchange and vertical interchange follow
  in `dev/PORT-UAT-U1-HORIZONTAL-LAWS.md`. Iterated precomposition,
  iterated postcomposition, and mixed whiskering now share four category
  families with eight independent universe levels; see
  `dev/PORT-UAT-U1-ITERATED-WHISKERING.md`. Pointwise horizontal
  associativity follows in
  `dev/PORT-UAT-U1-HORIZONTAL-ASSOCIATIVITY.md`. Identity whiskering and
  horizontal units follow in `dev/PORT-UAT-U1-NATTRANS-UNITS.md`.
  Pointwise left Kan mediator identity and cocone congruence follow in
  `dev/PORT-UAT-U1-LEFT-KAN-LAWS.md`.
  U1 remains open on
  equality of whole transformation records and source-type parity.
  Checked template composition supplies the source foundation;
  see `dev/M0-STAGE-C-COMPOSITION.md`.
- U2, M1.  Foundation modules: FunctorExt, HeqTransport, Discrete,
  Indiscrete, ConfigSpace, SymmetricGroup, Z2Group and ChoiceRule.
  The last four are framework and land in `prelude/aggregate/`.  The
  U2 design document names the prelude directory of the first four.
  The eight HEq helpers of HeqTransport become eight lemmas.  Its
  tactic suite (15 macro and syntax lines) is NEVER under Q4.  Needs:
  generic Eq at Prop with J and cast (D1), structure eta at SPi, and
  funext as a tracked axiom (D-UAT-4).
- U3, M1.  Aggregation core, in `prelude/aggregate/`: Aggregation
  (OrbitGroupoid, OrbitHom, orbitProjection, Aggregation as
  LeftKanExtension), Regimes, Characterization and Trichotomy.  Needs:
  Classical.em and Classical.choice as the tracked port axioms.
- U4, M1 to M2.  The arrow-cat sub-port (7 files, 1,297 lines, 56
  theorems, 12 defs) as `ports/unified-aggregation-theory/arrow-cat/`
  (D-UAT-2), then Bridge/ArrowImpossibility and Bridge/ArrowDebreu in
  `src/`.  Needs: Fin, decide and Auto for DecidableEq instances (M1).
- U5, M2.  Bridge/SchellingIsing, Bridge/Potts and
  TrichotomyWitnesses.  Needs: Rat (D6) and Int (M1), Quot.sound for
  funext (M2), and the kan_saturate lemma chains (D-UAT-5).
- U6, M2.  Leg 2.  Frozen fixtures for the mean-field fixed-point
  check and the Potts order parameter, evaluated on three hosts.
  Precondition: a fixture is confirmed to exist (verdict, port ledger
  row UAT leg 2, "unverified").

Under D-UAT-1, PORT-UAT closes at M2 before PORT-DAO, because the DAO
denotation is UAT's Aggregation and its fork transition cites
mean_field_bifurcation.

Gate lines of the track.  Each leg prints one PASS or FAIL line under
a watchdog tier in `dev/gates.sh`, and the count is a prefix oracle,
never a frozen number.

- PARITY-UAT checked=n/3202 by kind, NEVER=k.  Tier A.  PASS at 90
  percent (M1) and at 100 percent minus k (M2).
- PORT-UAT-SOURCE roots=n/570 bridges=b/570 NEVER=k.  Tier B.  PASS
  when n + k = 570 and b = n.
- PORT-UAT-HEADLINE checked=7/7.  The seven headline theorems check,
  and `mech axioms` prints exactly propext, Classical.choice and
  Quot.sound for them, plus the tracked funext before M2 (D-UAT-4):
  no Extern and no trustedWasm.
- PORT-UAT-RUNTIME OK cases=c hosts=3 mutation=1.  Leg 2, in the shape
  of EQUALITY-RUNTIME: kernel evaluator, Node and Wasmtime, byte
  equal, with one payload-change control.

Rulings of the track, ratified by the user on 2026-09-11.

- D-UAT-1.  Tier B closes at M2, before PORT-DAO.
- D-UAT-2.  arrow-cat ports as the sub-tree
  `ports/unified-aggregation-theory/arrow-cat/`, inside the 570-root
  denominator.
- D-UAT-3.  The reusable framework (SymmetryGroup, GAction,
  ChoiceRule, Aggregation, the three regimes, trichotomy) moves into
  `prelude/aggregate/`, because PORT-DAO imports it.  The bridges stay
  under `ports/`.
- D-UAT-4.  Before M2, funext is a tracked axiom printed by
  `mech axioms`.  It retires when Quot.sound lands.  UAT has 35 funext
  sites.
- D-UAT-5.  The 16 kan_saturate sites become explicit lemma chains
  over the normalized Rat carrier with the rewrite tool at fuel 512.
  The automation set stays at four.
- D-UAT-6.  The frozen export stays referenced by sha and path in the
  kanon-m2-corpus checkout.  No copy lands under `ports/`.

### Track 2: self-referential-dao, PORT-DAO

Milestone M2.  Source: `~/Documents/self-referential-dao` at a965183,
2 files, 810 lines.  The DAO denotation is `Aggregation act F`, so the
track opens after U3 and closes after U5.  It imports the framework
from `prelude/aggregate/` (D-UAT-3).  Leg 1 adds 38 roots to the UAT
closure.  That closure is not exported yet.  Leg 2 needs a computable
refinement of govObj, noncomputable at SelfReferentialDAO.lean:332.
The refinement is an M2 deliverable with its own gate.  The M2 gate
runs the DAO verdict on the Z2 witness on both hosts.

### Track 3: auction-cat, PORT-AUCTION

Milestone M3, leg 2 first.  Source: 5f11578 on v4.33.0-rc1, 31 files,
14,062 lines.  The five Examples values are lifted to named defs,
evaluated at v4.33.0-rc1 and frozen with a sha as
`ports/auction-cat/fixtures.json` at M1.  The closure exports at M0
into `ports/auction-cat/export/` (verdict question 6).  Its
denominator is unknown until then, and its ledger carries
Lean.ofReduceBool as a source-side row.  The M3 gate is FIXTURES-OK 5
of 5 with no wasm_decide on the oracle path, plus R3 gate (b).

## Change control

A change to this file needs a design document in `dev/` and a build
log section, and the user commits it.  A new NEVER row needs the
user's ratification and an exact per-constant classification, never
an inferred category.  A frozen denominator in `dev/denominators.json`
never changes in place; a re-measurement is a new dated file.
