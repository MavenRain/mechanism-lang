# PORT-UAT U1: natural transformations

Date: 2026-09-11.  Base: bb215e4.  This increment supplies checked
natural transformations within one MechCategory specialization.
Stage C and U1 remain open.

## Contract

The group gains eqSymm, NatTrans, natApp, naturality, idNat, vcomp,
vcompLaw, whiskerRight and whiskerLeft.  A specialization installs one
equality family and twenty-five definitions.  The existing category and
functor definitions keep their signatures and bodies.

NatTrans takes parallel functors F and G.  Its first field maps an
object x to a morphism from F.obj x to G.obj x.  This function can
inspect x at runtime.  Its second field is the naturality equation:
F.map f followed by app y equals app x followed by G.map f.
The equation is a proposition and erases.  natApp and naturality
project the two fields.  The law's morphism argument is available
for proof construction.  The complete law erases at runtime.

idNat uses the target category identity at each object.  Its proof
uses the two identity laws and symmetry.  vcomp applies alpha first,
then beta at each object.  The reusable vcompLaw proves composition
of two squares from associativity and congruence.  vcomp applies it
to both supplied naturality proofs.

The whiskering names follow CompCatTheory.Foundation.Category.
whiskerRight takes alpha and K from E to C.  It evaluates alpha at
K.obj x and applies naturality to K.map f.  whiskerLeft takes H from
D to E and alpha.  It maps alpha's component through H and proves
naturality with H's composition law on both sides.  Their result
types spell out the component and naturality fields of NatTrans
between the composed functors.  This avoids expanding complete
functor records while checking the template.

Every definition is checked universally and rechecked at closed
specialization.  All categories and functors use the same object
level u and morphism level v.  These two levels remain independent.
The source proofs add no axiom or kernel form.

## Validation

PRELUDE-NATTRANS checks the template from an empty environment,
parse/print round trips, four universe instances, installed names,
complete positive families, absence of axioms, kernel computations,
exact negative diagnostics and cancellation by an exhausted budget.

The source fixture uses endomorphisms with propositional endpoint
paths and a category of pairs of endomorphisms.  Alpha's component
depends on its object.  Alpha and Beta give different values in
opposite orders.  The fixture checks nested composition.  A functor
with a constant object map distinguishes precomposition from the
original component.
A swap functor distinguishes postcomposition from the original
component and checks both projections of the mapped pair.

The negative fixtures reject a reversed component endpoint,
reflexivity used as an arbitrary naturality proof, a missing law,
an erased runtime object, an incompatible middle functor, and a
whiskerRight that gets the postcomposition functor in the
precomposition position.  Complete diagnostics are compared byte
for byte.

PRELUDE-NATTRANS-RUNTIME checks eight exports on the kernel, Node
and Wasmtime at payloads 37 and 41.  Its Beta component reads its
object and its argument, so the two vertical orders and the nested
composition give three different answers.  Every answer changes
with the payload.  An emitted vertical composition that keeps one
transformation alone fails the leg.  Each
variant is checked and erased once by the existing prelude
runtime helper.  It audits source axioms before evaluating
or emitting exports.

The category and functor suites retain their existing fixtures,
negative diagnostics and computation checks.  Their inventories
include the nine new members in each of four specializations.
The build log records gate results and mutation evidence.

## Remaining work

Functor across separate universe pairs, LeftKanExtension,
desc_unique, source-type parity and bridge theorems remain due.
This increment does not change mapping verdicts, denominators,
trusted-line bounds, vendor sources or the U1 completion gate.
