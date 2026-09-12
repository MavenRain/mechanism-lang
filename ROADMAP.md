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
  kind.  The warm check time per kloc is at most 1.000 of the ocamlopt
  denominator re-measured on a quiet window.
- M2 PARITY-UAT-100 with PORT-DAO.  The translator re-checks 3,202
  minus the ratified NEVER declarations and prints the residue by
  kind.  The DAO verdict runs on the Z2 witness on both hosts.
- M3 PORT-AUCTION with R3 gate (b).  FIXTURES-OK 5 of 5 against the
  frozen `ports/auction-cat/fixtures.json`, with no wasm_decide on the
  oracle path.  The ported corpus checks in at most 1,641.6 ms per its
  own kloc on a quiet window.

Out of M0, each with its milestone: Auto and instance resolution,
decide, omega, rewrite, wasm_decide with trustedWasm, Rat, Fin and
Int, the five Extern globals, String literals, well-founded recursion
and the runtime package at M1; Quot with SPar and Quot.sound at M2;
SNu, macro, syntax, elab and Array literals NEVER through M3.

## Position on 2026-09-12

The base is 7c6d50d, the U1 natural transformation increment.
The next increment adds left Kan extensions.  Stage 0, Stage
A (prenex levels) and Stage B (import foundation) are committed.
Stage C has landed its data foundation, data equality and transport,
family templates, polymorphic transport and cast, equality operations,
dependent functions and pairs, ordered family groups and congruence,
textual prenex definitions, families and groups, checked categories,
and dependent closure calls.  The first U1 increment adds checked
functors, identity and composition within one category group instance.
Natural transformations now supply identity, vertical composition
and both whiskering operations in that group.  Left Kan extensions
add universal mediators and pointwise uniqueness.  Stage C remains
open on functors across separate universe pairs and source-type
parity.  Stage D (typed
mapping) and Stage E (M0-EXIT) have not started.  The map inventory
holds 19 NAME_ONLY, 2,273 UNMAPPED and 185 NEVER rows of 2,477.  The
gate battery adds PRELUDE-LEFT-KAN and PRELUDE-LEFT-KAN-RUNTIME.
TRUSTED-LINES stays red at
kernel=4208/3000 and encoder=246/900 until the user rules D-A-1.

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
  and desc_unique, with kernel and runtime gates.  U1 remains open
  on separate category universe pairs and source-type parity.
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
