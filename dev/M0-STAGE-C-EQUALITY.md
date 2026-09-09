# M0 Stage C: data equality and transport

Date: 2026-09-08.  Base: 44ab7b9, the committed Stage C foundation.
This increment resolves the foundation's data-equality declaration
blocker and adds checked transport to the prelude.  Stage C remains open.

## Declaration and checking

`MechEq (A : Type 0) (x : A) : A -> Prop` fixes the carrier and left
endpoint as erased parameters and binds the right endpoint as an erased
index.  Its single reflexivity constructor has no fields.  `mechJ` has a
Type 0 motive depending on both the right endpoint and the equality proof.
`mechTransport`, `mechSymm`, `mechTrans` and `mechCongr` are checked source
definitions, alongside `mechRefl`.

`Check.index_rules` permits an index above the family universe only when
that universe is definitely Prop.  It still checks that every index type
is well formed and every index binder has quantity zero.  Type-valued
families keep their predicative index bound.

The exception changes no constructor-field rule.  A constructor field of
a Prop family must still have a type living at Prop, including an erased
field.  This prevents an erased data witness from becoming a hidden choice
under proof irrelevance.  The large-elimination criterion is unchanged.
The nullary reflexivity constructor qualifies, while the older
MechProofEq constructor with a runtime proof field still fails it.
MechEq therefore obtains large elimination from the unchanged kernel
criterion at M0.  This does not discharge verdict D1.  The subsingleton
criterion proof for Eq, owed by `M0-PLAN.md` lines 29 and 87, stays an
M1 obligation.

The scrutinee proof erases to the existing erased value, emitted as
immediate tag zero.  A nullary singleton case reads that tag and enters its
only branch without projecting a payload.  Runtime transport therefore
uses the existing eraser and emitter.  The EQUALITY-RUNTIME gate checks
this path on the kernel evaluator, Node and Wasmtime.

## Scope

The prelude declarations are monomorphic at their written universes.
The equality regression client separately checks a higher-carrier family
and a type-cast example, establishing that the index exception is not
limited to the first data universe.  Universe-polymorphic family
templates and a general prelude cast remain future work.

Seven new external-name candidates cover Eq, Eq.refl, Eq.rec, Eq.ndrec,
Eq.symm, Eq.trans and congrArg.  Their verdict remains NAME_ONLY.  Stage D
must compare universally checked target types with imported source types.
The external denominator and all NEVER exceptions remain unchanged.

## Validation contract

PRELUDE checks equality clients and precise failure diagnostics from an
empty kernel environment.  EQUALITY checks raw declaration rules,
universe scope, quantities and the constructor-field boundary.
EQUALITY-RUNTIME compares fixed observable results across all three hosts
and changes a transported payload to verify that the oracle observes it.
AXIOMS continues to reject hidden postulates.  The inherited gate battery
covers the kernel, universes, surface, WASM, import, corpus, R0 and vendor
pin.  The trusted-line bounds remain 3,000/900.

The build copy is `/Users/oobi/Documents/gpt2/mechanism-lang`.  Integration
checks the canonical HEAD and original file hashes before copying only
the validated changes into `/Users/oobi/Documents/mechanism-lang` and
staging them.  No commit is created.
