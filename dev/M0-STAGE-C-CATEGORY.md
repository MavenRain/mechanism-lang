# M0 Stage C: checked categories and dependent records

Date: 2026-09-10. Base: 1f5d7bf. This increment starts the category
prelude after textual ordered groups. Stage C and PRELUDE-CHECKED remain
open. No mapping row is promoted to NAME_AND_TYPE.

## Source contract

Concatenate `prelude/cat/category.mech` with a client in one source
check. The group uses the existing template syntax:

```text
specialize MechCategory (1, 0) as Types
```

This installs equality Types for carriers at Type 0, plus
Types_Category, Types_Hom, Types_id, Types_comp, Types_idComp,
Types_compId and Types_assoc. The arguments are independent object and
morphism Type levels. Category takes an object type, and lives at
`Sort (max (succ u) (succ (succ v)))`.

The record is a nested dependent pair of a hom family, identity,
composition, left identity, right identity and associativity.
Composition takes x-to-y then y-to-z arrows. Each law compares arrows in
the group's nominal equality family. The hom family and operations take
explicit, erased object arguments. Proof fields have Prop types and are
erased by the existing type-directed pass. Callers supply all data and
proofs in nested pairs. No postulates or resolution mechanism are added.

## Kernel corrections required by the record

1. Quotation of a frozen elimination now opens its motive and branches
   under the captured environment and fresh binders before quoting them.
   Indexed motives add all indices followed by the scrutinee binder.
   The branch address round trip through the captured environment is
   defensive. It is the identity on the M0 branch addresses, a leg
   label and a constructor name.
   The old evaluator copied syntax whose free indices belonged to a
   different context, breaking dependent projections and partial
   matches.
2. A second projection's motive retains a typed first projection. The
   surface elaborator and pair eta rule use the same motive and
   scrutinee quantity. An untyped neutral first projection cannot be
   inferred when the dependent field applies that projection as a type
   family.
3. Constructor result indices compare at the family's index types. The
   telescope starts with family parameters and extends with each checked
   expected index. Constructor result expressions still evaluate under
   constructor parameters and fields. This makes existing typed eta and
   proof irrelevance available at indices without changing either rule.

These changes reconstruct existing judgments. They add no term, shape,
axiom, universe rule or large-elimination permission. The two
environments in the index check remain distinct: constructor fields do
not scope the family index telescope. A bad carrier, later dependent
index or distinct data endpoint is still rejected. The vendored
submodule is unchanged.

## Validation

PRELUDE-CATEGORY checks 49 definitions and seven families from
Global.empty. Four instances cover (0,0), (1,0), (0,1) and (2,1). Seven
exact normal forms distinguish composition order, heterogeneous arrows,
type-valued results and captured data. Three partially applied matches
are normalized, rechecked and compared with the original functions. Two
of them use an indexed motive, and one of those two reads an index
binder and the scrutinee binder in the same motive body. That motive
fixes the order of the fresh quotation binders. The fixture checks
generic category law projections and dependent pair eta. Eight misuse
cases pin complete diagnostics, and a ninth case checks the exhausted
caller budget. One misuse case reads an erased hom endpoint in a runtime
position. The suite compares the sorted names in `test/neg/category`
with its own list, so a new or a dropped negative refuses. The printed
quotation count is measured from the list of round trips. Template
globals remain private, inventories are exact and every global is a
definition with empty axiom disclosure.

PRELUDE-CATEGORY-RUNTIME checks concrete record projections on the
kernel, Node and Wasmtime. The payload changes from 37 to 41 while the
second composition stays 12. Both category variants share one prelude
prefix, and every invoke elaborates that prefix again. The first `emit`
of a variant makes the same source pass as `check`, and it prints a
kernel refusal on a source error. The category modes therefore make no
separate check pass and no axiom pass. The mutation variant of the
concrete mode runs the payload export only, because the second
composition gives 12 with and without the mutation. The concrete mode
makes twelve subprocesses: eight for the original variant and four for
the mutation variant. The other modes keep their own check and axiom
passes. The ordinary gate battery includes these two new legs.

Run `python3 -I dev/category-mutations.py NEW_WORK_DIRECTORY` to replay
ten isolated controls. Each mutant must build without errors or warnings
and fail the designated category check. The final restored copy must
pass. Evidence is recorded in `dev/validation/stage-c-category/`. The
replay kills all ten controls and the restored suite passes. The
recorded outputs, the replay report and the current source hashes are
rechecked with
`python3 -I dev/category-mutations.py --verify WORK_DIRECTORY`, which
also runs against a copy of the recorded evidence directory outside
the repository.

## Remaining runtime boundary

Generic category accessor calls check and compute in the kernel but trap
on Node and Wasmtime with an illegal cast. Direct projections from a
concrete record pass. The diagnostic probe is deliberately separate from
the passing concrete-projection gate:

```sh
env -u OPAM_SWITCH_PREFIX -u CAML_LD_LIBRARY_PATH \
  python3 -P test/prenex_runtime.py --category-accessors
```

It uses `test/fixtures/prelude/category-accessor-runtime.mech` and
returns nonzero until the emitter supports this boundary. It reports
eight host failures over two exports and two payload variants, while
kernel checks pass. The harness separates the two outcomes. A failing
check, axiom, emit or kernel invoke prints `kernel_checks=`, and host
traps alone print `host_checks=`, so a kernel regression stays
distinguishable from the documented boundary. This increment does not
claim generic category accessor WASM parity. Functor, NatTrans,
LeftKanExtension and checked source-type mapping are still outstanding.

The trusted bounds remain 3,000 kernel and 900 encoder lines. The active
kernel is 4,208 lines, up from 4,182, and the encoder remains 246 lines.
The bound ruling remains open. No watchdog tier or acceptance threshold
is changed.
