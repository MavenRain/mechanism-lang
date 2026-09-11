# M0 build log

## Stage 0 (2026-09-07)

Stage 0 creates the repository, vendors kanon at the pin, carries the
bench script and freezes the denominators.  No kernel code, no import
code and no prelude source is written at Stage 0.  No agent commits.
The judge ran every gate of the brief section 4 again on 2026-09-07 and
the three mutation checks of section 5 on scratch copies.  The evidence
below holds the judge's own printed lines.

### Deliverables

- The repository /Users/oobi/Documents/mechanism-lang, `git init -b main`,
  branch main and no commit.
- The directory tree of M0-PLAN.md:45-66.  lib/, import/, map/, bin/,
  prelude/ with its eight sub-directories, ports/auction-cat/export/ and
  test/fixtures, test/neg and test/golden are created empty, because
  their files are Stage A to Stage E work.
- vendor/kanon, a git submodule with url /Users/oobi/Documents/kanon,
  detached at 936a43a92dd59a04698648f24fa5ae94cdb532df.
- PIN, the full sha and one newline.
- dev/pin-delta.sh and dev/PIN-DELTA.md.
- dev/bench.sh, carried from the pin with zero byte difference.
- dev/denominators.json and dev/DENOMINATORS.sha256.
- dev/trusted-lines.sh, with the kernel at 3,000 lines and the encoder
  at 900.
- dev/gates.sh with the Stage 0 legs BUILD, PIN, PIN-DELTA and
  DENOMINATORS, and TRUSTED-LINES as an informational note.
- dev/dunecho.sh, dune-project, dune, README.md, SPEC.md, LICENSE-MIT,
  LICENSE-APACHE and .gitignore.
- dev/M0-BUILD-LOG.md and dev/MUTATION-LOG.md, written by the judge.

### Gates

| id | result | evidence |
| --- | --- | --- |
| S0-G1 | PASS | `git symbolic-ref --short HEAD` prints `main`;  `git status -sb` prints `## No commits yet on main`;  `git rev-list --count HEAD` exits 128 with `fatal: ambiguous argument 'HEAD': unknown revision`, so the commit count is zero.  `git rev-parse --abbrev-ref HEAD` prints `HEAD` and exits 128 on an unborn branch, not `main`. |
| S0-G2 | PASS | `git -C vendor/kanon rev-parse HEAD` prints `936a43a92dd59a04698648f24fa5ae94cdb532df`;  `git -C vendor/kanon symbolic-ref -q HEAD` exits 1, so HEAD is detached. |
| S0-G3 | PASS | `PASS PIN pin=936a43a92dd59a04698648f24fa5ae94cdb532df gitlink=936a43a92dd59a04698648f24fa5ae94cdb532df head=936a43a92dd59a04698648f24fa5ae94cdb532df want=936a43a92dd59a04698648f24fa5ae94cdb532df` |
| S0-G4 | PASS | `zsh dev/dunecho.sh build` exits 0 and prints `OK build: 0 errors, 0 warnings`. |
| S0-G5 | PASS | `PIN-DELTA file diff expected` then `PIN-DELTA OK`, exit 0. |
| S0-G6 | PASS | `BENCH true median_ms=10.228 min_ms=8.948 max_ms=12.440 runs=5` in the judge's run, median under 20. |
| S0-G7 | PASS | `BENCH-ERROR bad exit=1`, exit 1. |
| S0-G8 | PASS | node reads the file and prints `DENOM-OK` with `DENOM medians ocamlopt_ms_per_kloc=8157.104 ocamlopt_ms_per_kloc_remeasure=1441.031` and `DENOM missing none`. |
| S0-G9 | PASS | `denominators.json: OK` |
| S0-G10 | FAIL, informational at Stage 0 | `TRUSTED-LINES kernel=3816/3000 encoder=246/900 FAIL`, exit 1.  The leg is informational at Stage 0 by brief 3.10, so it is not one of the four hard legs and gates.sh still ends GATES-OK.  See finding S0-F1, which needs a user ruling before Stage A. |
| S0-G11 | PASS | `PASS BUILD`, `PASS PIN pin=936a43a...`, `PASS PIN-DELTA`, `PASS DENOMINATORS denominators.json: OK`, then `MEASURE BUILD tier=SLOW elapsed_ms=85.608 exit=0`, `MEASURE PIN tier=FAST elapsed_ms=69.843 exit=0`, `MEASURE PIN-DELTA tier=MED elapsed_ms=41.766 exit=0`, `MEASURE DENOMINATORS tier=MED elapsed_ms=57.842 exit=0`, `MEASURE TRUSTED-LINES tier=FAST elapsed_ms=36.781 exit=1`, `NOTE TRUSTED-LINES (informational at Stage 0)`, then `GATES-OK`, exit 0. |
| S0-G12 | PASS | 23 directories of the section 3 tree print `DIR OK` and 17 Stage 0 files print `FILE OK`;  `git ls-files` holds 16 .gitkeep paths;  after `git add -A` the counts are `staged=36` and `porcelain=36`, and `rg -c '^\?\?'` and `rg -cv '^A '` both match no line, so every staged row is an addition and no path is untracked. |
| S0-G13 | PASS | the pin worktree prints no porcelain line and HEAD is 936a43a92dd59a04698648f24fa5ae94cdb532df;  /Users/oobi/Documents/kanon HEAD is the same sha and its porcelain line count is 16, the count recorded before the submodule add. |

Carry checks beside the gates.  `cmp -l` between the pin's dev/bench.sh
and the repository copy prints 0 lines, and both files hash
d408fb5a3d88da3334783e0fb67e8c7ce25aa546c4c428853031d83cfd30d5ae.  The
R0 block of SPEC.md compares equal to the pin's SPEC.md:176-183 under
`cmp`, over 8 lines.  LICENSE-MIT and LICENSE-APACHE compare equal to
the pin's copies.

### Mutations

S0-M1, S0-M2 and S0-M3 all killed on scratch copies.  The repository
files were never written.  dev/MUTATION-LOG.md holds the commands and
the printed lines.

### Frozen numbers

- kanon pin 936a43a92dd59a04698648f24fa5ae94cdb532df.
- ocamlopt_ms_per_kloc 1641.6, NOISY, carried from the kanon pin with
  median_ms 8157.104, min_ms 5949.238, max_ms 14355.273, runs 5 and
  lines 4969.
- ocamlopt_ms_per_kloc_remeasure 231.863 on 2026-09-07, median_ms
  1441.031, min_ms 1295.708, max_ms 1536.366, runs 5, lines 6215 over 24
  files, binding false.
- kanon_check_ms_per_kloc 43, binding false.
- uat_export_sha256 f4439dce6a0b488e9bc328592e53c47867c4d19fb123b31358aeb35ed5d14354.
- lean_warm_uat_ms 6383 and lean_warm_auction_ms 5846, both lower bounds
  and both binding false.
- Trusted base at the pin: kernel 3816 lines over the ten vendored
  files, encoder 246 lines.
- Host arm64, 12 processors, macOS 26.4, OCaml 5.2.1 and dune 3.24.2.
- dev/DENOMINATORS.sha256
  3148d714a481696297e75ef416255618cbc2cd9b56ab84c34f5fb9464fa7cd51.

### Findings

- S0-F1, open, and it needs a ruling before Stage A.  The kernel budget
  is already spent.  The ten vendored kernel
  files hold 3,816 lines at the pin, which is 816 lines above the 3,000
  bound of M0-PLAN.md:164, before the D3 overlay adds one line.  The
  counts are shape.ml 60, term.ml 133, rules.ml 1481, check.ml 538,
  value.ml 137, eval.ml 297, conv.ml 396, totality.ml 146,
  positivity.ml 118 and order.ml 510.  The pin's own script holds the
  kernel at 4,000 over thirteen files.  No agent moves either number, so
  the script carries 3,000 and prints FAIL.  The leg is informational at
  Stage 0, so GATES-OK holds, and it becomes hard at Stage A.  The
  finding is open and it needs a ruling before Stage A: either the bound
  moves, or the believed file list changes, or the overlay stage carries
  a smaller trust base.
- S0-F2.  The BUILD leg compiles no vendored file at Stage 0.  dune
  keeps a vendored directory out of the default alias, so `dune build`
  at the root builds the mechanism package alone and
  _build/default holds no vendor path.  BUILD ran in 85.608 ms in the
  judge's run.  The
  vendored tree does compile: a copy of vendor/kanon lib/ builds through
  the same runner with `OK build: 0 errors, 0 warnings`.  Stage A makes
  the vendored library a build dependency of the D3 overlay, and BUILD
  covers it from that stage on.
