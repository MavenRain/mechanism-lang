# M0 Stage B: lean4export import

Date: 2026-09-07.  Base: adf3085, Stage A.  Scope comes from
M0-PLAN.md sections 6, 8, 9, 10 and 11.  The source repositories and
vendor/kanon remain read-only.  The user requested continued development
and staging of all changes.  No commit is made by this workflow.

## Deliverables

The OCaml reader accepts lean4export format 3.1.0.  Its four tables retain
names, levels, expressions and declarations.  Each reference must resolve
to an earlier record in its table.  Anonymous name zero and universe zero
are implicit.  An inductive group contributes a declaration for each type,
constructor and recursor.  Bodies and recursor rules are reference-checked.
An inductive group must also agree with itself: each constructor is the one
its inductive type lists at that index, and each recursor rule of a
constructor of the group has that constructor's field count.  Translation
at M0 is limited to declaration types.

The CLI exposes `mech import FILE --out DIR` and the Stage B count report
through `mech diff-parity --export FILE`.  Type translation must preserve
universe parameters and arguments, binder scope and projection information.
A parsed declaration is not evidence of kernel acceptance.  Deferred work
must remain explicit in both the type table and the count report.  Prelude
mapping and NAME_AND_TYPE judgments belong to Stages C and D.

The local exporter specification is
`/Users/oobi/Documents/lean4export/format_ndjson.md` at exporter commit
8554815c2dc6b7abe99ec1f08849c9759ba77947.  The independent corpus validator
is `/Users/oobi/Documents/kanon-m2-corpus/dev/lean-parity/check-corpus.sh`.
The importer does not call that validator to parse its input.

## Validation

- BUILD: warnings are errors, using dev/dunecho.sh.
- IMPORT-GRAMMAR: full UAT read plus positive and negative reader tests.
- CORPUS-UAT: the independent frozen corpus gate prints CORPUS-OK.
- PARITY-COUNTS: 3,202 declarations, 2,477 distinct referenced external
  constants and 2,543 declared external constants.  Every kind is counted.
- Regression: existing kernel, levels, surface and WASM suites, pin checks,
  R0 checks and denominators continue to run.
- Mutation B-M1: admitting format 3.2.0 must break the version negative.
- Mutation B-M2: admitting a forward name reference must break INDICES.

The inherited TRUSTED-LINES failure remains visible until decision D-A-1
is ruled.  Stage B does not use importer lines to alter that limit.

## Workflow

1.  Inspect the committed base, plan, source format and frozen census.
2.  Build the reader and translation in parallel with separate ownership.
3.  Integrate the CLI, output tables, regression tests and gate legs.
4.  Review the integrated change, fix findings and run the mutations.
5.  Record measured results and remaining limits in the build log.
6.  Install the validated files with baseline hash checks and stage them.

The writable build copy is
`/Users/oobi/Documents/gpt1/mechanism-m0-stage-b`.  The canonical tree is
`/Users/oobi/Documents/mechanism-lang`.  Installation refuses a changed
canonical HEAD, changed source files or a changed vendor checkout.
