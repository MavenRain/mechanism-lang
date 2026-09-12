# PORT-UAT U1: left Kan extensions

Date: 2026-09-12.  Base: 7c6d50d.  This increment adds left Kan
extensions within one MechCategory specialization.  Stage C and U1
remain open on separate category universe pairs and source parity.

## Contract

LeftKanExtension takes K from J to C and F from J to D.  It stores
an extension H from C to D, a unit from F to K followed by H, and
an operation that solves each cocone from F to K followed by G.
All three categories share the group's object level u and morphism
level v.  The object and morphism levels remain independent.

LanCocone spells out the components and naturality law of the
transformation from F to K followed by G.  It expands the composite
maps, as the existing whiskering operations do.  LanFactor states
that the unit at x followed by a mediator at K.obj x equals the
given cocone at x.  It uses the target category's composition and
the group's morphism equality.

LanSolution pairs a mediator with its factorization and uniqueness
proofs.  Uniqueness quantifies over each competing mediator and its
factorization proof.  It concludes equality of the two components
at every object of C.  This package keeps the proofs next to the
mediator they constrain and reduces nested record projections.

lanFunctor and lanUnit read the candidate.  lanSolve applies its
solver to G and a cocone.  lanDesc, lanFac and lanUniq read the
resulting LanSolution, with explicit candidate and cocone arguments.
desc_unique takes that solution and two mediators that factor the
same cocone.  It applies lanUniq to each, then symmetry and
transitivity.  The result is pointwise equality.  No equality of
whole natural transformations or function extensionality is used.

The unit and mediator components compute.  The factorization and
uniqueness fields are propositions and erase.  A specialization
installs the equality family and forty definitions.  The fifteen
new definitions add no axiom or kernel form.

recordFirst and recordSecond check projections against abstract
carriers.  LanTail pairs the unit and solver, and lanTail reads that
pair.  The mediator operations accept the solution package so their
projection motives do not expand a whole extension.
The accessors that return data carry quantity zero on their
type-only arguments.  They do not become runtime captures or
arguments.  lanFac, lanUniq and desc_unique return proofs that
erase whole, so their binders stay relevant.

## Validation

PRELUDE-LEFT-KAN checks the source from an empty environment.  It
checks parse/print round trips, four universe specializations,
installed members, family completeness and the absence of axioms.
The fixture constructs the extension along an identity functor for
arbitrary categories and F.  Its contract clients connect cocones
to composed functors and factorization to whiskerRight.

The concrete client uses the existing category of pairs of
endomorphisms.  It checks six computations, factorization and
desc_unique.  The cocones have distinct components, and one depends
on its object.  A second projection checks both halves of the
endomorphism pair.  The unit computation reads a second object and
the second projection.  The map computation runs the lanFunctor
arrow map of the swap functor, so the morphism argument changes the
answer.  Six negative fixtures reject a missing
uniqueness field, a false factorization, a false uniqueness premise,
a reversed unit, erased cocone data and reversed composition order.
The checker compares complete diagnostics and checks cancellation.

PRELUDE-LEFT-KAN-RUNTIME checks six exports on the kernel, Node and
Wasmtime, at payloads 37 and 41.  The expected answers distinguish
the two cocones and two object arguments.  The map export runs
through the swap functor and doubles its payload.  The existing runtime
helper checks and erases one program containing both payloads and
audits its axioms.  The proof-only contract clients run in the
source suite, keeping this runtime batch focused on computation.

The larger group raises the cost of every category specialization.
The category and functor source suites move from SLOW (120 s) to
CATEGORY (900 s), after reaching an intermediate 300 s ceiling.
The natural transformation source suite uses SUITE (300 s).
The four existing category runtime legs use CATEGORY (900 s), with
210 s per kernel batch.  The functor and natural transformation
harnesses keep an 870 s whole-run deadline.
These are watchdog ceilings.  Every predicate and refusal remains;
the trusted-line bounds remain 3000 kernel and 900 encoder lines.
Template checking and erasure still need performance work.

`dev/left-kan-mutations.py` changes the object argument, the cocone,
the mediator projection, one uniqueness premise and composition
order.  It requires a passing baseline, the expected refusal for
each control, and a passing restored source.  It records source
and checker hashes beside the captured outputs.

## Remaining work

The executable witness extends along an identity functor.  The
generic declarations allow arbitrary K, but this increment does
not construct an extension for every K and F.  Existence is data
that a client must supply.

Functors across separate universe pairs, checked imported source
types and bridge theorems remain due.  The grouped mediator and
proof representation needs a bridge to the source structure in
the parity track.  Mapping verdicts, denominators, trusted-line
bounds and vendor sources keep their existing contracts.