- S0-F3.  M0-PLAN.md:68 reads the vendored lib as 6,193 lines over 22
  files.  `wc -l` over vendor/kanon/lib/*.ml and lib/*.mli at the pin
  prints 6,215 lines over 24 files.  The re-measure row records the
  measured pair, not the plan's pair.
- S0-F4.  dunecho takes one MODE and no path, and it holds no clean
  mode, so the re-measure command of the brief is not reachable through
  dev/dunecho.sh as written.  Resolved by S0-D7.

### Decisions

- S0-D1.  The package is declared at Stage 0, in the pin's form:
  `(package (name mechanism) (allow_empty) (depends (zarith (= 1.14))))`.
  Evidence: dune-project on disk, and `OK build: 0 errors, 0 warnings`
  with an empty lib/.  Stage A then adds source files and no build file
  changes.
- S0-D2.  The warnings-as-errors env block lands at Stage 0:
  `(env (_ (flags (:standard -warn-error +a))))` in the root dune file.
  Evidence: the BUILD leg prints `OK build: 0 errors, 0 warnings` with
  the block on disk.  dune relaxes the warning set inside a vendored
  directory, so the block does not break the vendored tree.
- S0-D3.  The re-measure row is
  `ocamlopt_ms_per_kloc_remeasure` with the fields date, value,
  median_ms, min_ms, max_ms, runs, lines, files, corpus, command,
  binding and note.  Evidence: the judge's node check prints
  `DENOM remeasure-fields OK keys=date,value,median_ms,min_ms,max_ms,runs,lines,files,corpus,command,binding,note`,
  `DENOM remeasure-date OK date=2026-09-07`,
  `DENOM remeasure-binding-false OK binding=false` and `DENOM-OK`.
  Stage D reads this field set.
- S0-D4.  The empty-directory marker is `.gitkeep`, one file in each of
  the sixteen leaf directories: lib, import, map, bin,
  ports/auction-cat/export, prelude/prelude, prelude/cat, prelude/stoch,
  prelude/mechanism, prelude/game, prelude/aggregate, prelude/charter,
  prelude/host, test/fixtures, test/neg and test/golden.  prelude/,
  ports/, ports/auction-cat/ and test/ carry no marker, because git
  records them through their children.
- S0-D5.  No tier moved.  BUILD is SLOW, PIN is FAST, PIN-DELTA is MED,
  DENOMINATORS is MED and TRUSTED-LINES is FAST, with FAST 10, MED 30,
  SLOW 120 and SUITE 300 seconds.  Evidence: the builder's MEASURE block
  reads 112.998 ms, 45.147 ms, 29.582 ms, 37.490 ms and 24.667 ms, and
  the judge's run reads 85.608 ms, 69.843 ms, 41.766 ms, 57.842 ms and
  36.781 ms.  Every leg is two orders of magnitude under its hang
  ceiling.
- S0-D6.  The overlay scope of dev/pin-delta.sh is the OCaml source
  trees lib/, wasm/, surface/ and bin/, with the extensions .ml and
  .mli.  Evidence: dev/, test/ and the repository prose hold files whose
  names also exist in the vendored tree, such as dev/bench.sh and
  README.md, and none of them overlays a kanon source file.  With the
  scope as ruled the Stage 0 overlay set is empty, which is what the
  brief states, and the mutant of S0-M2 is killed.
- S0-D7.  The re-measure command deviates from the brief and the row
  records the deviation.  dunecho takes no path and holds no clean mode,
  and a vendored directory is outside the default alias, so
  `dev/dunecho.sh build ./vendor/kanon/lib` is not reachable.  Evidence:
  `dunecho: too many arguments, don't know what to do with
  ./vendor/kanon/lib`, exit 124.  The measured pair removes _build with
  rm -rf and then builds through the same runner script, over a copy of
  the vendored tree that holds lib/, dune-project, dune and
  dev/dunecho.sh only, so the build compiles the named corpus and
  nothing else.  The row is informational and binding false.

### Close

Every Stage 0 path is staged and nothing is committed.  The user's first
commit is one command:

```
git -C /Users/oobi/Documents/mechanism-lang commit -s -m 'M0 Stage 0: repository, vendored kanon, bench and denominators'
```

## Stage A implementation (2026-09-07)

Entry HEAD is 71f47449f943f54efb819a7bbaba175b3eb08703.  The canonical
repository was clean.  Work was built and reviewed in
`/Users/oobi/Documents/gpt1/mechanism-m0-stage-a`, then installed with
baseline and copy hashes checked.  The vendor pin, frozen denominators
and all watchdog tiers are unchanged.  No commit is made.

### Implementation and review

The overlay implements the five semantic level forms with compact,
arbitrary-precision successor offsets.  Equality and ordering split each
variable into zero and positive cases and compare unbounded normal
forms.  These are exact decisions, with checking-budget polls throughout
comparison.  No finite set of numeric samples is used as the oracle.

The checker admits universe variables only under a declared global
template arity.  A raw-term scope pass also checks fields that a typing
rule can ignore.  Templates stay in a separate, abstract Poly catalog;
each explicit closed specialization is rechecked before Global insertion.
Family universe declarations remain closed at this stage.  Term and shape
constructors are unchanged.  The integration decisions are recorded in
`M0-STAGE-A.md` and SPEC.md.

Dune recompiles unchanged pinned modules alongside the physical overlays.
The inherited tests link the mechanism libraries and read the pinned
fixtures.  The driver carries the pinned commands through a separate
entry point, and its Node and Wasmtime helpers are unchanged copies.

Independent review found two defects before validation: ignored shape
fields could retain free universe variables, and symbolic comparison
could run past its caller's budget.  Both were fixed with regressions.
Conversion now also preserves budget exhaustion during its proposition
probe.  A second read-only review found no further concrete defect in
the scope, conversion or specialization boundaries.

### Validation status

The final kernel count is 4,140 lines, counting each active trusted
implementation once and every additional local kernel source/interface.
The encoder remains 246 lines.  The original kernel bound remains 3,000
and the encoder bound remains 900.  TRUSTED-LINES is now a hard gate and
fails.  S0-F1 / D-A-1 is unresolved: a 4,200-line kernel limit was proposed
to the user, but no ruling has been received.  This is a staged
implementation with a known failing gate, not a Stage A acceptance.

The working-copy build reports `OK build: 0 errors, 0 warnings`.  The
inherited kernel, surface and WASM suites pass, as do R0 count and dispatch
checks, pin verification and frozen-denominator verification.

An initial PIN-DELTA driver count used a stdin diff, whose repeated-line
alignment differed from the script's regular-file diff.  The ledger was
corrected to the script's measured 14 lines.  A subsequent battery's
PIN-DELTA leg overlapped a second invocation and lost their shared scratch
directory.  An isolated rerun passed all six rows: check 66, conv 51,
rules 47, level implementation 49, level interface 17 and driver 14.
The failed attempts are not counted as passing battery runs.

Mutation testing initially exposed missing coverage of Rules_lvl.imax:
the algebra tests exercised Level.imax directly.  Checker-level
impredicativity tests were added before the mutation checks were repeated.
The mutation log records both the initial survivor and final results.

### Canonical validation and staging

The installed 39 files matched their recorded source hashes.  A separate
read-only verification confirmed exactly the expected changed/untracked
paths, unchanged repository HEAD and a clean vendor worktree at the pin.
The final canonical battery ran with no overlapping gate invocation:

| leg | result | measured ms |
| --- | --- | --- |
| BUILD | PASS, zero errors and warnings | 2626.722 |
| PIN | PASS, all three pins equal the frozen SHA | 66.949 |
| PIN-DELTA | PASS, all six overlays | 441.734 |
| R0-COUNT | PASS | 650.112 |
| R0-AUDIT | PASS, effective compiled source | 108.931 |
| SUITE-KERNEL | PASS | 684.625 |
| LEVELS | PASS, 55 cases | 464.362 |
| SUITE-SURFACE | PASS | 268.604 |
| SUITE-WASM | PASS | 4802.691 |
| TRUSTED-LINES | FAIL, kernel 4140/3000, encoder 246/900 | 43.704 |
| DENOMINATORS | PASS | 42.776 |

The battery exits 1 and prints `GATES-FAIL`, solely for the unchanged
kernel bound.  All three final mutations compile and are killed by the
behavioral suite.  No failed attempt or unmet bound is reported green.
The evidence log was then updated and all 39 changed files staged in the
canonical repository, with no unstaged changes and no commit.

## Stage A review (2026-09-07)

Findings kept, all fixed.

- L1-1 (low), lib/level_var.ml.  offset built a Succ past the
  positive-offset invariant.
- L4-2 (low), test/levels.ml.  The unchanged-environment assertion of
  failed-instances-preserve-environments was vacuous.
- L3-3 (low), test/levels.ml.  The level suite stopped at the first
  failing case, so run could not show the A-M2 checker observation.
- L4-1 (low), test/levels.ml.  Bare integer division on staged
  non-vendored lines.
- L3-1 (low), dev/PIN-DELTA.md.  Four new dune overlays shadowed pinned
  build files with no PIN-DELTA row.
- L3-4 (low), dev/gates.sh.  The battery header named three legs that
  are not legs of plan section 9.
- L3-2 (low), dev/M0-BUILD-LOG.md.  One space after a sentence, 43
  authored hits in eight files.

Fixes: lib/level_var.ml; test/levels.ml; dev/PIN-DELTA.md; dev/gates.sh;
dev/M0-BUILD-LOG.md; dev/MUTATION-LOG.md; dev/trusted-lines.sh;
README.md; dev/r0-audit.sh; dev/r0-count.sh; lib/dune; test/dune.

Gate verdict BOUND-ONLY, kernel 4140, encoder 246.  TRUSTED-LINES stays
red until the user rules D-A-1.

## Stage B import foundation (2026-09-07)

Base adf30859a0d2d75e0bb4938b19a0e89218211ae0.  The user requested
continued development and staging of all changes.  The canonical tree was
clean.  Work used the isolated copy documented in dev/M0-STAGE-B.md, with
parallel reader and translator builders, separate regression tests, review,
fixes and isolated mutation checks.  No commit was made.

Delivered: a strict OCaml lean4export 3.1.0 reader, four typed tables,
shared scoped type translation, checked lowering through an explicit
resolver, the import CLI, intermediate artifacts and count reports.
The raw UAT export is read independently by the OCaml importer and by the
frozen corpus validator.  No dependency was installed or source corpus
regenerated.  Unix is the standard-library dependency for file output.

Scope result: the import foundation is validated; full Stage B remains
incomplete.  The plan requires every translated kernel type, but source
constant references need checked prelude mappings that have not yet been
built.  Installing those signatures as unchecked globals would silently
introduce postulates, so the implementation retains them as DEFERRED.
Projection lowering also needs the checked prelude representation.
Stage C supplies the prelude, and Stage D establishes mapping equivalence.
A successful lower_type call proves only that its result is a kernel type
under the supplied mapping.  It does not prove source-to-target parity.

The 3,202 declaration types resolve into 38,644 shared expression nodes.
All declarations remain in the output: 100 have KERNEL_TYPE and 3,102 have
DEFERRED with a named reason.  This corpus run has zero UNSUPPORTED and
zero KERNEL_ERROR rows.  NEVER remains zero because no ledger is applied.
No NAME_AND_TYPE claim or parity percentage is printed.

| Kind | Declared | Kernel types | Deferred |
| --- | ---: | ---: | ---: |
| axiom | 3 | 0 | 3 |
| def | 1176 | 23 | 1153 |
| thm | 1649 | 0 | 1649 |
| opaque | 1 | 0 | 1 |
| quot | 4 | 1 | 3 |
| inductive | 112 | 76 | 36 |
| constructor | 143 | 0 | 143 |
| recursor | 114 | 0 | 114 |

Frozen census: 219,778 lines, 17,759 emitted names, 146 emitted levels,
198,927 expressions, 3,017 distinct const-node identities, 2,477 referenced
external constants and 2,543 declared external constants.  The export SHA
is f4439dce6a0b488e9bc328592e53c47867c4d19fb123b31358aeb35ed5d14354.
The source axioms are Classical.choice, Quot.sound and propext.

The five roots absent from the 3,202 declarations, as required by plan N3:
CompCatTheory.Category.«term_≫_», CompCatTheory.Category.«term𝟙»,
CompCatTheory.Functor.«term_⋙_», CompCatTheory.«term_⟹_» and
CompCatTheory.«term_⥤_».  They remain visible here and are not used to
change the denominator.

Validation: build passed with zero errors and warnings.  The 120 reader
cases plus the full frozen corpus, 28 type/lowering cases and 12 CLI cases
passed.  The CLI suite also invokes the independent JSON artifact checker
on its own fixture.  That gated run covers the fixture output only.  The
fixture carries an implicit lambda binder, a forall binder, a nondependent
let, a projection, a metadata node and a universe parameter, so each
per-kind branch of the checker runs in the gate.  A separate by-hand run
applied the same checker to the complete UAT output, comparing reachable
source type nodes, universe graphs, raw names, declaration order and
parameters.  Its evidence is the independent output check recorded below.
The version and forward-name mutations were killed; see MUTATION-LOG.md.

The last full battery had 14 passing legs and one inherited failure:
TRUSTED-LINES kernel=4140/3000 encoder=246/900.  No kernel source, encoder,
pin, trusted-line limit or watchdog tier changed.  The new type translation
is outside the kernel line budget; source correspondence remains a separate
trust obligation, as the plan states.

Selected informational measurements, in milliseconds, from the final full
battery: BUILD 1776.181, IMPORT-GRAMMAR 1469.614, IMPORT-CLI 180.796,
CORPUS-UAT 5520.692, PARITY-COUNTS 1131.559.  These include host noise and
concurrent isolated mutation work.  They are not R3 performance claims.
The later addition of the artifact check to the CLI suite passed its own
scoped rerun.  It changes no compiler source or gate tier.

Review fixes: census sets now use canonical structural identities, raw
names are retained in artifacts, display strings are injective across
numeric/string/quoted names, aliases share universe binders, and lowering
has finite node-work and kernel budgets.  A depth-25 shared Pi DAG that
would otherwise expand exponentially now reports budget exhaustion.
Output errors use one named stdlib exception boundary returning Result.

Stage B review fix: the reader checks that an inductive group agrees with
itself.  Each constructor must be the one its inductive type lists at that
index, and a recursor rule of a constructor of the group must carry that
constructor's field count.  A rule of a nested inductive names a
constructor of an earlier group.  Such a name stays a reference check only,
and the frozen corpus holds such rules.  Four reader negatives cover the
new refusals,
so the reader suite prints 120 cases.

Stage B review fix: `mech import --out` accepts an output path that ends
with a path separator.  Before the fix the staging directory landed inside
the missing output directory and the command refused with ENOENT.  The
staging directory now comes from the parent directory and the last
component.  The published artifacts are byte-identical to the run without
the separator, and one CLI case covers the spelling.

Evidence retained under /Users/oobi/Documents/gpt1:
full battery .kanon-exec/run-Z0Ve02; UAT output mechanism-m0b-uat-types;
independent output check .kanon-exec/run-4HT5Zz; mutation artifacts under
mechanism-m0b-mutations.  The installation manifest records source and
canonical baseline hashes, and staging verifies every resulting index blob.

## Stage B review (2026-09-07)

Review round 1 kept seven findings and fixed all seven.

- L1-2 (medium) dev/import-gates.sh:39.  PARITY-COUNTS pinned only the
  declared= column, so a total status regression stayed green.
- L4-3 (medium) test/import_cli.py:55.  The independent artifact checker
  ran on a two-node-kind fixture only, so most of the writer was uncovered.
- L2-1 (low) import/io.ml:12.  `mech import --out DIR/` failed and
  published nothing, because staging landed inside the missing directory.
- L1-3 (low) import/decls.ml:157.  Recursor rules and constructors were
  reference-checked only, so an inconsistent inductive group was accepted.
- L2-6 (low) dev/gates.sh:202.  The IMPORT-CLI oracle accepted any case
  count, so a deleted CLI test block still passed the leg.
- L3-1 (low) test/import_cli.py:41.  Both staged Python tests were
  assert-only with no explicit exit code, against the house rule.
- L1-5 (low) import/ndjson.mli:5.  One space after a sentence in four new
  interface comment lines, against the two-space house rule.

Fixes: dev/import-gates.sh, test/import_cli.py, import/io.ml,
import/decls.ml, dev/gates.sh, test/import_output.py, import/ndjson.mli,
import/export.mli, test/import.ml, SPEC.md, dev/M0-STAGE-B.md,
dev/MUTATION-LOG.md.

Gate verdict: BOUND-ONLY.  Kernel 4140/3000, encoder 246/900.  The
TRUSTED-LINES leg stays red until the user rules D-A-1.
## Stage C foundation (2026-09-07)

Base: 77eda36.  Workflow: separate prelude and mapping builders, an
independent equality feasibility check, source review of the inventory,
axiom gates and constructor overlay, then integration and full validation.
The build copy is `/Users/oobi/Documents/gpt16/mechanism-lang`.
This increment is ready to stage.  Stage C and PRELUDE-CHECKED remain open.

Delivered: nine SMu families and ten definitions in prelude/init.mech,
checked from Global.empty with zero axioms and zero primitives.  The
surface overlay now propagates expected family parameters into constructor
fields, including dependent and nested fields.  Constructor result indices
are computed from their declaration and values, never taken on trust from
the expected result.  All terms still pass through the unchanged kernel.

The inventory records 2,477 referenced external names, 2,543 declared
external names and 3,017 referenced names in total.  Its verdicts are
NAME_ONLY 12, UNMAPPED 2,280 and NEVER 185.  The exact Lean.Omega exception
rows remain in the denominator.  Both TSVs reproduce byte for byte from
the CLI.  No NAME_AND_TYPE judgment is claimed.

Validation: BUILD has zero errors and zero warnings.  All 28 prelude cases
and 14 mapping cases pass.  Positive clients check computation through
singleton indices, not only function types.  Thirteen negative files
require specific diagnostic prefixes.  AXIOMS rejects both an added
postulate and the replacement of the Unit family by postulates.  The old
Stage B driver rejects the new prelude at the parameterized constructor,
confirming that the new source reaches the fixed surface path.

The full battery has 17 PASS legs and one FAIL: TRUSTED-LINES, unchanged
at kernel=4140/3000 and encoder=246/900.  PIN, PIN-DELTA, R0, kernel,
levels, surface, WASM, import, corpus and denominators pass.  No bound,
watchdog tier, frozen denominator or vendored source changed.

| New leg | Tier | Elapsed ms | Exit |
| --- | --- | --- | --- |
| PRELUDE | FAST | 347.758 | 0 |
| AXIOMS | FAST | 106.783 | 0 |
| MAP-INVENTORY | MED | 1770.504 | 0 |

Full gate capture: `/Users/oobi/Documents/gpt16/.kanon-exec/run-Rw7Kuk`.
Old-driver control: `/Users/oobi/Documents/gpt16/.kanon-exec/run-eoLViJ`.
Independent review found no semantic defect.  Its stale surface comment
was fixed, and PIN-DELTA recorded the 79-line diff output of that round.

Remaining work: the kernel refuses data-valued indices in Prop families,
so generic Eq cannot be declared as the plan assumes.  Moving an endpoint
to a field is also refused.  The checked MechProofEq and its dependent J
are restricted to proof endpoints.  Relaxing arbitrary constructor fields
would interact unsafely with proof irrelevance and the erased-field
large-elimination criterion; this increment changes neither rule.
Generic Eq/J/cast, universe-polymorphic families, the category targets,
and source-type mapping judgments remain incomplete.  The trusted-line
bound decision also remains pending.

Stage C review fix round (2026-09-08).  The surface elaborator now reads
the constructor record from the family its expected type names, the way
the kernel introduction rule reads it.  A constructor name that a later
family declares again no longer refuses well-typed source, and the arity,
the field telescope and the result indices come from the expected family.
The name scan stays as the fallback when no expected type is available.
Two prelude cases were added.  One checks a second family that declares
mechInl beside MechSum.  One requires every NAME_ONLY target of
map/prelude.map.tsv to name a checked prelude declaration.  The
empty-environment case now measures Global.empty, the hidden-builtin case
pins its diagnostic, and the indexed-wrong-result expectation holds the
whole measured message.  MAP-INVENTORY compares the CLI output with the
bytes of the checked-in TSV, so a line-ending edit no longer passes and an
ASCII locale no longer fails a correct repository.  The prelude suite
prints 28 PASS lines and PRELUDE-OK families=9 definitions=10 axioms=0
primitives=0.  PIN-DELTA records the measured surface/elab.ml diff of 116
lines.

## Stage C review (2026-09-08)

Seven findings kept and fixed.  L1-1 high, surface/elab.ml: elab_ctor_ref
resolved a constructor by a global name scan and rejected well-typed
prelude uses when a later family declared the same name.  L3-1 medium,
dev/prelude-gates.py: no leg tied a NAME_ONLY target to prelude/init.mech.
L2-1 low, dev/prelude-gates.py: MAP-INVENTORY compared newline-normalised
text.  L4-4 low, test/neg/prelude/indexed-wrong-result.err: the C-M4
expectation stopped before the computed index.  L4-3 low, test/prelude.ml:
the empty-environment case was tautological.  L4-2 low, test/prelude.ml:
reject-hidden-builtin passed on any error.  L4-5 low, dev/MUTATION-LOG.md:
the Stage C row set documented a control for only two of three legs.

Fixes: surface/elab.ml (expected_family, ctor_in, elab_ctor_ref);
test/prelude.ml (map-name-only-targets, empty-environment,
reject-hidden-builtin cases); dev/prelude-gates.py (run_bytes,
byte-compare mapping); test/neg/prelude/indexed-wrong-result.err (full
message); dev/MUTATION-LOG.md (restored Result column); SPEC.md,
dev/PIN-DELTA.md and map/README.md (matching prose and the measured
surface/elab.ml diff of 116 lines).

Gate verdict BOUND-ONLY: seventeen legs PASS except TRUSTED-LINES, kernel
count 4140 of the 4200 allowed, encoder count 246 of the 900 allowed.
TRUSTED-LINES stays red at kernel=4140/3000 and encoder=246/900 until the
user rules D-A-1.

## Stage C textual polymorphic families (2026-09-10)

Base: f7f7c04. The source parser now accepts a single recursive family
after poly universe binders. Its printer round-trips the full family
syntax. The elaborator checks a temporary symbolic family through the
existing Family_poly API, keeps it outside globals and installs only
explicitly specialized closed instances. Definition and family templates
share name reservations, including specialized constructor collisions.
The kernel, vendor pin and programmatic catalogs retain their bytes.

The new fixture checks from Global.empty and installs 13 families and
18 definitions. Five independently specified normal forms cover data,
types, recursive lists, sums and composition with a definition template.
Thirty refusal checks cover scope, arity, malformed constructors,
result-universe stability, collisions, budget and catalog lifetime.
The runtime gate observes payload 37 and recursive-list sum 12 on the
kernel, Node and Wasmtime; changing the payload to 41 changes only that
answer on all three hosts. Axiom output stays empty.

Final validation in /Users/oobi/Documents/gpt5/mechanism-lang:
`env -u OPAM_SWITCH_PREFIX -u CAML_LD_LIBRARY_PATH zsh dev/gates.sh`
prints 31 PASS legs and only FAIL TRUSTED-LINES, kernel=4182/3000 and
encoder=246/900. This is the inherited BOUND-ONLY result, with two added
family legs. No bound or watchdog tier changed. All seven new family
controls and all seven existing definition controls were killed; both
restored suites passed. Details of the initial scope-coverage correction
are in the appended mutation-log block.

Captures, implementation hashes and both mutation reports are retained
under dev/validation/stage-c-prenex-families/. The new slice document is
dev/M0-STAGE-C-PRENEX-FAMILIES.md. Textual family groups and members,
category targets, source-type parity and D-A-1 remain open. This does
not claim Stage C or M0 acceptance.

## Stage C textual polymorphic families review (2026-09-10)

This block records the slice review of the increment above.  The review
runs at ROOT on base f7f7c04, over the same staged paths, and adds no
new bound, tier or gate leg.

The review kept seven items, and all seven are fixed, together with the
two log defects of the fix round.  L2-1 (low) restates the
reservation and specialization rules of SPEC.md against the elaborator.
L2-4 (low) takes the constructor reservation of a family template out
of declaration order: the `names` fold no longer contributes the labels
of a `DPolyMu` group, so a template that repeats a label is refused when
the instance installs it.  L2-5 (low) refuses a family constructor
whose label repeats the family name, with the text `the constructor X
repeats the family name Y`.  L2-6 (low) refuses an instance
constructor whose label repeats the specialized instance name.  L3-1
(medium) pins the error kind of every negative through an exhaustive
match over `Error.t`, the shape of test/prenex.ml.  L3-2 (medium) adds
the negative late-family-template-collision.  L3-3 (medium) adds the budget
negative specialize-budget, whose poll fires inside
`Family_poly.instantiate`.  ND-1-1 (medium) and ND-1-2 (medium) are the
two log repairs, in dev/MUTATION-LOG.md and in this file.

The suite adds the negatives late-family-template-collision,
self-named-constructor, instance-own-constructor and specialize-budget,
and the controls C-PRENEX-FAM-M8, C-PRENEX-FAM-M9, C-PRENEX-FAM-M10 and
C-PRENEX-FAM-M11.  The family replay now holds eleven controls, in place
of the seven the block above records, and the suite runs 30 refusal
checks, in place of 26.  The definition replay keeps its seven controls.
All eleven family controls and all seven definition controls were
killed, and both restored suites passed.

The ladder of this review reports:

```
OK build: 0 errors, 0 warnings
PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30
PRENEX-OK entries=16 computations=4 negatives=25
PRENEX-FAMILIES-RUNTIME OK cases=2 hosts=3 mutation=1
PRENEX-RUNTIME OK cases=2 hosts=3 mutation=1
TRUSTED-LINES kernel=4182/3000 encoder=246/900 FAIL
```

The family replay in
`mechanism-lang-prenex-families-review/mutations-PFAM-2-families`
reports `{"passed": true, "killed": 11, "controls": 11}`, and the
definition replay in the sibling directory mutations-PFAM-2-defs reports
`{"passed": true, "killed": 7, "controls": 7}`.

PIN-DELTA passes with every row at its recorded number, surface/elab.ml
at diff=254, and PIN remains
936a43a92dd59a04698648f24fa5ae94cdb532df.  R0, the inherited suites, the
prelude suites, the axiom and mapping inventories and the three import
modes all pass.  Both map inventories reproduce byte for byte.

The gate verdict is BOUND-ONLY.  The battery prints 31 PASS legs and one
FAIL leg, `TRUSTED-LINES kernel=4182/3000 encoder=246/900`, so it exits
1 and the D-A-1 bound ruling remains open.  The kernel count is 4182 of
the 4200 allowed and the encoder count is 246 of the 900 allowed.  No
bound, tier or watchdog moved.  No commit is created.

The captures under dev/validation/stage-c-prenex-families/ record the
sources of the increment above, before this review edited surface/elab.ml
and test/prenex_families.ml.  They are not re-recorded here.

The round-2 check agent died on an API 529 response.  A hand check then
verified the seven kept items and the two round-1 log items on disk.  All
nine were fixed.  The hand check found one new low defect, ND-2-1: this
block stated the severity of every kept item as the inverse of the run
record.  ND-2-1 is fixed here.  The hand check also carried PFAM-6, a
documentation-only item: the grammar text did not state that a compound
universe level keeps its own parentheses inside a specialize level list.
That sentence is now in SPEC.md and in
dev/M0-STAGE-C-PRENEX-FAMILIES.md.  No parser and no test changed for
it.  The closing battery printed 31 PASS legs and the single FAIL leg
`TRUSTED-LINES kernel=4182/3000 encoder=246/900`, so the verdict stays
BOUND-ONLY at exit 1.  The hand check replays reported families killed
11/11 and definitions killed 7/7.  The suites printed
`PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30`
and `PRENEX-OK entries=16 computations=4 negatives=25`.

## Stage C data equality (2026-09-08)

Base: 44ab7b9.  The prelude now defines MechEq over a Type 0 carrier,
reflexivity, dependent J, transport, symmetry, transitivity and
congruence.  The equality family fixes its left endpoint as an erased
parameter and uses a nullary constructor.  The checker's Prop families
now permit erased data indices; Type index bounds, constructor-field
bounds, proof irrelevance and large elimination retain their separate
rules.  No new primitive, postulate, shape or R0 rule was added.

The prelude audit reports ten families and sixteen definitions, with
zero axioms and primitives.  All 34 prelude cases and 15 raw equality
cases pass.  Dependent clients check J and transport computation, a
proof-dependent motive and a separate higher-carrier cast example.
Precise negatives reject unequal endpoints, erased endpoint reads,
runtime indices, data constructor fields, ineligible large elimination
and a source index above a Type-valued family's universe.

Runtime transport returns 37 for a literal, 2 for a recursive value and
42 for a captured closure on the kernel, Node and Wasmtime.  Changing the
transported payload from 37 to 41 changes the observed results to 41,
2 and 46.  The eraser and emitter require no changes.  The previous
foundation driver rejects the new prelude with the exact old index-bound
diagnostic for MechEq, so the new positive case reaches the changed rule.

The full battery has 19 PASS legs and one FAIL: TRUSTED-LINES at
kernel=4145/3000 and encoder=246/900.  The active kernel grew by five
lines.  PIN, PIN-DELTA, R0, kernel, levels, surface, WASM, import, corpus,
prelude, equality, runtime, axioms, inventory and denominators pass.
The kernel overlay's measured diff output is 83 lines.  The vendor pin,
3,000/900 bounds, existing watchdog tiers and frozen denominators are
unchanged.

Two isolated implementation mutants confirm the raw equality gate's
negative oracles: removing the Type-family index bound fails
type-index-above-bound, and admitting erased data constructor fields
fails prop-erased-data-field.  Each mutant builds cleanly and fails only
its intended case, with the other twelve cases passing.  Details are in
`dev/MUTATION-LOG.md`.

| New leg | Tier | Elapsed ms | Exit |
| --- | --- | --- | --- |
| EQUALITY | FAST | 15.501 | 0 |
| EQUALITY-RUNTIME | MED | 1001.398 | 0 |

Gate capture: `/Users/oobi/Documents/gpt2/.kanon-exec/run-pBSEOX`.
Prelude capture:
`/Users/oobi/Documents/gpt2/mechanism-lang/.kanon-exec/run-CSFod2`.
Baseline and integration manifest:
`/Users/oobi/Documents/gpt2/mechanism-equality-evidence/`.

Seven new mapping candidates bring NAME_ONLY to 19, UNMAPPED to 2,273
and NEVER to 185, with the 2,477 denominator unchanged.  Stage C and
PRELUDE-CHECKED remain open: universe-polymorphic families, general
prelude cast, category targets and checked source-type parity still need
implementation.  The trusted-line ruling remains pending.

## Stage C equality review (2026-09-08)

Two fix rounds.  Eight findings kept, all fixed.  No finding was refused.

- L2-2 (medium), dev/M0-STAGE-C-EQUALITY.md.  Fix marks the M0 large
  elimination as not a discharge of verdict D1.  The subsingleton proof
  for Eq stays an M1 obligation.
- L2-5 (low), dev/gates.sh.  Fix loosens the two frozen leg oracles at
  lines 207 and 208 to a prefix match, so they no longer freeze a
  test-case count.
- L2-4 (low), dev/M0-BUILD-LOG.md, test/neg/prelude/eq-type-index.mech,
  test/neg/prelude/eq-type-index.err, test/prelude.ml.  Fix removes the
  unverifiable review sentence and adds a checked negative pair that pins
  the surface path to Index_above_universe.
- L4-2 (low), test/equality.ml.  Fix adds a data-field-at-bound case, so
  the two constructor-field cases get an accepted-field control.
- L4-1 (low), test/equality.ml.  Fix renames the case to
  index-type-honors-budget and adds a counting-budget case that can fail
  on the staged index_rules short circuit.
- L2-3 (low), dev/M0-STAGE-C.md.  Fix rewords the stale blocker paragraph
  to past tense and drops the eq-data-index reference.
- HV-1-1 (low), test/neg/prelude/eq-type-index.mech.  Fix stages the new
  fixture file, unchanged in content.
- HV-1-2 (low), test/neg/prelude/eq-type-index.err.  Fix stages the new
  error file, unchanged in content.

Gate verdict BOUND-ONLY.  Kernel line count 4145 of the 4,200 ceiling.
Encoder line count 246 of the 900 ceiling.  Nineteen legs PASS.

The TRUSTED-LINES leg stays red until the user rules D-A-1 on the
kernel and encoder bounds.

The Check.index_rules change at lib/check.ml is a documented deviation
from the M0 plan awaiting a user ruling.

## Stage C family templates (2026-09-08)

Base: 0563da5.  The user requested continued development and staging.
The build copy is `/Users/oobi/Documents/gpt6/mechanism-lang`; the canonical
repository was clean at that commit before work began.

Family_poly adds universally checked single-family templates and closed,
renamed, rechecked instances.  Check.check_family_scheme shares the existing
family and constructor rules under a prenex universe context and discards
the symbolic environment.  Closed entry points retain their scope checks.
Empty-family paths now poll budgets.  Kernel rules for indices, fields,
positivity, large elimination and erasure remain unchanged.

The programmatic prelude catalog supplies Eq at an arbitrary carrier Sort
and Sum at two Type levels.  Five closed instances coexist with the source
prelude and share its constructor names.  A source client checks equality,
casts at two carrier universes and sums with unequal universes.  Indexed
witnesses force the cast and sum results to compute.  Both entries and
family records are audited for hidden trusted dependencies.

Validation, with the ambient OPAM_SWITCH_PREFIX and CAML_LD_LIBRARY_PATH
unset and all builds through dev/dunecho.sh:

- Build: zero errors, zero warnings.
- FAMILY-POLY: 34 cases pass, including symbolic counterexamples, recursive
  metadata, scope, collisions, budgets, rollback and rechecking dependencies.
- PRELUDE-POLY: two templates, five instances and two negative clients pass.
- Full battery: 21 PASS legs; only TRUSTED-LINES fails.  Kernel 4182/3000,
  encoder 246/900.  The base measured 4145/3000 and 246/900.
- PIN-DELTA: check.ml measures 140 diff lines against the pin.  The other
  overlay counts, vendor pin, R0 counts, census and denominators are unchanged.
- Independent static review: no reportable defects or weakened gates.
  Raw kernel helpers retain the existing trusted-environment convention;
  the Family_poly interface publishes only closed, rechecked instances.
- Five mutation controls were killed.  A surviving per-Term budget mutant
  exposed a test gap, which was fixed and retested against the same mutant.
  Mutation details are in dev/MUTATION-LOG.md.

Captures under `/Users/oobi/Documents/gpt6/.kanon-exec/`:
baseline `run-jeriKx`; full battery `run-KfzNx5`; prelude clients
`run-XOTH7C`; final strengthened family suite and build `run-avVNFK`.
The full battery preceded the final budget-test strengthening; the final
targeted build and all 34 family cases passed after that test-only change.

No mapping row is promoted: NAME_ONLY remains 19, UNMAPPED 2273 and NEVER
185, over the frozen 2477-name denominator.  Stage C remains open for
textual universe binders, polymorphic library eliminators and cast,
category targets and source-type parity.  The trusted-line ruling and
the previously recorded equality proof obligations remain pending.

Integration checks the canonical base and original file contents, applies
the validated patch to the worktree and index, and creates no commit.
Build captures and mutation copies stay outside the staged source changes.

## Stage C family templates review (2026-09-09)

Findings kept, all verdict fixed.

- L3-1 (medium), lib/check.ml:442.  The kernel arity scope guard had no gate leg and no mutation row.
- L3-2 (medium), test/prelude_poly.ml:26.  The negative oracle pinned only the error constructor and closed with a catch-all arm.
- L4-1 (low), test/prelude_poly.ml:54.  The cross-instance negative was over-determined and could not detect the named conversion.
- L4-3 (low), test/family_poly.ml:283.  The hidden-free-level case did not isolate the ignored shape.
- L2-2 (low), dev/M0-STAGE-C-EQUALITY.md:43.  The scope section still called universe-polymorphic family templates future work.
- L2-1 (low), dev/M0-STAGE-C.md:41.  The remaining-work contract still said universe-polymorphic family templates were unshipped.
- ND-1-1 (medium), dev/M0-BUILD-LOG.md:660.  New defect from the fixes: the block still said 33 family cases, not 34.

Fixes.

- test/family_poly.ml: new refusal case for a free header level; ignored-shape case now isolates the hidden Elim.
- test/prelude_poly.ml: negative oracle rewritten as a prefix check on Error.to_string, catch-all arm removed; cross-instance client changed to a family-name-only mismatch.
- dev/M0-STAGE-C-EQUALITY.md and dev/M0-STAGE-C.md: scope and remaining-work text updated to record the shipped templates.
- dev/M0-BUILD-LOG.md:660: 33 corrected to 34.
- dev/MUTATION-LOG.md: updated for the replayed mutation control.

Gates.  Verdict BOUND-ONLY.  Kernel 4182/3000, encoder 246/900.  Kernel is at or below the recommended 4,200 and encoder at or below 900.  All 21 other legs PASS; only TRUSTED-LINES is red.

TRUSTED-LINES stays red until the user rules D-A-1, the trusted-line bounds.  The programmatic-only template scope is a plan question awaiting a user ruling.

## Stage C polymorphic library transport and cast (2026-09-09)

Base: 43ce94821f9b0229a6b4d562960f2157fc76b9ca.  The user requested
continued development and staging of all changes.  The canonical tree was
clean at entry.  Development and validation ran in
`/Users/oobi/Documents/gpt1/mechanism-m0-c-transport`.

Family_poly now accepts definition members under a shared universe scope.
It checks them in order under the symbolic family and prior definitions,
then stores their syntax in the catalog.  Closed specialization renames
family and member references, substitutes every universe occurrence and
rechecks each member before publishing the immutable updated environment.
Postulates, missing bodies, collisions, invalid scopes and unbound
references are refused.  No kernel or backend source changes.

The separate prelude Equality catalog supplies MechEq with `refl` and
`transport` at independent carrier and motive Sort levels, and MechTypeEq
with `refl` and `cast` at a Type level.  Each catalog checks from
Global.empty.  The original family catalog and monomorphic source prelude
retain their interfaces.  See `dev/M0-STAGE-C-TRANSPORT.md`.

Validation of the final implementation:

- BUILD: zero errors and zero warnings through dev/dunecho.sh.
- FAMILY-MEMBERS: 18 cases pass, including hidden level substitution,
  ordered references, isolated instance names, refusal cases, budgets and
  rechecking under changed globals.
- PRELUDE-TRANSPORT: two templates, seven instances and four negatives
  pass.  Indexed witnesses check cast and transport computation, including
  a dependent motive.  The installed globals contain no axioms, primitives,
  provisional families or builtin families.
- Full battery: 23 PASS legs.  TRUSTED-LINES is the only failure at the
  unchanged kernel 4182/3000 and encoder 246/900.  Both new gate legs pass;
  inherited kernel, levels, surface, WASM, import, corpus, equality runtime,
  axioms, mapping, R0, pin and denominator checks pass.
- Five isolated mutation controls build cleanly and are killed.  Both
  restored suites pass.  Exact controls are in dev/transport-mutations.py
  and the run is recorded in dev/MUTATION-LOG.md.
- Manual diff review found no concrete correctness defect or weakened
  existing gate.  The lexical source audit has no source violation; its
  sole candidate is the word `for` in an interface comment.  No authored
  file contains an em-dash character.

The final battery capture is
`/Users/oobi/Documents/gpt1/.kanon-exec/run-3bmF89`.  A copy of its output,
the source audit and integration fingerprints are retained in
`/Users/oobi/Documents/gpt1/mechanism-transport-validation`.
All build and gate commands cleared OPAM_SWITCH_PREFIX and
CAML_LD_LIBRARY_PATH before selecting the repository's opam wrapper.

No map verdict is promoted: NAME_ONLY remains 19, UNMAPPED 2273 and NEVER
185 over the 2477-name denominator.  Textual universe binders, further
polymorphic equality operations, category targets, source-type parity,
the equality soundness obligations and the trusted-line ruling remain
open.  This increment does not close Stage C or M0.

Integration checks canonical HEAD and file fingerprints, copies the
validated changes, stages all mechanism-lang changes and creates no commit.

## Stage C transport review, fix round 1 (2026-09-09)

The review of the polymorphic transport and cast slice kept seven findings.
Two are medium and five are low.  All seven are fixed in this round.

- L4-1, medium.  The two member budget cases of `test/family_members.ml`
  admitted any extra poll, so a dropped poll on the member path survived.
  Each case now measures the polls of the members-free run, then requires
  the exact count of the same run with one member.
- L2-1, medium.  `SPEC.md` called the polymorphic library cast outstanding
  after this slice shipped it.  The sentence now records the checked
  transport and cast and keeps textual universe binders and source-type
  parity open.  `SPEC.md` is staged with the slice.
- L1-1, low.  A member named like another template was refused with the
  cross-reference diagnostic.  `Family_poly.declare` now refuses such a
  member as a collision before it maps the member type and body.
- L4-2, low.  The fourth negative of `test/prelude_transport.ml` was a
  scope control chosen by a prefix test.  Every negative now carries its
  own scope, and the scope control is named in the test and the documents.
- L4-3, low.  The `wrongUniverse` negative used a proof of another
  instance that the checker never reached.  It now uses its own instance,
  and a new negative feeds a `TransportHigher` proof to `TransportData`.
- L4-4, low.  The PRELUDE-TRANSPORT OK line printed literal counts.  It
  now prints the measured template, instance and negative counts.
- L4-5, low.  Two of the five fixture witnesses repeated the definitions
  they were meant to force.  They are dropped, and a fixture comment names
  the two type annotations that are the Type-level computation witnesses.

Measured in this window: FAMILY-MEMBERS `cases=19`, PRELUDE-TRANSPORT
`templates=2 instances=7 negatives=5`, FAMILY-POLY `cases=34`, PRELUDE-POLY
`templates=2 instances=5 negatives=2`, PRELUDE `families=10 definitions=16
axioms=0 primitives=0`, EQUALITY `cases=15`, `AXIOMS OK prelude=0 fixture=1
hidden_builtins=0`, MAP-INVENTORY OK and EQUALITY-RUNTIME `cases=3 hosts=3
mutation=1`.  The battery prints 23 PASS legs and one FAIL leg,
`TRUSTED-LINES kernel=4182/3000 encoder=246/900 FAIL`, so the verdict is
BOUND-ONLY.  The mutation script reports killed 10, controls 10, with the
five new C-TR rows of `dev/MUTATION-LOG.md`.  PIN-DELTA prints seven OK
rows; no pinned overlay file changed.  Both map inventories reproduce byte
for byte, so NAME_ONLY stays 19, UNMAPPED 2273 and NEVER 185 over the 2477
name denominator.

TRUSTED-LINES stays red until the user rules D-A-1.  No agent moved the
bound or a tier.  The programmatic-only scope of the equality catalog is a
plan question that awaits a user ruling.  No commit is created.

## Stage C transport review (2026-09-09)

Findings kept, all fixed: L4-1 medium test/family_members.ml (member budget
poll controls); L2-1 medium SPEC.md (drops the stale outstanding-cast
sentence); L1-1 low surface/family_poly.ml (refuses a member name that
collides with a catalog key); L4-2 low test/prelude_transport.ml (names the
fourth negative a scope control); L4-3 low test/prelude_transport.ml (adds a
mixedInstance negative, fixes wrongUniverse); L4-4 low test/prelude_transport.ml
(OK line prints measured lengths, not literal counts); L4-5 low
test/fixtures/prelude/transport.mech (drops two redundant witnesses).

Fix paths: test/family_members.ml, dev/transport-mutations.py,
dev/MUTATION-LOG.md, dev/M0-STAGE-C-TRANSPORT.md, SPEC.md,
surface/family_poly.ml, surface/family_poly.mli, prelude/README.md,
test/prelude_transport.ml, test/fixtures/prelude/transport.mech.

Gate verdict BOUND-ONLY.  TRUSTED-LINES kernel=4182/3000 encoder=246/900.
FAMILY-MEMBERS cases=19.  PRELUDE-TRANSPORT templates=2 instances=7
negatives=5.  Mutation controls killed 10 of 10.  Fix round 2 found no new
defect; GATE-1 reproduced the same BOUND-ONLY result and is closed as
verified, not fixed.

TRUSTED-LINES stays red until the user rules D-A-1.  The programmatic-only
catalog scope stays a plan question awaiting a user ruling.  No commit is
created.

## Stage C polymorphic equality operations (2026-09-09)

Base bd92df54f1f69417c3503fe39a6c5d1bc2888d71 was clean in the canonical
repository.  The user requested continued development and staging of all
changes.  The build copy is
`/Users/oobi/Documents/kanon-inference/mechanism-lang-equality-ops`.
The vendor checkout and gitlink remain at 936a43a.

The Equality catalog adds MechEq members j, symm, trans and congr, and
MechTypeEq members symm and trans.  All six definitions check under the
existing symbolic family checker, then check again at specialization.
Congruence has one shared carrier Sort level.  No kernel, surface,
importer, mapping inventory, pin, trusted bound or watchdog tier changed.

Validation commands use `env -u OPAM_SWITCH_PREFIX -u CAML_LD_LIBRARY_PATH`
with the repository's `dev/dunecho.sh` and `dev/gates.sh` runners, under
kanon-wait and kanon-exec.  BUILD reports zero errors and zero warnings.
PRELUDE-EQUALITY-OPS reports `instances=10 computations=11 negatives=8`.
Every negative has an accepted counterpart in the same scope.  The
normalization checks compare data constructors directly, independently
of the indexed typing witnesses and proof conversion.

The full battery in
`/Users/oobi/Documents/kanon-inference/.kanon-exec/run-59ebLU` reports 24
PASS legs and one FAIL leg:
`TRUSTED-LINES kernel=4182/3000 encoder=246/900 FAIL`.  Its exit is 1.
PIN, PIN-DELTA, R0, kernel, levels, surface, WASM, import, corpus, parity,
prelude, equality, family templates, members, transport, equality runtime,
axioms, mapping inventory and denominators pass.  The new equality-ops
leg took 90.566 ms in that run.  This is validation timing, not an R3
performance claim.  Stage C and PRELUDE-CHECKED remain open.

Nine isolated controls in `dev/equality-ops-mutations.py` all build with
zero errors and zero warnings and fail with the expected diagnostic.
The final run is
`/Users/oobi/Documents/kanon-inference/mechanism-equality-ops-mutations-2`;
its `results.json` reports passed true, killed 9, controls 9.  The
restored suite passes all ten instances, eleven computations and eight
negatives.  The first run rejected the J mutant with a quantity error
instead of the anticipated mismatch; only that diagnostic expectation
was corrected before the final run.  See `dev/MUTATION-LOG.md`.

The final diff retains every existing gate and adds PRELUDE-EQUALITY-OPS.
The transport inventory assertion now includes the six added members.
The remaining Stage C work, the D-A-1 bound ruling and the
programmatic-only catalog question are recorded in the slice document.
Integration rechecks the canonical base and original files before
applying and staging the validated diff.  No commit is created.

## Stage C equality operations review (2026-09-09)

Round 1 fixed L1-2 (EQ-3, the j-base negative pinned name resolution) in
test/prelude_equality_ops.ml, L3-1 (EQ-2, no control on the J elimination
motive) in dev/equality-ops-mutations.py and dev/M0-STAGE-C-EQUALITY-OPS.md,
and L2-2 (the fixture header) in test/fixtures/prelude/equality-ops.mech.
Round 2 fixed HV-1-1 and HV-1-2: the round 1 edits inside the earlier
blocks of this log and of dev/MUTATION-LOG.md were restored to their
staged text, and this block holds the corrections.

Corrections to the block "Stage C polymorphic equality operations
(2026-09-09)" above.  "Nine isolated controls" is now ten, because the
review added C-OPS-M10.  The final mutation run is
`/Users/oobi/Documents/mechanism-lang-eqops-review/probes/mutations-EQ-1`
and its `results.json` reports passed true, killed 10, controls 10.
"killed 9, controls 9" names the earlier run
`/Users/oobi/Documents/kanon-inference/mechanism-equality-ops-mutations-2`
only.  `computations=11` stays true, because no computation was added.

Measured in the round 2 window: BUILD `OK build: 0 errors, 0 warnings`;
`PRELUDE-EQUALITY-OPS-OK instances=10 computations=11 negatives=8`;
`PRELUDE-TRANSPORT-OK templates=2 instances=7 negatives=5`; the full
battery reports 24 PASS legs, then `FAIL TRUSTED-LINES` with
`TRUSTED-LINES kernel=4182/3000 encoder=246/900 FAIL`, then GATES-FAIL.
The gate verdict is BOUND-ONLY with kernel 4182 and encoder 246.
TRUSTED-LINES stays red until the user rules D-A-1.  The
programmatic-only catalog scope is a plan question awaiting a user
ruling.  No commit is created.

## Stage C polymorphic dependent functions and pairs (2026-09-09)

Base: d59008b.  Build copy:
`/Users/oobi/Documents/gpt2/mechanism-dependent`.
The canonical repository was clean at that commit before this work.
The older equality build copy under gpt2/mechanism-lang was preserved.

Added Dependent.catalog with six independent Poly templates: MechPi,
MechSigma, mechSigmaMk, mechSigmaFst, mechSigmaSnd and mechSigmaRec.
Pi uses independent Sort levels with imax; pairs use independent Type
levels with max, and elimination has an independent motive Sort.
The templates check from Global.empty and closed instances check again.
No kernel source, rule, primitive or postulate was added.

Validation uses dunecho with OPAM_SWITCH_PREFIX and CAML_LD_LIBRARY_PATH
unset.  BUILD reports zero errors and zero warnings.  The new suite reports:

```text
PRELUDE-DEPENDENT-OK templates=6 instances=29 computations=12 negatives=6
```

Generic contracts check exact result universes, dependent fibers and a
motive indexed by the whole pair.  Independent normal forms and indexed
source witnesses check computation, and every negative has an accepted
counterpart.  The environment audit rejects unexpected trusted entries.

The full battery capture is
`/Users/oobi/Documents/gpt2/.kanon-exec/run-vIdElZ`.
It reports 25 PASS legs, including PRELUDE-DEPENDENT, and only the inherited
TRUSTED-LINES failure: kernel=4182/3000, encoder=246/900.  GATES-FAIL is
therefore expected.  The new leg took 27.973 ms, informational only.
Counts, bounds, watchdog tiers, vendor pin, mapping verdicts and
denominators are unchanged.

All nine mutation controls compiled cleanly and were killed in
`/Users/oobi/Documents/gpt2/mechanism-dependent-mutations-2`.
Its results.json reports passed true with nine killed controls, and the
restored suite passes.  The first run counted seven kills because M1 and
M2 hit the universe guard instead of the anticipated mismatch guard.
Only those diagnostic expectations were corrected; no implementation or
behavioral gate was weakened.  See dev/MUTATION-LOG.md.

The final diff adds PRELUDE-DEPENDENT and keeps every prior gate.
Integration checks the canonical HEAD and original contents before copying
and staging the validated files.  Stage C, source-type parity, the D-A-1
ruling and the programmatic-only catalog plan question remain open.
No commit is created.

## Stage C dependent functions and pairs review (2026-09-09)

Base: d59008b.  Review work directory:
`/Users/oobi/Documents/mechanism-lang-dep-review`.
The round changed only the suite, the fixture, the mutation script and the
prose.  No kernel, surface or backend source was touched, and no gate
tier, bound or oracle moved.

PRELUDE-DEPENDENT now refuses an exhausted caller budget.  The suite calls
`Dependent.catalog` with `Budget.of_poll (fun () -> true)` and pins
`budget: the check budget is exhausted`.  The instances figure is measured
in the installed environment as the sum of the entries each
`Poly.instantiate` adds, and the suite refuses a figure that differs from
the number of instantiations.

The wrong-result negative became `wrong-pi-body`.  It checks an accepted
Pi body and then an invalid one, so a MechPi instance carries a negative
for the first time.  The wrong-domain, dependent-step and wrong-pi-body
negatives pin the whole measured refusal, including the expected type, so
no two of them accept the same line.  In the fixture the Up witness now
checks a value against `UpSnd`, so that type-valued projection must
compute to the carrier.

Validation uses dunecho with OPAM_SWITCH_PREFIX and CAML_LD_LIBRARY_PATH
unset.  BUILD reports zero errors and zero warnings.  The suite reports:

```text
PRELUDE-DEPENDENT-OK templates=6 instances=29 computations=12 negatives=6
```

The mutation script gained C-DEP-M10 for the budget thread, and five
controls now pin their measured line instead of the shared mismatch
prefix.  The replay
`/Users/oobi/Documents/mechanism-lang-dep-review/probes/mutations-DEP-3`
reports `{"passed": true, "killed": 10, "controls": 10}`.  The block above
counts nine controls; ten is the count after this round.  See
dev/MUTATION-LOG.md.

TRUSTED-LINES stays red until the user rules D-A-1: kernel=4182/3000,
encoder=246/900.  GATES-FAIL is therefore still expected, and every other
leg passes.  No commit is created.

Close record.  Six findings were kept and all six are fixed.  One is
medium and five are low.

- L4-1, medium, test/prelude_dependent.ml:69.  The wrong-result negative
  was an ascription control, and no negative used MechPi.
- L4-2, low, test/prelude_dependent.ml:64.  Three negatives pinned only
  the "term has type" half of the refusal.
- L3-1, low, dev/dependent-mutations.py:48.  Five controls pinned only
  the generic mismatch prefix.
- L1-1, low, test/prelude_dependent.ml:81.  No case and no control
  observed the ?budget thread of Dependent.catalog.
- L4-3, low, test/prelude_dependent.ml:153.  The templates figure of the
  OK line was arithmetic over the test's own list.
- L4-4, low, test/fixtures/prelude/dependent.mech:55.  The upComputes
  witness pinned no computed field.

The fixes touch these paths:

- test/prelude_dependent.ml
- test/fixtures/prelude/dependent.mech
- dev/dependent-mutations.py
- dev/MUTATION-LOG.md
- dev/M0-STAGE-C-DEPENDENT.md
- prelude/README.md
- dev/M0-BUILD-LOG.md

Gate verdict: BOUND-ONLY.  All other legs pass.  The counts are
kernel=4182 and encoder=246.  The battery log is
`/Users/oobi/Documents/mechanism-lang-dep-review/gates-DEP-1-items.log`.

## Stage C independent-universe congruence (2026-09-09)

Base: a67c035.  Added ordered Family_poly groups and Congruence.catalog.
The group shares a universe scope, checks families and members in order,
renames every internal reference, and rechecks closed instances atomically.
The prelude maps domain equality to codomain equality at independent Sort
levels, including Prop.  The kernel and pinned vendor sources are unchanged.

Build copy: `/Users/oobi/Documents/gpt2/mechanism-congruence`.
Validation uses `env -u OPAM_SWITCH_PREFIX -u CAML_LD_LIBRARY_PATH` and the
repository dunecho runner.  Final full battery artifact:
`/Users/oobi/Documents/gpt2/mechanism-congruence/.kanon-exec/run-mP9fjB`.
All behavioral legs pass, including FAMILY-GROUPS (21 cases) and
PRELUDE-CONGRUENCE (8 instances, 4 computations, 4 paired negatives).
The unchanged TRUSTED-LINES gate is the only failure: kernel=4182/3000,
encoder=246/900.  No bound or acceptance condition was relaxed.

Seven isolated mutation controls compiled cleanly and were killed by their
pinned test failures.  Both restored suites passed.  Replay:
`/Users/oobi/Documents/gpt2/mechanism-congruence-mutations-2/results.json`.
The first replay rejected all seven mutants, but C-CONG-M1 expected a
universe-error prefix instead of the observed type mismatch.  The final
replay pins the measured diagnostic and reports 7/7 controls killed.

The diff was inspected for companion and member collisions, symbolic name
escape, raw level substitution, ordered dependencies and closed rechecking.
Existing single-family and member suites pass.  Textual universe binders,
category targets and source-type parity remain open.  Validated files are
staged after checking the canonical base and contents; no commit is created.

## Stage C family groups and congruence review (2026-09-09)

Base: a67c035.  Review work directory:
`/Users/oobi/Documents/mechanism-lang-cong-review`.
The round changed only the suites, the mutation script and the prose.  No
kernel, surface or backend source was touched, and no gate tier, bound or
oracle moved.  The staged prose uses two spaces after each sentence.

FAMILY-GROUPS gained the case `member-template-reference`, which pins the
refusal of a member type that names another template.  PRELUDE-CONGRUENCE
gained the negative `wrong-level`, which pins `mismatch: the term has type
Type 2 and the expected type is Type 1` next to its accepted Up
counterpart.  Every negative pin now carries the printed term and the
printed expected type, so no pin is a prefix of another.  The two atomic
failure cases require that the caller globals still accept a fresh
instance after the refusal.  The budget refusal is pinned by the budget
diagnostic instead of a catch-all arm.

Validation uses dunecho with OPAM_SWITCH_PREFIX and CAML_LD_LIBRARY_PATH
unset.  BUILD reports zero errors and zero warnings.  The suites report:

```text
FAMILY-GROUPS-OK cases=22
PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=5
```

The mutation script gained C-CONG-M8 (accept a template reference from a
group member) and C-CONG-M9 (drop the occupied check of the install
fold).  The replay
`/Users/oobi/Documents/mechanism-lang-cong-review/probes/mutations-CONG-1`
reports `{"passed": true, "killed": 9, "controls": 9}`.  The block "Stage C
independent-universe congruence (2026-09-09)" above reports 21 cases, 4
paired negatives and seven controls.  Those numbers name the run of that
round.  The totals after this round are 22 cases, 5 negatives (four paired
plus one wrong-level) and nine controls.  See dev/MUTATION-LOG.md.

TRUSTED-LINES stays red until the user rules D-A-1: kernel=4182/3000,
encoder=246/900.  The gitlink vendor/kanon 936a43a is unchanged.

Review summary.  The review kept nine findings, and every one is fixed:
L4-1 medium (the four negative oracles could not separate one misuse from
another), L2-1 medium (one space after a sentence in the staged prose),
L1-1 low (no control on the `declare_group` name closure), L3-2 low (no
control on the install-fold occupied check), L4-3 low (no negative for a
universe mismatch), L4-4 low (two vacuous "input changed" requires),
L4-5 low (a catch-all arm on `Error.t`) and HV-1-1 low (the wrap width of
`dev/M0-STAGE-C-CONGRUENCE.md`), and ND-1-1 medium (stale counts in this
log, found after the first fix round).  Four findings are refuted: L4-2,
L2-2, L2-4 and L1-2.  Three findings are dropped: L3-1 merges into L1-1,
L2-3 merges into L4-4, and L1-3 is cut at the seven-finding cap.

Two fix rounds ran.  Round 1 applied seven items and left three
documentation leftovers.  Round 2 applied those three: the document half
of L4-3, HV-1-1 and ND-1-1.  No kernel, surface or backend file changed
in either round.

The gate verdict is BOUND-ONLY at load `1:22  27 users, load averages:
52.41 50.48 46.97`.  Every leg passes except TRUSTED-LINES, which reports
`TRUSTED-LINES kernel=4182/3000 encoder=246/900 FAIL`.  The TRUSTED-LINES
leg stays red until the user rules D-A-1.  The replay of round 1 reports
`{"passed": true, "killed": 9, "controls": 9}` for C-CONG-M1 to
C-CONG-M9.  Round 2 changed documents only, so the replay was not rerun.

PIN-DELTA passes, and the seven measured rows equal the expected column
of `dev/PIN-DELTA.md` lines 42 to 48:

```text
lib/check.ml diff=140 expected=140 OK
lib/conv.ml diff=51 expected=51 OK
lib/rules.ml diff=47 expected=47 OK
lib/level.ml diff=49 expected=49 OK
lib/level.mli diff=17 expected=17 OK
bin/kanon.ml diff=14 expected=14 OK
surface/elab.ml diff=116 expected=116 OK
```

The staged total before this summary is 17 files changed, 841 insertions
and 16 deletions.  The staged total after it is 17 files changed, 882
insertions and 16 deletions.

## Stage C textual prenex definitions (2026-09-10)

Base: c9869ef, the committed congruence slice.  The canonical tree and
index were clean before this increment.  Work and validation used
`/Users/oobi/Documents/gpt11/mechanism-lang`.

The source language now accepts `poly (u, v) def`, named Sort levels with
succ, max and imax, and explicit closed `specialize` declarations.  The
lexer, token, syntax and parser overlays retain the inherited grammar
and add those forms.  The source elaborator carries an immutable Poly
catalog for one program check.  It checks each template universally and
rechecks each closed instance through the existing API.  Only closed
instances become ordinary global entries and output rows.  No kernel,
runtime, vendor or mapping file changes in this increment.

PRENEX reports `entries=16 computations=4 negatives=25`.  It checks from
Global.empty, audits entries, checks source-order output and confirms that
templates stay outside globals.  Round-trips preserve the parsed tree,
including a successor expression above the numeric-atom digit limit.
The fixtures exercise Prop, data and type levels, independent universes,
max, imax and annotated application during symbolic elaboration.

PRENEX-RUNTIME reports `cases=2 hosts=3 mutation=1`.  The original value
is 37 and its mutation is 41; the closure returns 12 in both variants.
The kernel, Node and Wasmtime agree.  Each variant also passes check,
axioms and emission.  The source mutation replay kills seven of seven
controls with clean builds and a passing restored suite.  Details and
the first replay's coverage repair are in dev/MUTATION-LOG.md.

Baseline battery: `.kanon-exec/run-0Vzf8j` under the build copy.  It has
27 PASS legs and only FAIL TRUSTED-LINES.  Final battery:
`.kanon-exec/run-ZgcXMJ` under that same copy.  It has 29 PASS legs and
only FAIL TRUSTED-LINES.  The final build has zero errors and warnings.
Both batteries report `kernel=4182/3000 encoder=246/900` and exit 1.
The existing D-A-1 bound ruling remains open.  No limit, watchdog tier
or prior gate leg was removed or weakened.

PIN remains 936a43a92dd59a04698648f24fa5ae94cdb532df.  PIN-DELTA passes
with surface/elab.ml at 198 lines and the new token, lexer, syntax and
parser overlays at 8, 4, 23 and 118 diff-output lines respectively.
R0, inherited kernel, surface, WASM, importer, every prior prelude suite,
axiom inventory, mapping inventory and denominators all pass.

Textual polymorphic families, category targets, checked source-type
parity and PRELUDE-CHECKED remain open.  Validated files are applied and
staged only after rechecking the canonical HEAD and original contents.
No commit is created.

## Stage C textual prenex definitions review (2026-09-10)

This block records the slice review of the increment above.  The review
runs at ROOT on base c9869ef, over the same staged paths, and adds no
new bound, tier or gate leg.

The review found a reservation gap: a constructor name sat outside every
occupancy check, so a template or an instance could take the name of a
constructor and become unreachable.  The source elaborator now adds every
constructor of a mu group to the program reservation, and refuses a
template name or a specialization target that repeats a constructor of a
family already in globals.  The refusal text `the name NAME is already
declared` is unchanged.  PRENEX adds the negatives constructor-collision,
constructor-template-collision and constructor-instance-collision.
SPEC.md and dev/M0-STAGE-C-PRENEX.md now record that a plain `def` keeps
the inherited kernel allowance and can repeat a constructor name, before
or after the family.  That allowance is a property of the kernel at PIN,
and this review does not change it.

PRENEX reports `entries=16 computations=4 negatives=25` and
PRENEX-RUNTIME reports `cases=2 hosts=3 mutation=1`.  The review replay
in `mechanism-lang-prenex-review/probes/mutations-PRENEX-2b` reports
`{"passed": true, "killed": 7, "controls": 7}`, with a restored suite of
`PRENEX-OK entries=16 computations=4 negatives=25`.

PIN-DELTA passes with every row at its recorded number, and PIN remains
936a43a92dd59a04698648f24fa5ae94cdb532df.  R0, the inherited suites, the
prelude suites, the axiom and mapping inventories and the three import
modes all pass.  Both map inventories reproduce byte for byte.  The final
battery keeps its PASS legs, and its only FAIL is TRUSTED-LINES with
`kernel=4182/3000 encoder=246/900`, so the battery exits 1 and the D-A-1
bound ruling remains open.  No commit is created.

The review kept ten items, and all ten are fixed.  L1-1 (low) refuses the
reserved universe names succ, max and imax in the universe binder list.
L1-2 (low) reports a malformed `poly` or `specialize` declaration at its
own keyword.  L1-4 (low) records that neither decl printer emits the text
that the lexer reads back as one Unit token.  L2-1 (low) is the
reservation gap above.  L3-4 (low) makes the C-PRENEX-M7 oracle the full
measured refusal, not a prefix of it.  L4-2 (low) counts the refusal
checks that run, in place of a hand-kept sum.  L4-3 (low) drops the
computation row that held no specialized template.  ND-1-1 (medium) and
ND-1-2 (medium) are the two log repairs, in dev/MUTATION-LOG.md and in
this file.  GATE-1 (high) is the battery, which is red at the bound only.

The fixes touch surface/parser.ml, surface/elab.ml, surface/syntax.ml,
test/prenex.ml, test/fixtures/prelude/prenex.mech,
dev/prenex-mutations.py, dev/PIN-DELTA.md, dev/MUTATION-LOG.md,
dev/M0-STAGE-C-PRENEX.md, SPEC.md and dev/M0-BUILD-LOG.md.

The review refutes L2-2, L3-3 and L3-5, and drops L1-3, L3-1, L3-2, L4-1
and L4-4 at the seven item cap.

The gate verdict is BOUND-ONLY.  The battery prints 29 PASS legs and one
FAIL leg, `TRUSTED-LINES kernel=4182/3000 encoder=246/900`.  The kernel
count is 4182 lines against a bound of 3000, and the encoder count is 246
lines against a bound of 900.  No fix in this review moves a bound, a
tier or a trusted line count.  The TRUSTED-LINES leg stays red until the
user rules D-A-1.

## 2026-09-10: textual ordered groups and members

Base: d4dc0c4.  Build copy: /Users/oobi/Documents/gpt12/mechanism-lang.
Canonical repository: /Users/oobi/Documents/mechanism-lang.  The source
now accepts ordered `and` families and `where ... end` member definitions
under one universe binder scope.  Specialization rechecks the complete
group, checks all generated names against both catalogs and the instance
name, and includes members in output order.
No kernel, encoder, pin or mapping verdict changes.

The build has zero errors and warnings.  PRENEX-GROUPS passes with
13 families, 23 entries, seven normal forms and 36 precise refusals.
The runtime test passes on the kernel, Node and Wasmtime, including the
37-to-41 payload change and the unchanged 12 result.  Eleven group,
eleven family and seven definition mutation controls pass.

The initial integration run exposed the old PRENEX expectation that an
ordered group must fail.  That row now checks the explicit mutual syntax,
which remains unsupported.  The new suite supplies ordered-group positive
clients.  One group mutation initially failed to match its designated
diagnostic.  The full measured error was recorded, and the full group
replay then passed.  No mutation was counted as killed by a build failure.

The final full battery has 33 PASS legs and one FAIL leg:
`TRUSTED-LINES kernel=4182/3000 encoder=246/900`.  The clean base has
the same bound failure and 31 PASS legs.  The watchdog tiers and bound
constants remain unchanged.  Stage C, category targets, source-type
parity and the D-A-1 ruling remain open.  The diff review checked
namespace reservation, declaration order, budget propagation, generated
rows and the absence of changes to trusted sources or gate limits.

See dev/M0-STAGE-C-PRENEX-GROUPS.md and
dev/validation/stage-c-prenex-groups/ for the contract and retained evidence.

## Stage C textual ordered groups review (2026-09-10)

Base: d4dc0c4.  The round fixes seven findings of the staged increment.
The specialize path now checks every installed constructor label against
the family and definition names of the caller globals, and it keeps a
name that the globals already hold as a constructor label, because one
template installs its labels again at every instance.  The member reader
of `surface/parser.ml` accepts `def rec` into its declaration arm, so a
recursive member reports `expected a nonrecursive member definition`
instead of naming `def` as unexpected.  The group elaborator drops a
`Family_poly.declare` call whose result it discarded, because
`Family_poly.declare_group` runs the same universal check; the group
suite pins the cost against the same two families declared as separate
templates.  The printer doc comment of `surface/syntax.ml` now scopes
the round-trip invariant to the trees the parser builds, because a group
with no companion and no member prints as a plain single family.  The
runtime harness names the gate of the mode that ran when it aborts.

PRENEX-GROUPS prints 13 families, 23 entries, seven normal forms and 39
precise refusals.  Three refusals are new:  a later label equal to an
earlier instance name, a later label equal to an earlier generated
member name and a group companion named after an existing constructor.
PRENEX-FAMILIES and PRENEX prints are unchanged at 30 and 25 refusals.
The `--groups` mutation replay has thirteen controls.  The measured
overlay rows move to surface/elab.ml 324, surface/syntax.ml 49 and
surface/parser.ml 151 in dev/PIN-DELTA.md.  PIN remains
936a43a92dd59a04698648f24fa5ae94cdb532df.  No kernel, encoder, bound,
tier or mapping verdict changes.

## Stage C checked categories (2026-09-10)

Base: 1f5d7bf. The category prelude adds independent object and morphism
universes, identity, composition and three checked laws. The supporting
kernel changes preserve captured elimination environments during quotation,
type dependent projection motives and compare constructor indices under
their family telescope. The evaluator is a physical overlay; vendor/kanon
remains pinned at 936a43a92dd59a04698648f24fa5ae94cdb532df.

The build reports zero errors and warnings. PRELUDE-CATEGORY checks 47
definitions, seven families, four instances, seven exact normal forms,
two quotation round trips and nine refusals. All nine mutation controls
were killed and the restored suite passed. Tighter diagnostic prefixes
were checked against the saved outputs and current source hashes, with
9/9 verified. Evidence is in `dev/validation/stage-c-category/`.

The initial battery had 35 passing legs and only TRUSTED-LINES failed.
The final battery had 34 passing legs, the same bound failure and a
30-second PRELUDE-CATEGORY-RUNTIME watchdog expiry. Its isolated retry
under the same 30-second limit passed: two exports, three hosts and a
payload-change control. No source change or timeout increase was needed.
Together the final battery and scoped retry cover all 35 behavioral legs.
The active counts are kernel=4208/3000 and encoder=246/900, compared with
kernel=4182/3000 at the base. The bound ruling remains open.

The passing runtime leg uses concrete record projections. Generic category
accessors compute in the kernel but trap on Node and Wasmtime. The separate
`--category-accessors` probe confirms eight host failures and is preserved
for the next emitter slice. This boundary is not claimed as passing WASM
parity. Functor, NatTrans, LeftKanExtension and source-type parity remain
open. See `dev/M0-STAGE-C-CATEGORY.md` for the full contract.

A second round answers the two items of the first check.  The 85 column
line of dev/M0-STAGE-C-PRENEX-GROUPS.md is rewrapped into three lines of
at most 72 columns, and no word changes.  The review increment gains its
own receipt in dev/validation/stage-c-prenex-groups/receipt-review-1.json
at base d4dc0c4, which records the baseline and gates verdicts, the
mutation counts of 13 groups, 11 families and seven definitions, the
group suite counts of 13 families, 23 entries, seven normal forms and 39
refusals, the runtime hosts and payloads, the trusted line counts, 25
source hashes taken from the index and eight evidence hashes over the
sibling captures.  The receipt of the first increment is untouched.

Three sentences of dev/M0-STAGE-C-PRENEX-GROUPS.md are corrected at the
close.  One paragraph is rewrapped, the count of isolated controls moves
from eleven to thirteen with a description of the two new controls, and
the mutation total moves from 29 controls with 11 group controls to 31
controls with 13 group controls.  The review raised 10 findings, kept and
fixed seven of them, and dropped three:  one merged into the generated
name finding, one merged into the recursive member finding and one cut at
the finding cap as a doc comment style point.  The frozen TRUSTED-LINES
bound is not a defect of this increment, because the clean base carries
the same red.

The close ladder repeats the suites, the three mutation replays and the
full battery:

    PRENEX-GROUPS-OK families=13 entries=23 computations=7 negatives=39
    PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30
    PRENEX-OK entries=16 computations=4 negatives=25
    PRENEX-GROUPS-RUNTIME OK cases=2 hosts=3 mutation=1
    PRENEX-FAMILIES-RUNTIME OK cases=2 hosts=3 mutation=1
    PRENEX-RUNTIME OK cases=2 hosts=3 mutation=1
    replay groups rc 0 {"passed": true, "killed": 13, "controls": 13}
    replay families rc 0 {"passed": true, "killed": 11, "controls": 11}
    replay defs rc 0 {"passed": true, "killed": 7, "controls": 7}
    battery PASS=33 FAIL TRUSTED-LINES  EXIT 1

The one FAIL leg reads `TRUSTED-LINES kernel=4182/3000 encoder=246/900
FAIL`, which is the bound the clean base also fails.  The verdict of the
close is bound only.  PIN remains
936a43a92dd59a04698648f24fa5ae94cdb532df, and no kernel, encoder, bound,
tier or mapping verdict changes.

## Stage C checked categories review (2026-09-10)

Base: 1f5d7bf. Four finders reported 16 raw findings. Verification
kept 14 of them. The judge kept seven findings and dropped seven.
Four fix rounds and three checks follow. Ten items are fixed in all:
the seven kept findings, one house violation and two defects that the
checks found. Nothing is open.

L1-1 (medium). The order of the fresh index binders in the new
quotation arm of lib/eval.ml had no control, so a full reversal of
that order still passed the suite. The fix adds the witness pair
indexedMotive and indexedMotiveClosed in
test/fixtures/prelude/category.mech, whose motive reads both an index
binder and the self binder, and control C-CAT-M10 in
dev/category-mutations.py on the anchor of the binder list. The
replay kills ten of ten controls, C-CAT-M10 included.

L2-1 (medium). The erased-endpoint negative exercised no category
rule, and its pin matched any erased binder named n. The case now
reads an erased `Small_Hom Point End point point` binder, with the
.err repinned from the new run and the eight-case count kept. A
control that renames Small_Hom turns the suite red with
`PRELUDE-CATEGORY-FAIL erased-endpoint: unbound: Small_HomX`.

L2-4 (medium). The PRELUDE-CATEGORY-RUNTIME leg was the slowest
medium leg and had expired the 30 s watchdog once. The
first round of the review removed the `check` of the mutation variant
and both `axioms` invokes, which gave 17 subprocesses, and the leg
stayed the slowest medium leg. The mutation variant now runs only the
export whose answer depends on the mutated definition, because the
second composition gives 12 with and without the mutation. The
category modes now make no separate `check` pass, because the first
`emit` makes the same source pass and prints a kernel refusal on a
source error. The concrete mode makes twelve subprocesses: eight for
the original variant and four for the mutation variant. An alternating
measurement of six runs of each harness, at load averages from 19 to
22, gives a wall median of 11.84 s before and 7.45 s after, which is
63 percent of the earlier time. The printed leg line, the tier, the
watchdog ceiling and the trusted bounds stay as they were. The
accessor probe keeps both mutated exports and still reports eight host
failures.

L2-5 (low). Two of the four counts in the suite line were not
measured from the tree: the negatives inventory was a literal name
list, and the quotation count was a format string literal. The suite
now reads the directory test/neg/category, compares the sorted names
with the list, and counts the quotation witnesses from the list that
it folds. Controls that add and that drop one negative both print
`PRELUDE-CATEGORY-FAIL category negative inventory changed`.

L2-6 (low). The accessor mode could not separate the documented host
boundary from a kernel regression. The mode now labels the kernel
side checks apart from the host checks. The real tree prints
`PRELUDE-CATEGORY-ACCESSORS FAIL host_checks=8 failing_checks=8`, and
a control with a broken accessor fixture prints
`PRELUDE-CATEGORY-ACCESSORS FAIL kernel_checks=3 failing_checks=3`.
No gate leg is added and no tier moves.

L3-2 (low). The staged replay report recorded the kill predicates
from before the controls were narrowed, so the stricter verification
could not be rechecked inside the repository. The file
dev/validation/stage-c-category/mutations.json now records the
narrowed diagnostics and the C-CAT-M10 row, and results.json with ten
stdout and stderr captures is staged. A `--verify` run on a copy of
the staged directory prints `{"passed": true, "verified": 10}`.

L1-3 (low). The contract sold the branch address round trip of the
trusted kernel as a kernel correction, but it is inert at M0 and has
no control. dev/M0-STAGE-C-CATEGORY.md now says that the round trip
is defensive and is the identity on the M0 addresses. lib/eval.ml is
untouched, and pin-delta.sh still reports `lib/eval.ml diff=24
expected=24 OK`.

HV-1-1 (house violation). The documented `--verify` example wrote
`verification.json` inside the repository. The example now copies the
evidence directory out of the repository first, so the mode writes
only outside the repository, and a new paragraph says where the file
lands. The control run on a copy exits 0 with
`{"passed": true, "verified": 10}`, and the new file compares equal to
the committed mutation-verification.json. The verify sentences in
dev/MUTATION-LOG.md and dev/M0-STAGE-C-CATEGORY.md are made true
against the new example.

ND-1-1 (new defect, round 1). The committed file
`dev/validation/stage-c-category/generic-accessors.log` was stale. It
is restaged from a current run and ends
`PRELUDE-CATEGORY-ACCESSORS FAIL host_checks=8 failing_checks=8`,
which is the split that the harness prints, and the README bullet
names that final line.

ND-3-1 (new defect, round 3). The comment that the round-3 remedy
added said that the second category export does not read the mutated
definition, which is false, because the fixture passes categoryInput
to it. The comment in test/prenex_runtime.py and its two echoes in
dev/validation/stage-c-category/README.md and in this block now say
that the mutation variant runs the export whose answer depends on the
mutated definition. The harness still prints
`PRELUDE-CATEGORY-RUNTIME OK cases=2 hosts=3 mutation=1`, exit 0.
Round 4 also rewrapped the prose lines of 73 to 76 columns in the two
staged documents, and none remains.

The counts of the increment changed in the first review round. The
category suite reports 49 entries, seven computations, nine refusals
and three quotation checks. The mutation replay holds ten controls.

The judge dropped seven findings:

- L2-3: merged into L2-5, the same file and the same defect class.
- L1-2: cut by the cap, a stale doc comment only, and its fix moves a
  measured row for no functional gain.
- L2-2: cut after the harm was refuted, only diagnostic text
  precision remains.
- L3-1: cut after downgrade, the mutation script refuses a non-unique
  anchor, so no false kill exists today.
- L4-1: cut after downgrade to cosmetic, one cross-reference gap in
  one document.
- L4-2: cut, whitespace only, no written rule and no gate effect.
- L4-3: cut after the 72-column norm was refuted for the sibling
  contract documents.

The review ladder gives:

```
OK build: 0 errors, 0 warnings
TRUSTED-LINES kernel=4208/3000 encoder=246/900 FAIL
PIN-DELTA OK with gitlink vendor/kanon 936a43a
R0-COUNT OK, R0-AUDIT OK
PRELUDE-CATEGORY-OK entries=49 computations=7 negatives=9 quotation=3
PRENEX-GROUPS-OK families=13 entries=23 computations=7 negatives=39
PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30
PRENEX-OK entries=16 computations=4 negatives=25
PRELUDE-CATEGORY-RUNTIME OK cases=2 hosts=3 mutation=1
replay rc 0 {"passed": true, "killed": 10, "controls": 10}
FAMILY-GROUPS-OK cases=22
PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=5
PRELUDE-TRANSPORT-OK templates=2 instances=7 negatives=5
FAMILY-MEMBERS-OK cases=19
FAMILY-POLY-OK cases=34
PRELUDE-POLY-OK templates=2 instances=5 negatives=2
EQUALITY-OK cases=15
PRELUDE-OK families=10 definitions=16 axioms=0 primitives=0
PRELUDE-AXIOMS OK
MAPPING-OK
EQUALITY-RUNTIME OK cases=3 hosts=3 mutation=1
AXIOMS OK prelude=0 fixture=1 hidden_builtins=0
MAP-INVENTORY OK
IMPORT-TYPES-OK, CORPUS-OK, PARITY-COUNTS OK
battery PASS=35 FAIL TRUSTED-LINES  EXIT 1
```

Both map inventory reproductions compare equal to map/prelude.map.tsv
and to map/NEVER.tsv. The separate generic accessor probe exits 1 with
eight host failures, which stays the documented open boundary.

The closing battery has 35 PASS legs. The replay prints
`{"passed": true, "killed": 10, "controls": 10}` and the verify mode
prints `{"passed": true, "verified": 10}`. The measured rows are
`MEASURE PRELUDE-CATEGORY tier=FAST elapsed_ms=1342` and
`MEASURE PRELUDE-CATEGORY-RUNTIME tier=MED elapsed_ms=3421`, at a load
average of 21 to 24. The tree holds 67 paths, 1698 insertions and 27
deletions, and `git diff --check` is clean.

One residual stays open as a cost, not as a defect. The cost of the
PRELUDE-CATEGORY-RUNTIME leg is intrinsic, because each emit and each
kernel run elaborates the template of 49 entries. The leg measured
3.4 s in the closing battery and 7 s to 18 s under load averages of 20
to 27. The MED ceiling of 30 s therefore keeps a margin that gets
smaller under a heavy load. The SLOW tier is available if the leg ever
expires.

The one red leg is TRUSTED-LINES, which the clean base also fails, so
the verdict of the review is bound only. PIN remains
936a43a92dd59a04698648f24fa5ae94cdb532df, and no kernel, encoder,
bound, tier or mapping verdict changes.
