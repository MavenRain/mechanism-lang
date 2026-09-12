# PORT-UAT U1: category prelude

Date: 2026-09-11.  Base: 7742d96.  This increment supplies checked
functors between categories at the same object and morphism levels.
It is the first part of U1.  Stage C and U1 remain open.

## Contract

The MechCategory group gains nine members: eqTrans, eqCongr, Functor,
functorObj, functorMap, functorMapId, functorMapComp, idFunctor and
compFunctor.  A specialization C now installs one equality family
and sixteen definitions.  The seven existing category members keep
their signatures and bodies.

Functor takes object types C and D and category records c and d.
It contains an object map, a dependent arrow map, an identity law
and a composition law.  Both law fields are propositions and erase.
Object maps remain runtime functions.  Arrow endpoints are erased.
The record is a nested dependent pair and adds no kernel form.

compFunctor takes F from C to D, then G from D to E.  Its object
map computes G.obj (F.obj x).  Its arrow map computes G.map (F.map f).
The law proofs first apply equality congruence to F's law, then
transitivity with G's law.  Both equality helpers are source proofs
by elimination over the existing nominal equality family.

The definitions extend the existing group so they consume its
category records directly.  Separate template specializations have
distinct equality families.  A functor in this increment therefore
uses one group instance for both categories, with a common object
level u and a common morphism level v.  These two levels remain
independent of each other.

## Validation contract

PRELUDE-CATEGORY checks the extended template in its existing four
instances.  Its expected entry inventory grows from 49 to 85.
PRELUDE-FUNCTOR checks an empty environment, parse/print round trips,
four universe instances, seven kernel computations, seven exact
refusals and an exhausted budget.  It compares the full installed
family list and asserts the entry count, so an extra family or an
extra definition also fails.  All installed globals must be
definitions.  The tests consume generic dependent endpoints and
check higher-universe identity functors.

The object tests compose successor and constant maps in both orders.
The arrow tests compose a swap and a first-component duplication on
pairs of endomorphisms.  Both pairs of operations give different
answers when reversed.  Nested composition is also checked.
The false composition law uses f mapped to f composed with itself:
it preserves identity, so refusal must reach the composition field.
The refusal files pin complete diagnostics, without prefix matches.

PRELUDE-FUNCTOR-RUNTIME checks six exports on the kernel, Node and
Wasmtime.  All six run at payloads 37 and 41.  The leg also drives
two helper refusals: a source axiom and an export name that is not
a basename.  Its batch command has 90 seconds for six exports and
each host command has 20 seconds.  The leg keeps a 270-second
deadline, so a slow command prints its own label before the
300-second watchdog.  The object and arrow
composition tests each include both orders.  The new kernel gate
uses SLOW (120 seconds) and the runtime gate uses SUITE (300 seconds).

test/prelude_runtime.ml checks and erases each runtime variant once,
audits its axioms, then evaluates and emits every requested export.
It uses the same kernel and emitter APIs as the CLI.  The two existing
category runtime modes also use it.  Their host comparisons, expected
values and payload mutations remain in place.
PRELUDE-CATEGORY moves from FAST (10 seconds) to SLOW (120 seconds):
its four specializations now check sixteen members each, up from
seven.  Its inventory, computation and refusal checks remain intact.
The category runtime modes also move from MED to SLOW.  The larger
source template, with sixteen members per specialization, adds to
their check and emission work, and their wall time grows with host
load.  The staged battery measured 10.5 seconds for
PRELUDE-CATEGORY-RUNTIME and 11.2 seconds for
PRELUDE-CATEGORY-ACCESSORS at a load of 20.  The author battery that
the review replaced measured the same legs at 39.0 and 33.3 seconds,
above the 30-second MED limit.  The review notes in
dev/M0-BUILD-LOG.md record both runs.  SLOW (120 seconds) keeps a
margin of ten times over the staged walls.  The
individual host command limit stays at 20 seconds.  The category
batch command has 60 seconds for the shared check, erasure, kernel
evaluation and emission of its exports.  The category modes keep
every expected value, host comparison, fixture and refusal check.
They no longer call the mech CLI: the batch helper replaces check,
axioms, emit and run --host kernel for those two modes.  The other
runtime modes still drive the whole CLI surface.

Reproducible source mutation controls live in dev/functor-mutations.py.
Each control names a diagnostic that no other control can print.
Each replay run has 600 seconds, above the 120-second SLOW ceiling
of the gate leg, because the replay runs the suite six times.
Results and gate measurements are recorded in dev/M0-BUILD-LOG.md.

## Remaining U1 work

The next increment supplies NatTrans, idNat, vcomp, whiskerRight
and whiskerLeft; see `dev/PORT-UAT-U1-NATTRANS.md`.  LeftKanExtension
and desc_unique follow in `dev/PORT-UAT-U1-LEFT-KAN.md`.  Functor
between separate universe pairs remains due.  The source API uses
compFunctor to distinguish it from category composition in the flat
member namespace.  Source-type parity and bridge theorems remain
separate work.  No map verdict, denominator, trusted-line bound,
axiom or vendor source changes in this increment.
