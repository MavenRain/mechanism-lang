# M0 Stage C: dependent closure calls

Date: 2026-09-11.  Base: f1482dc, checked categories.

Generic category identity and composition now run on the kernel, Node
and Wasmtime.  Stage C remains open.  Functor, NatTrans,
LeftKanExtension and source-type parity remain due.

## Runtime contract

A generic category treats each morphism as an abstract value.  A
function category makes that value a function.  The erased generic
composition field has two runtime parameters, but its concrete closure
can have three.  The previous emitter cast that closure's code pointer
to the two-parameter signature and trapped.

Indirect calls now use the existing apply helper, which reads the
closure's stored arity.  It handles exact, partial and extra arguments.
Known global calls keep their direct signatures.  A second correction
dispatches abstract nullary closures when all source arguments erase.
Such a call returns a generic value.  When specialization exposes more
parameters, apply:0 returns a partial application of the stored closure.
When the stored arity is zero, it invokes the closure once.

The local WASM link and emit overlays implement these corrections.
The kernel, erasure, encoder, prelude definitions and vendor pin have
no source changes.  The trusted-line bounds and mapping verdicts stay
as before.

## Open boundary: a generic head at arity zero

A fully erased application whose head already widened to the generic
representation stays out of scope for this increment.  Dispatch at arity
zero keys on the static closure type of arity zero, so such a head gets
no apply step.  A dependent result that the link step widens to the
generic representation then answers the closure reference, not the
payload.  A WASM host refuses that value with an illegal cast while the
kernel answers the payload.

The repair needs a record of the application that is still due beside
the generic representation, because that representation keeps no such
count.  The emit guard for the skipped call reads the same record, so
the link step and the emit step must change together with a new fixture
export.  No staged fixture exports such a call.  The boundary stays open
with the Functor, NatTrans and LeftKanExtension work.

## Validation contract

PRELUDE-CATEGORY-ACCESSORS promotes the former diagnostic probe into
the ordinary gate battery.  Two category exports run at payloads 37
and 41 on all three hosts.  They exercise generic identity and ordered
composition.

DEPENDENT-CLOSURE-RUNTIME checks nine exports from a small source
fixture, then changes the shared input from 37 to 41 and repeats each
export.  The cases cover a nullary data result, a function result,
captured data, a global alias, partial and extra applications, an exact
call, a captured function and a nullary call inside another call.
Every export must produce the exact expected value on all three hosts.
The fixture also passes a source check and an empty axiom disclosure.

SUITE-WASM keeps the pinned source fixtures and its byte comparison,
WASM validation and kernel-to-Node result checks.  Eight local WAT
goldens record the changed dispatch instructions.  The gate runner
checks the exact overlay inventory and uses every remaining pinned
golden.  It never rewrites a golden during a gate run.  It also reads
the suite output and refuses unless each of the eight overlay fixtures
reports its own emission line, because the suite skips a fixture that
stops elaborating without any line.

Replay four isolated controls with:

```sh
python3 -I dev/closure-mutations.py /tmp/mechanism-closure-controls
```

The output directory must be new and outside the repository.  Each
control must build without warnings and cause the designated export
to fail on both WASM hosts while the kernel checks pass.  The baseline
and restored suites must pass.  Saved evidence lives under
`dev/validation/stage-c-closures/`.

After a build, `dev/wasm-golden-mutations.py NEW_DIRECTORY` checks that
a changed golden and a missing overlay each fail SUITE-WASM.  It uses
an isolated input tree and the current build.  The restored suite must
pass.  These two controls retain the byte comparison and inventory
checks as explicit acceptance requirements.
