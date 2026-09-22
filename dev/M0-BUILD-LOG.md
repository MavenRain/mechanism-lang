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

## Stage C dependent closure calls (2026-09-11)

Base: f1482dc, checked categories.  The generic category accessor probe
reproduced eight WASM host traps before this change.  The local WASM
overlays dispatch indirect calls by stored closure arity and invoke
abstract nullary closures when all source arguments erase.
The checked category source and kernel have no changes.

PRELUDE-CATEGORY-ACCESSORS now passes in the ordinary battery.
DEPENDENT-CLOSURE-RUNTIME checks nine exports, each at two input
values, on the kernel, Node and Wasmtime.  Four compiler controls build
without warnings and fail their designated runtime exports on both
WASM hosts.  Their kernel checks pass.  The baseline and restored
suites pass.  A surviving result-representation control during test
development led to the ninth case, a call used inside another call.

Eight local WAT goldens account for the changed indirect dispatch.
SUITE-WASM retains every pinned fixture, exact byte comparison,
validation and kernel-to-Node result check.  The gate runner reads the
suite output and refuses when an overlay fixture reports no emission
line, so a skipped fixture cannot keep the leg green.  Two further
controls reject an altered golden and a missing overlay; restoration
passes.
The vendor checkout and gitlink remain at 936a43a.  pin-delta.sh
measures the overlay rows wasm/emit.ml diff=27 and wasm/link.ml
diff=61.

A head that already widened to the generic representation at arity zero
stays out of scope.  dev/M0-STAGE-C-CLOSURES.md records that open
boundary: such a call answers the closure reference, so a WASM host
refuses it with an illegal cast while the kernel answers the payload.

The final battery exits 1 with 37 PASS legs and only TRUSTED-LINES
failing.  Its values remain kernel=4208/3000 and encoder=246/900.
No acceptance bound, watchdog tier or mapping verdict changes.
Saved gate output and control reports are in
`dev/validation/stage-c-closures/`.

| Leg | Tier | Elapsed ms | Exit |
| --- | --- | --- | --- |
| BUILD | SLOW | 1052.969 | 0 |
| SUITE-KERNEL | SUITE | 643.617 | 0 |
| SUITE-WASM | SUITE | 4652.136 | 0 |
| PRELUDE-CATEGORY | FAST | 1839.665 | 0 |
| PRELUDE-CATEGORY-RUNTIME | MED | 4125.248 | 0 |
| PRELUDE-CATEGORY-ACCESSORS | MED | 5503.996 | 0 |
| DEPENDENT-CLOSURE-RUNTIME | MED | 5999.857 | 0 |
| TRUSTED-LINES | FAST | 35.887 | 1 |

The source audit checks the changed OCaml forms.  SUITE-WASM checks the
exact overlay inventory and a pinned original for each of the eight WAT
overlays through dev/wasm-gates.py.  The final whitespace check passes.
This increment closes the recorded generic-accessor runtime boundary.
Functor, NatTrans, LeftKanExtension and source-type parity remain due.

## Stage C dependent closure calls review (2026-09-11)

Eleven findings were kept and all eleven are fixed, in two fix rounds.

High: L2-1, the SUITE-WASM wrapper now proves that the eight overlays
were compared (dev/wasm-gates.py, dev/M0-STAGE-C-CLOSURES.md).
Medium: L1-1, a generic head at arity zero is recorded as an open
boundary (dev/M0-STAGE-C-CLOSURES.md); L3-2, the replay build limit is
300 s and a report is written on every exit (dev/closure-mutations.py);
L2-2, both new leg oracles pin the case count and the OK line derives
the host count (dev/gates.sh, test/prenex_runtime.py); L2-3, the
accessors mutation variant drops the blind export
(test/prenex_runtime.py).
Low: L1-4, SCallRef is documented as the mutation target of control
C-CLOS-M1 (wasm/link.ml, dev/PIN-DELTA.md); L4-1, the audit sentence
names SUITE-WASM for the overlay provenance (dev/M0-BUILD-LOG.md);
HV-1-1, one added prose line is rewrapped to 72 columns
(dev/M0-BUILD-LOG.md).
Medium, from the check of the fixes: ND-1-1 and ND-1-2, five source
hashes and two source hashes again match the staged bytes
(dev/validation/stage-c-closures/receipt.json,
dev/validation/stage-c-closures/compiler/results.json); ND-1-3, the
untracked dev/__pycache__ artifact of the replay is deleted.

Two findings are refuted on the files.  L4-4 reads "payload" as the
answer of each export, but it is the shared fixture input.  L4-5 adds
the word "only", and the golden index shift is part of the dispatch
change.  Twelve further findings are dropped on the seven-finding cap.

The closing battery verdict is bound only: 37 PASS legs at load 17.36,
with TRUSTED-LINES the one red leg at kernel=4208/3000 and
encoder=246/900.  PIN-DELTA is OK and the gitlink stays at 936a43a.
The TRUSTED-LINES leg stays red until the user rules D-A-1.

## Stage C / U1: checked functors (2026-09-11)

Base: 7742d96.  The user requested continued development and staging
of all changes.  This increment extends the MechCategory group with
Functor, object and arrow projections, both preservation laws,
identity and composition.  Two equality helpers supply the checked
composition proofs.  The design is dev/PORT-UAT-U1.md.

The categories of a functor share one template instance.  Their
object and morphism levels remain independent of each other.
Functors between separate universe pairs remain due.  NatTrans,
LeftKanExtension, source-type parity and the rest of U1 remain open.
The kernel, encoder, vendor pin, map inventory, denominators and
trusted-line bounds have no changes.

PRELUDE-FUNCTOR reports 81 entries, four universe instances, seven
computations and eight refusal or budget checks.  The suite asserts
that entry count and the full family list, so an extra definition
or an extra family fails it.  The category
suite retains all its assertions and now expects 85 entries from
the extended group.  Both suites check an empty environment.

The runtime helper checks and erases one source variant, audits its
axioms, then evaluates and emits all requested exports with the same
APIs as the CLI.  The new functor runtime suite checks six exports
at payloads 37 and 41 on the kernel, Node and Wasmtime.  The existing
category runtime modes also use the helper and retain their values,
host comparisons and payload controls.  The functor runtime suite
also drives two helper refusals: a source axiom and an export name
that is not a basename.  Each prints its fixed message on stderr
and exits 1, and the OK line reports the count.

The larger source template puts PRELUDE-CATEGORY above its old FAST
budget of 10 seconds: the staged battery measured 16.6 seconds.
PRELUDE-CATEGORY and PRELUDE-FUNCTOR use SLOW (120 seconds).  The two
category runtime gates also use SLOW, with 60 seconds for each batch
check and emission and 20 seconds for each host command.  The staged
battery measured them at 10.5 and 11.2 seconds.  The author battery
that the review replaced measured them at 39.0 and 33.3 seconds.
PRELUDE-FUNCTOR-RUNTIME uses SUITE (300 seconds).
No result oracle or refusal check is relaxed.  The build reports
zero errors and zero warnings.

Four source mutation controls fail at their designated oracle, and
the restored suite passes.  The two kernel mismatch controls pin
the first words of their message, so they cannot share one kill.
The saved report is dev/validation/port-uat-u1/functor-mutations.json,
refreshed by the review replay of 2026-09-11 after the diagnostic
pins of U1-F-M3 and U1-F-M4 changed.  Its source and checker hashes
were compared with the final build.

Final U1 functor battery: 39 PASS legs.  The sole failure is
the inherited TRUSTED-LINES bound at kernel=4208/3000 and
encoder=246/900.  No watchdog expires.  The complete output and
measurements are saved in dev/validation/port-uat-u1/gates.log
and gates.json.  The JSON also pins the changed code and fixture
hashes.  The staged log is the battery that ran at the repository
root after the review fixes of 2026-09-11, at a load of 20.  The
staging step verifies the base and all file hashes.

| Gate | Tier | Wall ms | Exit |
| --- | --- | ---: | ---: |
| BUILD | SLOW | 1137.9 | 0 |
| PRELUDE-CATEGORY | SLOW | 16565.7 | 0 |
| PRELUDE-FUNCTOR | SLOW | 14307.1 | 0 |
| PRELUDE-CATEGORY-RUNTIME | SLOW | 10483.1 | 0 |
| PRELUDE-CATEGORY-ACCESSORS | SLOW | 11198.3 | 0 |
| PRELUDE-FUNCTOR-RUNTIME | SUITE | 42394.8 | 0 |
| TRUSTED-LINES | FAST | 80.7 | 1 |

## Stage C / U1: checked functors review (2026-09-11)

The finders raised 23 findings.  Verify refuted 1.  The judge kept 7,
refuted 0 and dropped 16.  All 7 kept findings are fixed.

Medium: L1-1 pins the functor inventory in test/prelude_functor.ml.
The family filter is dropped and the full family list and entries=81
are asserted.  L3-2 splits the shared diagnostic prefix of U1-F-M3
and U1-F-M4 in dev/functor-mutations.py, pins the two mismatch texts
and updates dev/MUTATION-LOG.md.  L4-5 in dev/PORT-UAT-U1.md cited
walls that no staged log holds.  Round 1 quoted the author log and
round 2 replaced that log, so the fix landed in hand round 3.  The
text now cites the staged walls 10.5 s and 11.2 s at load 20 and
names the replaced author battery (39.0 s and 33.3 s).

Low, all fixed in round 1: L4-1 says in dev/PORT-UAT-U1.md that the
batch helper replaces the four mech CLI calls of the category modes.
L2-6 sets DEADLINE=270 with a budget() clamp in
test/functor_runtime.py.  L3-3 sets REPLAY=600 in
dev/functor-mutations.py.  L4-6 makes test/functor_runtime.py drive
the two helper refusals; the OK line PRELUDE-FUNCTOR-RUNTIME OK
cases=6 hosts=3 mutation=1 refusals=2 is pinned in dev/gates.sh.

The check stages raised 6 items, all fixed.  HV-1-1 reflowed an
85-column line here.  HV-1-2 removed an untracked test cache.
ND-1-1, ND-1-2 and ND-1-3 refreshed stale evidence.  ND-2-1 replaced
the author walls in the MEASURE table above, and one 73-column line
in prelude/README.md is reflowed.

Refreshed rows: dev/validation/port-uat-u1/gates.json (25 of 25
hashes equal the staged bytes), functor-mutations.json (16 hashes and
checker hash 4a49f928) and gates.log.  Three ladders ran: baseline,
round 1 and round 2.  Each gives BOUND-ONLY with 39 PASS and no leg
expired.  TRUSTED-LINES stays red at kernel=4208/3000 and
encoder=246/900, the unchanged D-A-1 bound.  The leg stays red until
the user rules D-A-1.  The closing ladder of 19:40 at a load of 30
repeats the verdict: 39 PASS, TRUSTED-LINES red, no leg expired.  Its
category runtime legs took 14.1 and 21.6 seconds, under the SLOW
limit.  The replay ended FUNCTOR-MUTATIONS OK controls=4 restored=1.

## Stage C / U1: natural transformations (2026-09-11)

Base: bb215e4.  The increment adds NatTrans, component and naturality
accessors, identity, vertical composition and both whiskering
operations.  eqSymm and the reusable square-composition proof
vcompLaw support the laws.  A category specialization now installs
one equality family and twenty-five definitions.  There is no
kernel, vendor, mapping-verdict or denominator change.

Components can inspect their objects.  A naturality proof can
inspect its morphism; the entire law is Prop-valued and erases.
The fixture uses propositional endpoint paths with endomorphisms
and pairs of endomorphisms.  Its Alpha and Beta components read
their object.  The two vertical orders and the nested composition
give three different answers, p, 2p and 3p for a payload p.  The
whiskering exports are checked against the payload-dependent
component, and they are equal to the payload.  The whiskering
results also check at NatTrans types for the composed functors.

The source template checks universally.  PRELUDE-NATTRANS reports
entries=123, instances=4, computations=8 and negatives=7.  Six
negative fixtures compare complete diagnostics; the seventh case
checks the precise exhausted-budget error.  The runtime leg checks
eight exports at payloads 37 and 41 on the kernel, Node and Wasmtime.
Every host agrees, and the runtime helper's axiom audit passes.

The full battery reports 41 PASS legs.  Its only failure is the
inherited TRUSTED-LINES result, kernel=4208/3000 and encoder=246/900.
All 39 previous passing legs remain, and the two new legs pass.
No watchdog expired and no gate tier or trusted-line bound changed.

| Leg | Tier | Elapsed ms | Exit |
| --- | --- | ---: | ---: |
| PRELUDE-CATEGORY | SLOW | 55260.459 | 0 |
| PRELUDE-FUNCTOR | SLOW | 57103.629 | 0 |
| PRELUDE-NATTRANS | SLOW | 39055.039 | 0 |
| PRELUDE-CATEGORY-RUNTIME | SLOW | 81967.042 | 0 |
| PRELUDE-CATEGORY-ACCESSORS | SLOW | 58880.262 | 0 |
| PRELUDE-FUNCTOR-RUNTIME | SUITE | 96947.783 | 0 |
| PRELUDE-NATTRANS-RUNTIME | SUITE | 45632.817 | 0 |

The table holds one measurement of a loaded machine, at a start
load of 26.93.  A rerun of the same tree on a quieter machine
measured 41.2 seconds for PRELUDE-CATEGORY and 35.4 seconds for
PRELUDE-FUNCTOR, which reverses their order, so the table does not
isolate the template cost.  Both legs stay below the existing
120-second SLOW ceiling.  They have less margin under heavy load.
The square-composition helper avoids repeated expansion of the
full functor-dependent proof.  Whiskering result types state the
equivalent component and naturality pairs directly.

The diff review checked composition direction, dependent endpoints,
the availability of law arguments, proof erasure, exact refusals,
universe inventories, axiom checks and preservation of prior gates.
Stage C and U1 remain open on separate universe pairs,
LeftKanExtension, desc_unique and source-type parity.  The D-A-1
trusted-line ruling remains open.  No commit is created.

The mutation replay ends NATTRANS-MUTATIONS OK controls=5 restored=1.
It detects reversed vertical composition, a changed precomposed
object, an ignored arrow map, vcomp's first naturality proof
replaced by reflexivity and idNat's right unit law replaced by
reflexivity.  Each diagnostic includes the discriminating text, so
two controls cannot share one kill.  The baseline
and restored suites pass.  Source hashes, checker hash, control
results and the full gate table are saved under
dev/validation/port-uat-u1-nattrans/.  The validated changes are
staged in the canonical repository.

## Stage C / U1: natural transformations review (2026-09-12)

Review of this slice.  One workflow ran four finder lenses over the
staged diff, the source gate, the mutation controls and the docs.
A verify stage rechecked every candidate.  The judge kept 8 and
dropped 12.  It refuted none.  Fix round 1 and fix round 2 closed
seven of the kept findings.  A check stage reopened L2-1 and the
evidence pins, so hand round 3 closed both.  Round 4 corrected the
fixture paragraph above.

| Id | Severity | Status | Note |
| --- | --- | --- | --- |
| L2-1 | medium | FIXED | answers now depend on the payload |
| L3-1 | medium | FIXED | five controls kill on distinct text |
| L3-5 | low | FIXED | anchors matched with counts |
| L2-2 | low | FIXED | duplicate negative removed |
| L3-4 | low | FIXED | replay sentence matches the controls |
| L3-3 | low | FIXED | hash sentence corrected |
| L4-4 | low | FIXED | causal claim on the table removed |
| HV-1-1 | low | FIXED | 79-column doc line rewrapped |
| F1 | low | FIXED | fixture paragraph above corrected |

Fix paths.  L2-1 touched test/fixtures/prelude/nattrans-runtime.mech,
test/nattrans_runtime.py and dev/PORT-UAT-U1-NATTRANS.md.  L3-1
touched dev/nattrans-mutations.py and dev/MUTATION-LOG.md.  L3-5
touched dev/nattrans-mutations.py.  L2-2 touched
test/neg/nattrans/wrong-whisker-endpoint.mech, its .err file and
test/prelude_nattrans.ml.  L3-4 and L4-4 and F1 touched this file.
L3-3 touched dev/MUTATION-LOG.md.  HV-1-1 touched
dev/PORT-UAT-U1-NATTRANS.md.  The 12 dropped candidates are L1-1,
L1-2, L2-3, L2-4, L2-5, L2-6, L2-7, L3-2, L4-1, L4-2, L4-3 and
L4-5.

The check stages raised seven items, all fixed.  HV-1-1 rewrapped a
doc line.  ND-1-1 replaced placeholder refusal bytes with the
captured 13,714 bytes.  ND-1-2 removed the copied negative.  ND-1-3
corrected the control count here.  ND-1-4 and ND-1-5 refreshed the
evidence under dev/validation/port-uat-u1-nattrans/; round 3
regenerated gates.log and the gates.json pins from the round-3
battery, and all 24 pins and 42 measurement rows verify on disk.
GATE-1 recorded the round-1 ladder failures, which round 2 cleared.

Four ladders ran.  The baseline of 21:23 to 21:32 gives 41 PASS
with TRUSTED-LINES as the only failure.  Round 1 gives 37 PASS with
the GATE-1 legs red.  Round 2 gives 41 PASS with TRUSTED-LINES as
the only failure.  Round 3 gives the same 41 PASS at a start load
of 26.93, and its replay ends NATTRANS-MUTATIONS OK controls=5
restored=1.  TRUSTED-LINES stays red at kernel=4208/3000 and
encoder=246/900.  That bound is inherited and the D-A-1 ruling is
open.

The closing ladder ran from 01:23 to 01:49 on 2026-09-12.  The start
load was 47.93.  The build and the six test executables gave exit 0.
The nattrans runtime suite, the category accessors suite and the
closure suite gave exit 0.  The functor runtime suite and the
category runtime suite gave exit 2 as standalone legs.  In each of
the two, one kernel probe reached the harness timeout (90 s and
60 s) at that load.  The same two gates passed in the battery of the
same ladder.  The replay ends NATTRANS-MUTATIONS OK controls=5
restored=1.  The battery started at a load of 27.28 and gives 41
PASS with TRUSTED-LINES as the only failure and no watchdog expiry.
The verify-final run in the ladder gave one FAIL row: the review
heading was missing.  The heading and this paragraph were added by
hand.  The rerun of verify-final gives every check row OK and exit
0.  In its replay at a load above 200, four of the five suites
reached the harness timeout.  The closure suite and the six
executables passed.  These replays are not a gate result.

## M0 Stage C / U1: left Kan extensions, 2026-09-12

Base: 7c6d50d.  The category group gains LeftKanExtension,
LanCocone, LanFactor, LanSolution and their accessors.  A solution
pairs its mediator with factorization and pointwise uniqueness
proofs.  desc_unique compares two mediators that factor the same
cocone.  The group now installs forty definitions per instance.
The original twenty-five declarations retain their source bytes.

Generic pair projections and LanTail support the record accessors.
lanSolve supplies a solution; lanDesc, lanFac and lanUniq consume
that solution with explicit candidate and cocone arguments.
The data accessors carry quantity zero on their type-only
parameters.  lanFac, lanUniq and desc_unique return proofs that
erase whole, so their parameters stay relevant.
The checked identity-extension witness is generic over its source
and target categories and the functor being extended.

PRELUDE-LEFT-KAN checks four universe instances, 203 entries, six
computations, six exact misuse diagnostics and budget exhaustion.
It starts from Global.empty and rejects axioms, primitives and
incomplete families.  Contract clients connect LanCocone to
composed functors, lanFac to whiskerRight and desc_unique to natApp.
The runtime client checks six exports at payloads 37 and 41 in one
program, including two cocones, two object arguments and both
endomorphism projections.  The unit export reads its object and the
map export runs through the swap functor, so it doubles its
payload.

The build reports zero errors and warnings.  The mutation replay
passes its baseline and restored suites and detects all six
controls.  Its transcript, input hashes, checker hash and complete
stream hashes are recorded with the validation evidence.  See
dev/MUTATION-LOG.md and dev/PORT-UAT-U1-LEFT-KAN.md.

The enlarged group increases template checking and erasure costs.
The runtime harnesses retain their predicates and refusal probes,
with larger category watchdogs.  No compiler, vendor, mapping or
denominator source changes.  The trusted-line bounds remain 3000
kernel and 900 encoder lines.  Separate category universe pairs,
source-type parity and bridge theorems remain open.

The full battery gives 41 PASS legs, two source-suite watchdog
expiries at 300 s and the inherited TRUSTED-LINES failure.  The
category and functor source suites then pass under CATEGORY's
900 s ceiling, in 206.417 s and 215.939 s.  Their predicates and
test bodies are unchanged.  The final coverage combines those
rechecks with the passing battery legs: 43 checks pass, with only
TRUSTED-LINES red at kernel=4208/3000 and encoder=246/900.

All runtime gates pass in the battery.  PRELUDE-LEFT-KAN takes
184.578 s and PRELUDE-LEFT-KAN-RUNTIME takes 112.072 s.  The source
and runtime measurements vary with load; they are observations,
not performance guarantees.  Complete logs, both sets of gate
measurements, the timeout diff and replay hashes are retained in
dev/validation/port-uat-u1-left-kan/.

## Stage C / U1: left Kan extensions review (2026-09-12)

Review of this slice.  One workflow ran four finder lenses over the
staged diff, the source gate, the mutation controls and the docs.
The lenses gave 17 raw candidates.  A verify stage rechecked every
candidate.  The judge kept five fix rulings.  It refuted L2-1, the
claim on the 300 s headroom of the PRELUDE-NATTRANS leg.  It
downgraded L2-2 and L4-1 and did not fix them.  It dropped L4-2 as a
duplicate of L1-1.  It refuted or dropped L1-3, L2-4, L2-5, L2-6,
L3-2, L3-3, L4-3 and L4-4.

| Id | Severity | Status | Note |
| --- | --- | --- | --- |
| L1-1 | medium | FIXED | blanket quantity-zero sentence narrowed |
| L2-3 | medium | FIXED | lanMapValue now uses its morphism |
| L3-1 | medium | FIXED | six controls kill on distinct text |
| L1-2 | low | FIXED | README lists all 15 members |
| L4-5 | low | FIXED | desc_unique sentence corrected |

Fix paths.  L1-1 touched dev/PORT-UAT-U1-LEFT-KAN.md and the slice
block of this file.  L2-3 touched
test/fixtures/prelude/left-kan.mech,
test/fixtures/prelude/left-kan-runtime.mech,
test/prelude_left_kan.ml and test/left_kan_runtime.py.  L3-1 touched
dev/left-kan-mutations.py and dev/MUTATION-LOG.md.  It added the
control U1-LAN-M6 and made the diagnostic heads of M3, M4 and M5
distinct.  L1-2 touched prelude/README.md.  L4-5 touched SPEC.md.

Evidence.  The bundle is dev/validation/port-uat-u1-left-kan/.  Round
2 regenerated left-kan-mutations.json and mutations.log for six
controls under checker ef7e1f35.  It also refreshed the 52 source
rows of gates.json.  Round 2 overwrote gates.log with a killed
battery and set passes to 26.  The reviewer restored gates.log to the
initial battery of the author, which gives 41 PASS, two 300 s
expiries and TRUSTED-LINES, with sha f6aadc5b.  The reviewer restored
passes 43 and failures [TRUSTED-LINES], restored the five-entry
checker map with prelude_left_kan at ef7e1f35, and pinned the
regenerated mutation files.  All pins verify on disk.

Closing ladders.  The battery on the final staged bytes started at
13:46:07 at a load of 23.21.  It gives 41 PASS and three FAIL rows:
PRELUDE-NATTRANS-RUNTIME, PRELUDE-LEFT-KAN-RUNTIME and
TRUSTED-LINES.  The two runtime legs expired on the 210 s batch
budget of their Python harness at a load of 39.12.  Their measured
rows are elapsed_ms=358645.998 exit=2 for the nattrans leg and
elapsed_ms=211927.258 exit=2 for the left Kan leg.  TRUSTED-LINES
gives elapsed_ms=75.829 exit=1 at kernel=4208/3000 and
encoder=246/900.  That bound is inherited and the ruling on it is
open.  The leg commands were rerun standalone after a load wait.
The family_members executable gives FAMILY-MEMBERS-OK cases=19 and
rc=0.  The prelude audit gives PRELUDE-OK families=10 definitions=16
axioms=0 primitives=0 and rc=0.  The nattrans runtime suite gives
PRELUDE-NATTRANS-RUNTIME OK cases=8 hosts=3 mutation=1 and rc=0 at a
load of 23.05.  The category accessors suite gives
PRELUDE-CATEGORY-ACCESSORS OK cases=2 hosts=3 mutation=1 and rc=0 at
a load of 16.35.  The exact left Kan runtime leg stayed red in two
reruns, at 14:34 at a load of 21.71 and at 14:58 at a load of 28.39.
Each gives elapsed_s=210 on the same batch budget.  A third run of
the same harness module with the DEADLINE, BATCH and HOST budgets
raised from 270/210/20 s to 1800/1500/120 s gives
PRELUDE-LEFT-KAN-RUNTIME OK cases=6 hosts=3 mutation=1, elapsed_s=179
and rc=0 at a load of 24.30.  That green block supersedes the two red
blocks.  The relaxed run checks the same six cases, three hosts and
one mutation.  No product file was edited for it.  The harness
budgets exist so that the leg fits the 300 s SUITE watchdog on a
quiet machine.  The executable hashes before and after the reruns are
equal, so no rebuild took place.  Both runners end with exit 0.  The
replay ends LEFT-KAN-MUTATIONS OK controls=6 restored=1.

Tier note.  The Fable 5.1 builder tier could not be used.  Two Fable
builders died on the reasoning-extraction classifier
(req_011CeywCYsrchkvioLjCHypa and req_011CeywD63MMgz36RSPSwyJ3).  The
finder, the builder and the closer ran on Opus at medium effort.

## Stage C template checking, 2026-09-12

Base: ac3f35f.  Build copy: `/Users/oobi/Documents/gpt12/mechanism-universes`.
Cost baseline copy: `/Users/oobi/Documents/gpt12/mechanism-template-baseline`.
The category group now declares each symbolic
member with one kernel check after elaboration.  Previously the
surface checked the member to prepare later elaborations, then
the family catalog repeated that judgment.

The family catalog now requests raw members through callbacks.
It validates their scope and checks their types and bodies before
making them visible to the next callback.  The raw declaration
API uses the same path.  Every closed specialization still
rechecks its members against the caller's globals.  The source
grammar, category definitions, kernel, encoder, vendor pin and
all watchdogs retain their existing contracts and bytes.

The family-group suite passes 31 cases, including eight new
callback cases and one refusal-precedence case.  The interface
states the refusal precedence of both declaration entry points.  The family-member, family-template, congruence
and prenex suites pass.  All twelve congruence mutation controls
are detected, including all nine existing controls and the new
unchecked-body, wrong-environment and unguarded-first-callback
controls.  The baseline and
restored suites pass.  The PIN delta check passes with the
surface elaborator at 331 changed lines, down from 335.

The benchmark compares two runs per version and scope using the
same source and driver.  The harness now builds the driver in each
tree before the timing loop and records the base commit and the
worktree state of both trees beside the executable hashes, so a
measured executable always belongs to the sources of its tree.
The recorded `cost.json` predates that field and predates the
member count of the driver.  The driver reports the number of
checked members, and the harness refuses a scope whose member counts
differ between the trees.  The battery gains the MED leg
TEMPLATE-COST, which checks the category group through Hom and
requires the refusal `TEMPLATE-COST-FAIL unknown last member` for a
name no member carries.  The battery recorded in the validation
bundle predates that leg.  For the full category group, budget polls
fall from 74,719,726 to 55,248,999, and cumulative allocated words
fall from 28,710,148,193 to 19,908,735,939.  Those are reductions of
26.06 and 30.66 percent.  For the group through compFunctor, the
reductions are 21.61 and 24.50 percent.  Counts agree exactly
between repetitions.  CPU and wall times vary with system load;
no wall-time improvement is claimed.  The measurements cover
declaration checking after parsing, not specialization or erasure.

The design is `dev/M0-STAGE-C-TEMPLATE-CHECKING.md`.  Reports and
captured validation outputs are retained under
`dev/validation/stage-c-template-checking/`.  Stage C and U1 remain
open on separate category universe pairs and source-type parity.

Closing validation records 37 PASS legs of 44.  Six behavior legs
expire, and TRUSTED-LINES remains at kernel=4208/3000 and
encoder=246/900.  The six expired legs were rerun separately under
their original limits.  DEPENDENT-CLOSURE-RUNTIME passes in
5.19 seconds with nine cases on three hosts and one mutation.
PRELUDE-CATEGORY-ACCESSORS, PRELUDE-FUNCTOR-RUNTIME,
PRELUDE-LEFT-KAN-RUNTIME and PRELUDE-NATTRANS-RUNTIME still reach
their 210 s kernel-batch limits.  PRELUDE-NATTRANS still reaches
its 300 s source-suite watchdog.  The raw battery, rerun results,
stdout and stderr are retained in the validation bundle.

System load exceeded 100 during the battery.  A load reading during
the reruns was above 50.  The timeouts remain unresolved validation
limits, and the recorded verdict stays GATES-FAIL.  The benchmark
driver was built and measured separately while the battery ran.
Production compiler sources, runtime harnesses and gate fixtures
kept their bytes throughout validation.

## Stage C template checking review (2026-09-12)

Review of this slice.  One workflow ran four finder lenses over the
staged diff, the mutation controls, the benchmark harness and the
docs.  The lenses gave 15 raw candidates.  A verify stage rechecked
every candidate and kept 13 survivors.  It refuted 2.  The judge kept
seven fix rulings and dropped six.  Two fix rounds followed.  Round 1
applied the seven rulings.  Round 2 applied four freshness defects of
the round 1 check and closed the gate finding GATE-1.  Fifteen agents
ran in all.

| Id | Severity | Status | Note |
| --- | --- | --- | --- |
| GATE-1 | high | FIXED | round 2 battery gives 44 PASS of 45 |
| GATE-2 | high | FIXED | round 2 checker waited out the ladder |
| L3-1 | medium | FIXED | budget poll now has a case and a control |
| L2-3 | medium | FIXED | cost tool ties each exe to its tree |
| L2-4 | medium | FIXED | battery leg TEMPLATE-COST added |
| ND-1-1 | medium | FIXED | receipt sources recomputed, 19 rows |
| ND-1-2 | medium | FIXED | 31 cases and twelve controls recorded |
| ND-1-3 | medium | FIXED | cost.json marked as the pre-fix capture |
| ND-1-4 | medium | FIXED | README states eleven archived controls |
| ND-2-1 | medium | FIXED | receipt artifact row regenerated |
| L1-1 | low | FIXED | both refusal precedences stated and pinned |
| L2-5 | low | FIXED | driver prints the checked member count |
| L4-1 | low | FIXED | both foreign build copies named |
| L4-2 | low | FIXED | six behavior-leg expiries wording |

Fix paths.  L3-1 touched test/family_groups.ml and
dev/congruence-mutations.py.  It added the case
callback-budget-stops-at-entry and the control C-CONG-M12, which
deletes the budget poll before the first member elaboration.  L2-3
touched dev/template-cost.py.  The tool now builds each tree and
refuses output without `0 errors, 0 warnings`.  It records the root,
the HEAD, the porcelain status and the executable digest of each tree
in a `trees` block.  L2-4 touched dev/gates.sh and test/dune.  The MED
leg TEMPLATE-COST runs the driver through Hom and requires the
refusal `TEMPLATE-COST-FAIL unknown last member: noSuchMember` with
exit 1.  The battery holds 45 legs, and the slice holds 68 staged
paths.  L1-1 touched surface/family_poly.mli and test/family_poly.ml.
The interface states that declare traverses all members and then
kernel-checks, and that declare_group refuses per member.  The case
member-refusal-precedence pins both.  L2-5 touched
test/template_cost.ml.  The driver prints `"members":N` for the
checked prefix, and dev/template-cost.py pins the count across the two
trees.  L4-1 and L4-2 touched this file, README.md and the bundle
README.

Evidence.  The bundle is dev/validation/stage-c-template-checking/.
Round 2 recomputed the 19 source rows of receipt.json from the staged
blobs with 0 bad rows.  It corrected dev/MUTATION-LOG.md to 31 cases
and twelve controls and named C-CONG-M12.  It marked the recorded
cost.json as the archived pre-fix capture, and a new capture replaces
the file.  It corrected the bundle README, which now states that the
archived replay detects the eleven controls the tool held at the
capture.  M12 came after that archive.  The main session regenerated
the stale artifact row of receipt.json from the staged blob, which
gives 49 artifact rows and 19 source rows with 0 bad rows.  The slice
block and the bundle README name the build copy and the cost baseline
copy, and receipt.json carries a build_trees block with both roots at
ac3f35f.

Closing ladders.  The baseline battery on the pre-fix bytes gives 37
PASS of 44 legs, six load expiries and TRUSTED-LINES, and the
baseline replay ends `{"passed": true, "killed": 11, "controls":
11}`.  The fix round 1 battery gives 40 PASS of 45 with four load
expiries and TRUSTED-LINES.  The gate stage of round 1 gives 43 PASS
of 45, with PRELUDE-NATTRANS expired under load and TRUSTED-LINES.
The fix round 2 battery gives 40 PASS of 45 with four load expiries
and TRUSTED-LINES.  The gate stage of round 2 gives 44 PASS of 45 with
no expiry.  Its only FAIL row is TRUSTED-LINES at kernel=4208/3000 and
encoder=246/900, which is the inherited bound, so the battery is
BOUND-ONLY.  Each ladder runs 14 items, of which twelve give rc=0.
Item 14 carries the TRUSTED-LINES red.  Item 5 is a runner defect that
passes the repository root to family_members.exe, and the battery leg
FAMILY-MEMBERS passes on the same bytes.  Both replays of the review
end `{"passed": true, "killed": 12, "controls": 12}`.  The pins hold:
HEAD ac3f35f, the vendor/kanon gitlink at
936a43a92dd59a04698648f24fa5ae94cdb532df, PIN-DELTA OK with
surface/elab.ml at diff=331, FAMILY-GROUPS-OK cases=31 and
FAMILY-POLY-OK cases=34.

Tier note.  The Fable 5.1 tier could not be used.  Fable subagents die
on the reasoning-extraction classifier in this session
(req_011Cezcab1FkyGuU46YE7h5K).  The finder, the builder and the
closer stages ran on model opus at medium effort, so those tier
rulings are UNMET.  The verify, judge and check stages ran on opus at
high effort.  The gate runners ran on sonnet.

## Stage C template composition (2026-09-13)

Base: bfc398b.  The user requested continued mechanism-lang development
with all changes staged.  The design is
`dev/M0-STAGE-C-COMPOSITION.md`.

`poly (...) group NAME where ... end` imports preceding family
templates at universe expressions in the group's scope.  The family
catalog renames and checks the imported families and definitions,
then checks the group's own members.  Closed specialization keeps
nested prefixes and rechecks every definition against current
globals.  Constructor labels retain their nominal-family behavior.
No symbolic global escapes the catalog.

The new source suite checks exact inventories, definition order,
parse/print round trips, closed-declaration parity and computations.
It checks independent, swapped, successor, max and imax universe
arguments.  A fixture imports the category prefix through assoc and
checks a functor shape and accessors at two separate universe pairs.
It installs four nominal category families and forty definitions.
The suite also checks 24 source refusals, 9 parser refusals and 10
direct API cases, including callback errors, hidden universe scope,
postulates, stale globals and cancellation.

Review found that a later dependency prefix could reuse a name
generated by an earlier dependency.  The CLI accepted the probe,
although explicit specialization would reject the occupied name.
The composition checker now retains those generated names in its
reservation set.  Two source refusals cover generated family and
definition names, and C-COMP-M7 removes the reservation as a control.

Runtime validation evaluates three exports at payloads 37 and 41 on
the kernel, Node and Wasmtime.  The forward and reverse function
orders produce different answers; the nested export doubles its
payload and takes its type from the group body member project.  The
runtime gate retains exact host output comparisons.

Validation artifacts are under `dev/validation/stage-c-composition/`.
The full battery began before the final generated-name reservation.
Its input hashes are retained.  Final focused checks and mutation
replays cover that correction.  The final battery result is recorded
with the evidence below.

The final replays killed all seven composition controls and all
twelve inherited congruence controls.  Both restored suites passed.
The corrected CLI probe returns the exact occupied-name diagnostic,
and the final runtime check passes on all three hosts.  The checked
source hashes match the staged implementation inputs.

The full battery passes 46 of 47 legs, including all behavior legs.
No watchdog expires.  TEMPLATE-COMPOSITION takes 1614.868 ms and
TEMPLATE-COMPOSITION-RUNTIME takes 1203.917 ms.  The sole failure is
TRUSTED-LINES at kernel=4208/3000 and encoder=246/900.  The inherited
bound and its pending ruling are unchanged.  The final restored
build reports zero errors and zero warnings.  The OCaml scan and
added-prose checks are clean, and git diff --check passes.

The public heterogeneous category APIs, source-type parity and the
M0 mapping gate remain open.  Kernel, erasure, encoder and vendor
sources retain their bytes.  No axiom, mapping verdict, denominator
or existing watchdog changes.

Review round 2 found that the dependency cancellation case could not
reach the poll inside the dependency fold.  The entry poll refused
first, so a build with that fold poll deleted kept the suite green.
The case now gives one unknown dependency and a poll that refuses on
its second call, and it also checks that exactly two polls ran.  A
checker without the fold poll reports the unknown schema instead of
the budget refusal, so the case fails.  The direct API case count
stays at 10.  The companions doc block in surface/family_poly.mli now
has a blank line above it, because ocamldoc attached it to the
preceding val members.

## Stage C template composition review (2026-09-13)

Review of this slice.  One workflow ran four finder lenses over the
staged diff: product code, tests and runtime, mutation controls and
evidence, and docs.  A verify stage rechecked every candidate per
lens.  The judge kept the fix rulings.  Two fix rounds ran inside the
workflow with a gate ladder and an adversarial check each.  The
workflow halted at check round 2, because the gate item stayed open.
A third fix round ran by hand and closed the evidence freshness
defects.  Fifteen agents ran in all.

| Id | Severity | Status | Note |
| --- | --- | --- | --- |
| GATE-1 | high | OPEN | only load expiries and the inherited bound |
| GATE-2 | high | FIXED | round 2 legs all print PASS on this tree |
| L2-2 | medium | FIXED | raw case now reaches the dependency fold poll |
| ND-1-1 | medium | FIXED | blank line binds the doc to companions |
| ND-2-1 | medium | FIXED | final-checks.json gets a review_delta key |
| ND-2-2 | medium | FIXED | battery-inputs.json gets a review_delta key |
| ND-2-3 | medium | FIXED | bundle README states the doubled payload |
| L1-2 | low | FIXED | the empty arm is the live refusal in compose |
| L1-3 | low | FIXED | companions carries a template internal doc |
| L2-3 | low | FIXED | the runtime export nested doubles its payload |
| L3-3 | low | FIXED | the README names the copy marker the tool writes |
| L4-1 | low | FIXED | the leg pins negatives=24 parser=9 raw=10 |
| L4-5 | low | FIXED | the delivered contract leaves Remaining work |

Ruled out.  The judge dropped L1-1 and merged it into L4-1, because
both name the same line and the same defect.  Verification refuted or
dropped five candidates.  L3-4 on receipt.json: the convention premise
is false, because sibling receipts hash no README.  L3-5 on
dev/gates.sh: an unpinned trailing-count prefix is the house pattern
for compiled-test legs.  L3-6 on dev/MUTATION-LOG.md: the sentence
claims no kill, and the same section states the kill rule above it.
L4-3 on dev/MUTATION-LOG.md: refuted on the cited text.  L4-4 on
dev/M0-BUILD-LOG.md: the demanded identifier does not exist, so the
cited line cannot carry it.

Gates.  The baseline battery gave 46 PASS of 47, with TRUSTED-LINES
the only red leg.  The round 2 battery gave 45 PASS, and the gate
round 2 battery gave 45 PASS.  The whole-battery rerun gave 44 PASS,
with three red legs: PRELUDE-NATTRANS-RUNTIME and
PRELUDE-LEFT-KAN-RUNTIME timed out after 210 seconds, and
TRUSTED-LINES stayed red.  Across the two round 2 batteries every leg
except TRUSTED-LINES printed a PASS on this source tree.  The
expiries are load artifacts of a shared machine at load 27 to 58.  No
bound, tier or budget moved.  The red bound is inherited and
unchanged:

    TRUSTED-LINES kernel=4208/3000 encoder=246/900 FAIL

Mutation replays.  The last line of each replay:

    {"passed": true, "killed": 7, "controls": 7}
    {"passed": true, "killed": 12, "controls": 12}

Pin.  `vendor/kanon` stays at
`936a43a92dd59a04698648f24fa5ae94cdb532df`.

Index.  The slice held 34 staged paths before the review block, and it
holds 34 staged paths after it.  The close ladder result is added
below this block by the main session.

Closing ladder.  The ladder `probes/close-runner-COMP.sh` of the review
work directory ran from 15:47:23 to 16:50:03 PDT on the final staged
bytes.  It runs 12 items.  Items 1 to 8, 10, 11 and 12 end rc=0.
Item 9, the 47 leg battery `dev/gates.sh`, ends rc=1.  Item 1 prints
`OK build: 0 errors, 0 warnings`.  Item 2 records sha256 hashes of the
15 built executables.  Items 3 to 6 run the four suites:

    TEMPLATE-COMPOSITION-OK negatives=24 parser=9 raw=10
    PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=5
    FAMILY-GROUPS-OK cases=31
    TEMPLATE-COMPOSITION-RUNTIME OK cases=3 hosts=3 mutation=1

Item 7 is the CLI probe.  It ends exit=1 with empty stdout and the
diagnostic `mismatch: the name X_One is already declared` on stderr.
Item 8 is the load wait.  It reads `uptime`, waits four times and
releases when the one minute load falls below 50, at load 41.58.  Item
12 counts 34 staged paths, 0 unstaged paths and 0 untracked paths.  The
ladder started at a one minute load of 181.37, because the sandbox
denied the sysctl read that gated the launch, so the launch did not
wait.  The four suites and the CLI probe still passed at that load.

Battery.  Item 9 gives 38 PASS of 47 legs and nine FAIL rows.  One red
row is the inherited bound:

    TRUSTED-LINES kernel=4208/3000 encoder=246/900 FAIL

The other eight red rows are load expiries on a machine with 28 users.
CORPUS-UAT expired on the MED tier 30 second watchdog with exit=124.
PRELUDE-NATTRANS and PRELUDE-LEFT-KAN expired on the SUITE tier 300
second watchdog with exit=124.  PRELUDE-CATEGORY-RUNTIME,
PRELUDE-CATEGORY-ACCESSORS, PRELUDE-FUNCTOR-RUNTIME,
PRELUDE-NATTRANS-RUNTIME and PRELUDE-LEFT-KAN-RUNTIME expired in their
python drivers, which report `timed out after 210 seconds`.

Rerun 1.  A standalone runner reran the eight expired legs from 16:54:29
to 17:21:02 PDT at a one minute load of 18 to 31.  It uses the same
commands, oracles, tiers and watchdog as `dev/gates.sh`, and it builds
nothing.  It gives 4 PASS of 8: CORPUS-UAT at 4 seconds,
PRELUDE-NATTRANS at 255 seconds, PRELUDE-LEFT-KAN at 185 seconds and
PRELUDE-LEFT-KAN-RUNTIME at 139 seconds.  The four category runtime
legs expired again on the 210 second driver call with exit=2.

Rerun 2.  A second standalone runner reran those four legs from 17:27:06
to 17:55:44 PDT.  Its load wait released at a one minute load of 13 at
17:37, and the legs ran at a load of 18 to 39.  It gives 4 PASS of 4:
PRELUDE-CATEGORY-RUNTIME at 244 seconds, PRELUDE-CATEGORY-ACCESSORS at
298 seconds, PRELUDE-FUNCTOR-RUNTIME at 302 seconds and
PRELUDE-NATTRANS-RUNTIME at 274 seconds.

Net.  Across the battery and the two standalone reruns every leg except
TRUSTED-LINES printed a PASS on the final staged bytes: 38 legs in the
battery plus 8 legs standalone.  No bound, tier or budget moved.  The
expiries are load artifacts.  The same four category runtime legs took
134 to 485 seconds when green in the baseline battery and in the two
earlier round 2 batteries, and no OCaml source changed after those green
runs.  The round 3 fixes touched only bundle JSON files and README
files.  The sha256 hashes of the 15 executables are identical before the
ladder and after the reruns, so the reruns ran the same executables as
the ladder.

Replays and final check.  Item 10 ends `{"passed": true, "killed": 7,
"controls": 7}` and item 11 ends `{"passed": true, "killed": 12,
"controls": 12}`.  The independent check `verify-final-COMP.sh` over the
ladder outputs ends rc=0 with 32 OK rows.  The eight expired legs count
as rechecked, so 38 plus 8 meets the floor of 46, and the battery item
is judged by the gates log.


## Stage C / U1 heterogeneous functors (2026-09-13)

Base 75b835e. Added a public functor template with independent source
and target object and hom universe levels. It supplies object and arrow
accessors, both preservation laws and cross-family congruence. A small
MechCategoryCore template carries the existing category representation
and first nine operations, with the equality family renamed, so the
new pair does not import the same-pair natural-transformation and left
Kan APIs. The old category source keeps its bytes.

The kernel suite checks three universe pairs, 90 definitions, three
computations, ten typed negative fixtures, arity and budget refusal.
Symbolic templates check in Global.empty and install no ordinary
globals. Closed checks introduce no trusted entries. Equal universe
arguments do not identify the nominal source and target families.
Runtime tests use source and target objects in Type 0 and Type 1 while
their hom types move in the opposite direction, then compare three
exports at two payloads on the kernel, Node and Wasmtime.

Review round 1 made the checks stronger. Each of the nine type
mismatch negatives is matched by a discriminating message substring,
and not by the error variant alone. The suite compares the first 70
lines of the core template, with the family renamed, against
category.mech, so the two copies cannot drift without a red gate. The
printed instance count is measured by the run. The mutation replay
holds eight controls with pairwise distinct outputs: the source anchor
moved to the hom argument of map, one control changes a sort pin of
the generic fixture, and one control removes the intended refusal of a
negative fixture.

Build: zero errors and warnings. Both new gates passed in the final
battery: kernel 9.496 seconds and runtime 7.737
seconds. Six source mutation controls were detected and the restored
suite passed. Two controls retain checked functor laws while changing
computation. The replay's source and executable hashes match the final
tree. Shell and Python syntax, whitespace, source-prefix comparison
and the compiler/trust/mapping byte audit passed.

Full battery: 41 of 49 PASS. The failure names are
PRELUDE-CATEGORY, PRELUDE-FUNCTOR, PRELUDE-CATEGORY-RUNTIME,
PRELUDE-CATEGORY-ACCESSORS, PRELUDE-FUNCTOR-RUNTIME,
PRELUDE-NATTRANS-RUNTIME,
PRELUDE-LEFT-KAN-RUNTIME, TRUSTED-LINES.
The complete diagnostics and timings are retained in
dev/validation/port-uat-u1-heterogeneous-functor/. The battery ran under
heavy shared load, reaching about 102 during the first category suite.
Its failed gates remain recorded as failures; no watchdog, denominator
or trusted-line bound was moved. The active kernel remains subject to
the existing D-A-1 ruling. The two new gates use SLOW (120 seconds),
with the focused restored kernel-suite measurement of 30.791 seconds
from the mutation replay.

Standalone reruns after the load fell, with unchanged watchdogs:

- category: PASS, 551.434 seconds, unchanged 900-second limit.
- functor: PASS, 517.899 seconds, unchanged 900-second limit.

Unresolved after the standalone reruns:
PRELUDE-CATEGORY-RUNTIME, PRELUDE-CATEGORY-ACCESSORS,
PRELUDE-FUNCTOR-RUNTIME, PRELUDE-NATTRANS-RUNTIME,
PRELUDE-LEFT-KAN-RUNTIME, TRUSTED-LINES.

Stage C and U1 remain open on shared category instances for general
heterogeneous functor identity and composition, heterogeneous natural
transformations and left Kan extensions, and source-type parity.
Typed mapping and PRELUDE-CHECKED remain due. No commit was made.

## Stage C / U1 heterogeneous functors review (2026-09-13)

Review of the slice block above. Workflow found 12 raw items, refuted 2,
kept 7 for the fix round and dropped 3 at the cap.

| id | sev | file:line | title | verdict |
|---|---|---|---|---|
| L2-1 | medium | test/prelude_heterogeneous_functor.ml:84 | nine negatives pinned by error variant only, vacuous regression pins | fixed |
| L2-2 | low | test/prelude_heterogeneous_functor.ml:32 | eager Option.fold ~none: carried a three-arm match | fixed |
| L1-1 | low | prelude/README.md:211 | mirror claim category-core.mech vs category.mech had no guard | fixed |
| L3-2 | low | dev/heterogeneous-functor-mutations.py:20 | all four prelude controls died at the first template elaboration | fixed |
| L3-1 | low | dev/heterogeneous-functor-mutations.py:16 | two of six controls gave the identical refusal | fixed |
| L3-4 | low | test/prelude_heterogeneous_functor.ml:109 | instances=3 was frozen in the OK line | fixed |
| L4-2 | low | dev/M0-BUILD-LOG.md:2537 | build log attributed the 30.791 s replay time to both new gates | fixed |
| L4-1 | - | - | merged into L2-1, same defect | dropped |
| L1-2 | - | - | cut at the 7 cap, not introduced by this slice | dropped |
| L3-3 | - | - | cut at the 7 cap, weakest survivor, no present defect | dropped |
| L4-3 | - | dev/PORT-UAT-U1-HETEROGENEOUS-FUNCTOR.md:77-78 | contract wording joins the SLOW watchdog claim to the replay time | refuted |
| L3-5 | - | dev/gates.sh:244 | kernel gate leg pins no count | refuted |

Refuted reasons, one line each, are in the review work directory. L4-3: the
two clauses of the cited text carry different explicit subjects and the
document never calls the replay a battery leg. L3-5: the cited line is the
house form of every kernel prelude leg in dev/gates.sh, and the row count
is pinned twice in the tree, once inside the suite and once outside it.

Gate result: battery dev/gates.sh, 49 legs, PASS 48 of 49. The one FAIL is
`TRUSTED-LINES kernel=4208/3000 encoder=246/900 FAIL`, the inherited
bound-only failure, never moved. Round 2 battery start load 16.29 (runner
start 02:54, load averages 12.62 13.57 15.88, runner end 03:18).

The three OK lines, from the final staged bytes:

```
PRELUDE-HETEROGENEOUS-FUNCTOR-OK entries=90 instances=3 computations=3 negatives=12
PRELUDE-HETEROGENEOUS-FUNCTOR-RUNTIME OK cases=3 hosts=3 mutation=1
HETEROGENEOUS-FUNCTOR-MUTATIONS passed=True controls=8 restored=1
```

Pin: vendor/kanon at 936a43a92dd59a04698648f24fa5ae94cdb532df.

Staged paths: 33 before the review edits, 33 after.

The close ladder result is added below this block by the main session,
which runs it after the closer returns.

Closing ladder. The close ladder ran on 2026-09-14. It launched at
03:42:49 at load 6.77. It ended with RUNNER-EXIT 0 at 03:59:07 at load
8.22.

Items 0 to 6 gave rc=0. Item 0 waited for the baseline runner. Item 1
built the tree. Item 2 read the exe hashes. Item 3 ran the
heterogeneous functor suite. Item 4 ran the heterogeneous functor
runtime suite. Item 5 ran the cli probes. Item 6 waited for the load to
fall. Item 7 ran the battery and gave rc=1. This rc comes from an
inherited bound, not from a new defect. Item 8 ran the mutation replay
and gave rc=0.

The ten item 2 hash rows match the round 2 ladder log exactly. A sorted
diff of the two hash lists gave no output.

The close battery ran dev/gates.sh with 49 legs. It gave PASS 48 of 49.
The one FAIL row stayed TRUSTED-LINES kernel=4208/3000
encoder=246/900. This bound is inherited and did not move. The two new
gates measured PRELUDE-HETEROGENEOUS-FUNCTOR at 3.58 seconds and
PRELUDE-HETEROGENEOUS-FUNCTOR-RUNTIME at 3.42 seconds. No leg gave
exit=124 and no leg timed out, so no standalone rerun was needed.

The mutation replay ended with
HETEROGENEOUS-FUNCTOR-MUTATIONS passed=True controls=8 restored=1.

Run 1 of verify-final-HF.sh gave one FAIL row, staged paths outside
the review, naming dev/MUTATION-LOG.md. That FAIL came from an
allowlist gap in the review's own verify script, not from the tree.
The repair added dev/MUTATION-LOG.md to the allowlist before run 1b.

Run 1b then gave 30 OK rows and no FAIL row, but it errored on an
unset rc variable before it could print a VERIFY rc line, because no
FAIL row set the variable. This is a second, separate defect in the
review's own script. The repair set rc=0 before the helper functions,
so the script always reaches its result row.

Run 2 of verify-final-HF.sh gave 30 OK rows, no FAIL row, and one
NOTE row. The NOTE row read: ladder battery rc=1 in
ladder-HF-close.log, judged by the gates log check. This is the
expected inherited bound noted above, not a new defect. The script
ended with VERIFY rc=0.

## Stage C / U1 composable functors (2026-09-14)

Base c1a6075. Added MechComposableFunctors with six independent
universe levels and three shared category instances. First, Second
and Composite functor records share those instances; composition
requires the same middle category record for both inputs. The object
and arrow maps apply the first functor and then the second. Both
preservation laws are checked source proofs, using the second
functor's cross-family congruence and target equality transitivity.
The three functor bodies mirror MechHeterogeneousFunctor under
renaming, enforced by the new kernel suite.

The suite checks 158 definitions and three instances, including six
distinct levels and an all-zero instance. Generic signatures pin each
hom and functor sort, and generic witnesses retain arbitrary input
laws. Ten negative fixtures refuse incompatible middle records,
nominal category and equality mismatches, reversed inputs, missing
and false laws, erased objects and wrong hom levels. Arity and budget
refusals bring the total to twelve. No symbolic globals escape and no
new trusted entries appear.

Four runtime exports pass on the kernel, Node and Wasmtime at two
payloads. The object levels rise twice while hom levels first fall
and then rise. Erased type arguments disappear and reappear at
different levels in the composed arrow closure. The numeric oracles
detect changed object maps, changed arrow maps and reversed source
arrow composition. Mapping identity returns the original payload.

Build: zero errors and warnings. The new kernel and runtime gates
passed in 17.113 and 24.947 seconds. The existing
heterogeneous-functor suite passed. Seven mutation controls produced
distinct named failures, then the restored suite passed in
38.040 seconds. Source and executable hashes match the replay.
Shell and Python syntax checks and the staged whitespace check pass.

Full battery: 47 of 51 PASS, exit 1.
Failed gate: PRELUDE-LEFT-KAN.
Failed gate: PRELUDE-CATEGORY-RUNTIME.
Failed gate: PRELUDE-CATEGORY-ACCESSORS.
Failed gate: TRUSTED-LINES.
Watchdog timeout: PRELUDE-LEFT-KAN, exit 124.
Subprocess timeout: PRELUDE-CATEGORY-RUNTIME, 210 seconds.
Subprocess timeout: PRELUDE-CATEGORY-ACCESSORS, 210 seconds.

Complete diagnostics, timings and hashes are retained in
`dev/validation/port-uat-u1-composable-functors/`. Existing watchdogs,
trusted-line bounds, compiler and vendor bytes, mapping verdicts and
corpus denominators are unchanged.

The design contract is `dev/PORT-UAT-U1-COMPOSABLE-FUNCTORS.md`.
Stage C and U1 remain open on sharing across separate template
instances, general identity and repeated composition APIs,
heterogeneous natural transformations and left Kan extensions, and
source-type parity. All changes are staged for the user's commit.

### Review round 1 (2026-09-14)

`dev/composable-functors-mutations.py` holds one more control,
composite-mirror. It changes the composite functor law accessor of
`prelude/cat/composable-functors.mech`. The mirror fold of the kernel
suite stops at its first changed row, so canonical-mirror reaches the
First row only. On a temporary copy the mutant printed
PRELUDE-COMPOSABLE-FUNCTORS-FAIL functor mirror changed: Composite and
the restored copy printed PRELUDE-COMPOSABLE-FUNCTORS-OK entries=158
instances=3 computations=4 negatives=12.

The control that was named object-order is now named second-object-map.
It replaces one operation of the second object map. The two object maps
have different source and target types, so no well typed swap of their
order is possible. `dev/MUTATION-LOG.md` gives the same name.

### Review round 2 (2026-09-14)

The renaming table for levels in `test/prelude_composable_functors.ml`
now applies to sort positions only, that is to a name after `succ`.
The object binder of the Second edge and of the Composite edge of
`prelude/cat/composable-functors.mech` is `z`, the name of the
template, and the universe parameter `q` of the new prelude does not
move: `rg -c -F 'succ q'` gives 6 in the index and 6 in the tree. The
suite printed PRELUDE-COMPOSABLE-FUNCTORS-OK entries=158 instances=3
computations=4 negatives=12 after the change, and the build printed
OK build: 0 errors, 0 warnings.

## Stage C / U1 composable functors review (2026-09-14)

Findings, fixed: L3-1 (medium, universal Mismatch substring pinned two
negative fixtures), L3-2 (low, canonical-mirror control covered only
the First edge), L2-4 (low, eager three-arm match inside Option.fold
~none:), L2-5 (low, catch-all `_` match arm), L2-6 (low, partial
string indexing text.[index]), L2-1 (low, token-blind mirror renaming
map), L3-4 (low, control named object-order tested no order, renamed
second-object-map), GATE-1 (high, round 2 waited for the round 1
battery EXIT row before it touched _build and .gatework).

Dropped: L4-2 refuted by control, the temp directory is removed on
timeout and mkdtemp gives each replay a distinct path. L1-1 and L2-2
folded into L3-1 as duplicates. L2-3 folded into L3-2 as a duplicate.
L3-3 cut at the finding cap, same refuted scenario as L4-2. No item
was ruled.

Gate verdict BOUND-ONLY. PASS count 50 of 51 legs. TRUSTED-LINES
stays red on the inherited kernel bound (D-A-1); the leg stays red
until the user rules D-A-1. Kernel count 4208 of 3000. Encoder count
246 of 900. Load at battery start 15.02, waits 0.

PRELUDE-COMPOSABLE-FUNCTORS-OK entries=158 instances=3 computations=4
negatives=12
PRELUDE-COMPOSABLE-FUNCTORS-RUNTIME OK cases=4 hosts=3 mutation=1
COMPOSABLE-FUNCTORS-MUTATIONS passed=True controls=8 restored=1

Pin: vendor/kanon at 936a43a92dd59a04698648f24fa5ae94cdb532df.

Staged paths: 30 before this review block, 30 after.


## M0 Stage C / U1: heterogeneous natural transformations (2026-09-14)

Base: d89fe51. The new MechHeterogeneousNatTrans group reuses
one MechHeterogeneousFunctor instance as Base. It adds components,
naturality, identity and vertical composition across four independent
universe levels. Target symmetry and square-composition helpers are
checked source proofs. No kernel, surface, encoder, vendor or mapping
source changes are needed.

Validation:

- Build: zero errors and warnings.
- Kernel: 150 entries, four specializations, five computations, 13 refusals.
- Earlier runtime harness: five exports, three hosts, payloads 37 and 41 passed.
- Final runtime harness, initial check under shipped deadlines: FAIL (exit 1, 111538 ms).
- Separate 900-second runtime diagnostic: PASS (exit 0, 63103 ms).
- Subsequent runtime confirmation under unchanged shipped deadlines: FAIL (exit 1, 110298 ms).
- Mutation replay: eight distinct required failures and a passing restoration.
- Full battery before runtime harness batching: 41 of 53 PASS.

Failed battery rows:

- FAIL PRELUDE-COMPOSABLE-FUNCTORS
- FAIL PRELUDE-NATTRANS
- FAIL PRELUDE-LEFT-KAN
- FAIL PRELUDE-HETEROGENEOUS-FUNCTOR-RUNTIME
- FAIL PRELUDE-HETEROGENEOUS-NATTRANS-RUNTIME
- FAIL PRELUDE-COMPOSABLE-FUNCTORS-RUNTIME
- FAIL PRELUDE-CATEGORY-RUNTIME
- FAIL PRELUDE-CATEGORY-ACCESSORS
- FAIL PRELUDE-FUNCTOR-RUNTIME
- FAIL PRELUDE-NATTRANS-RUNTIME
- FAIL PRELUDE-LEFT-KAN-RUNTIME
- FAIL TRUSTED-LINES

Watchdog timeouts (exit 124): PRELUDE-COMPOSABLE-FUNCTORS, PRELUDE-NATTRANS, PRELUDE-LEFT-KAN.


Bounded rechecks after the battery, before runtime harness batching:

- heterogeneous-nattrans-runtime: FAIL (exit 1, 110327 ms)
- composable-functors: FAIL (exit 124, 120075 ms)

Initial probes encountered contention and timeouts, documented in the
validation README. The shipped runtime harness retains a 110-second
total deadline, with compilation sharing that budget. Both new gate
legs use the existing SLOW watchdog. All earlier gate bounds remain.
The final harness checks both payloads in one program and preserves all
ten kernel results and twenty WASM comparisons. The longer diagnostic
does not count as a pass within the shipped gate budget.

The design is `dev/PORT-UAT-U1-HETEROGENEOUS-NATTRANS.md`.
Reproduction commands, source hashes and captured results are under
`dev/validation/port-uat-u1-heterogeneous-nattrans/`. U1 and Stage C remain
open on general category-instance reuse, general identity and repeated
functor composition, heterogeneous whiskering and left Kan extensions,
and source-type parity. Typed mapping and PRELUDE-CHECKED remain due.

## Stage C / U1 heterogeneous natural transformations review (2026-09-14)

- L2-1: wrong-middle and wrong-endpoint got distinct binder names and longer
  `.err` prefixes, and `test/prelude_heterogeneous_nattrans.ml` replaced the
  length floor by a pairwise prefix check. Leg PRELUDE-HETEROGENEOUS-NATTRANS.
- L2-2: missing-law.err names the expected naturality component type. Leg
  PRELUDE-HETEROGENEOUS-NATTRANS.
- L2-3: identityValue reads `natAdd heterogeneousNatInput 5` through the
  identity component and is 42, so it differs from componentValue. Legs
  PRELUDE-HETEROGENEOUS-NATTRANS and the mutation replay.
- L2-4: the test stanza drops the unused mechanism_prelude library. Leg BUILD.
- L3-1: five replay controls expect recorded 260-character diagnostics. Leg
  HETEROGENEOUS-NATTRANS-MUTATIONS.
- L3-2: component-universe became source-hom-universe and lowers the wide
  source Hom sort. Leg HETEROGENEOUS-NATTRANS-MUTATIONS.
- L3-4: dev/MUTATION-LOG.md names the first component of Beta.
- L2-3 follow-on: `test/heterogeneous_nattrans_runtime.py` expects
  `payload + 5` for identityValue at both payloads. Leg
  PRELUDE-HETEROGENEOUS-NATTRANS-RUNTIME.
- ND-1-1: ten `sources.sha256` rows of
  `dev/validation/port-uat-u1-heterogeneous-nattrans/` are re-bound to the
  staged blobs of the round 1 fixes. No gate leg reads that file, so the
  control is `probes/hash-check.py`, which goes from BAD 16 to BAD 7. The
  seven rows that stay are captures: six `mutations.json` source rows and the
  `checks.json` row of `sources.sha256`.
- GATE-1: not fixed. The only red leg is TRUSTED-LINES, kernel 4208 against
  the inherited bound 3000 and encoder 246 against 900. A green leg needs a
  bound move, which the D-A-1 rule refuses.

Review close: the close ladder (RUNNER-EXIT 0, 20:30 to 21:16 at load 60 to
80) prints PASS 44 of 53, EXIT 1, nine FAIL rows: the inherited TRUSTED-LINES
row (kernel 4208 against the bound 3000, encoder 246 against 900, no kernel
file edited) plus eight legs that expired under load, each PASS in rounds 1
and 2 on the same code: PRELUDE-HETEROGENEOUS-FUNCTOR (exit=124),
PRELUDE-CATEGORY-RUNTIME (exit=2), PRELUDE-CATEGORY-ACCESSORS (exit=2),
DEPENDENT-CLOSURE-RUNTIME (exit=124), PRELUDE-FUNCTOR-RUNTIME (exit=2),
PRELUDE-NATTRANS-RUNTIME (exit=2), PRELUDE-LEFT-KAN-RUNTIME (exit=2),
MAP-INVENTORY (exit=124). A recheck ladder (RUNNER-EXIT 0, 21:31 to 22:20 at
load 15 to 41) prints PASS 52 of 53 with TRUSTED-LINES as the sole FAIL row;
all eight legs pass. Every heterogeneous-nattrans leg passes in both ladders:
kernel suite line
`PRELUDE-HETEROGENEOUS-NATTRANS-OK entries=150 instances=4 computations=5
negatives=13`, runtime line
`PRELUDE-HETEROGENEOUS-NATTRANS-RUNTIME OK cases=5 hosts=3 mutation=1`, and
replay line `HETEROGENEOUS-NATTRANS-MUTATIONS passed=True controls=8
restored=1`. Prior rounds: round 1 PASS 52 of 53, round 2 (fix-2) PASS 52 of
53, both with TRUSTED-LINES as the sole red leg. Pin: vendor/kanon at
936a43a92dd59a04698648f24fa5ae94cdb532df. Staged count before the close 58
paths, after the close 58 paths (no new path added at close).

## Stage C / U1 heterogeneous whiskering (2026-09-15)

Base: c7e95c3. Added MechHeterogeneousWhiskering over one shared
MechComposableFunctors instance at six independent universe levels.
First, Second and Composite natural transformations expose component and
naturality accessors. whiskerRight precomposes by a First functor;
whiskerLeft postcomposes by a Second functor. Both return Composite
transformations between the actual compFunctor results, with checked proofs.

The suite checks all three canonical mirrors, opposite universe orderings,
all-zero nominal separation, fourteen structural refusals and budget
exhaustion. Explicit endpoint calls give distinct refusal prefixes. The
middle-record refusal places the mismatched record in the whiskering call;
its result annotation uses a valid functor. Runtime exports distinguish the
object map and both target arrow fields at two payloads on all three hosts.

Validation:

- Kernel suite: PASS, contract 274 entries, four instances,
  four computations and 15 refusals.
- Runtime: PASS, four exports on three hosts at two payloads.
- CLI arity refusal: PASS.
- Mutations: 9 of 9 caught, restored suite PASS.
- Full battery: 54 of 55 PASS, exit 1.

Failed battery rows:

- FAIL TRUSTED-LINES

The kernel, checker, encoder and vendor sources are unchanged. Existing
watchdogs and the TRUSTED-LINES bound are unchanged. Sources, executable
hashes, mutation results and full battery output are recorded under
`dev/validation/port-uat-u1-heterogeneous-whiskering/`. The design is
`dev/PORT-UAT-U1-HETEROGENEOUS-WHISKERING.md`.

Two refusal-prefix files lost trailing spaces after validation. Their
String.trim results are identical; original and final hashes are retained
in the evidence's source_normalizations records.

Stage C and U1 remain open on category-instance reuse, general identity and
repeated functor composition, heterogeneous left Kan extensions and source-type
parity. Typed mapping and PRELUDE-CHECKED remain due.

## Stage C / U1 heterogeneous whiskering review (2026-09-15)

The suite adds a universe-arity control that specializes
MechHeterogeneousWhiskering at five universe arguments and compares the
refusal text with the message of the CLI probe. The refusal count of
PRELUDE-HETEROGENEOUS-WHISKERING moves from 15 to 16, and the suite leg of
dev/gates.sh now pins the whole OK line.

H maps a middle arrow to that arrow in both target slots, and
whiskerLeftFirst reads the first slot of the component at the payload
object. The export therefore measures the whiskering. The arrow-map
mutation control replaces both slots with the identity and it now fails on
whiskerLeftFirst.

Review findings, the fixes and the legs that prove them:

- L4-2 (medium): SPEC.md and dev/M0-STAGE-C.md no longer list heterogeneous
  whiskering as open work. Proof: the staged diff of both files.
- L2-3 (low): the PRELUDE-HETEROGENEOUS-WHISKERING leg of dev/gates.sh pins
  the whole OK line. Proof: the leg is PASS in the review battery, and a
  negatives=15 line fails it.
- L2-1 (low): the suite gains the universe-arity refusal control, so the
  refusal count moves from 15 to 16. Proof: the suite item of the review
  ladder prints negatives=16.
- L3-3 (low): H maps a middle arrow to both target slots, so the export
  whiskerLeftFirst measures the whiskering. Proof: the runtime leg is PASS,
  and an identity in the first slot fails on whiskerLeftFirst.
- L3-2 (low): the replay prints the count of timeouts on its own line.
  Proof: the replay item of the review ladder prints count=0.
- L4-3 (low): the Reproduce block of the evidence README runs as written.
  Proof: the staged diff of that README.
- L4-5 (low): the Evidence list names the two empty files and the two
  post-capture keys. Proof: the staged diff of that README.
- HV-1-1 (low): the arity row of the evidence README wraps at 67 columns.
  Proof: a column count of the file.
- ND-1-1 (medium): the evidence README names rows 5, 8, 9 and 10 of
  sources.sha256 as the rows the fixes changed. Proof: a rehash of the 39
  rows against the index.
- ND-1-2 (medium): dev/MUTATION-LOG.md records the rerun replay under the
  author's block. Proof: the replay item of the review ladder.
- HV-2-1 (low): this block gives one line per finding id. Proof: this list.
- GATE-1 and GATE-2: bound only. The review battery holds 55 legs, 54 PASS,
  and the single FAIL is TRUSTED-LINES, the inherited kernel bound.

Gate result, from the round 2 review ladder (ladder-HW-2.log,
gates-HW-2.log). Battery start load 17.88. The battery holds 55 legs, 54
PASS. The sole FAIL is TRUSTED-LINES: kernel=4208/3000 encoder=246/900
FAIL. That leg stays red until the user rules D-A-1; the bound is
inherited, not moved by this review.

The three OK lines, verbatim from ladder-HW-2.log:

PRELUDE-HETEROGENEOUS-WHISKERING-OK entries=274 instances=4 computations=4
negatives=16
PRELUDE-HETEROGENEOUS-WHISKERING-RUNTIME OK cases=4 hosts=3 mutation=1
HETEROGENEOUS-WHISKERING-MUTATIONS passed=True controls=9 restored=1

Pin: vendor/kanon at 936a43a92dd59a04698648f24fa5ae94cdb532df.

Staging: 52 paths staged before the close, 52 paths staged after it.

## Stage C / U1 heterogeneous left Kan extensions (2026-09-15)

Base: 171513f. The design is
dev/PORT-UAT-U1-HETEROGENEOUS-LEFT-KAN.md.

MechHeterogeneousLeftKan imports one shared whiskering instance at
six independent universe levels. Seventeen new definitions supply
cocones, factorization, universal solutions, candidate records,
accessors and desc_unique. Each solution carries a mediator, its
factorization proof and pointwise uniqueness against every other
mediator that factors the same cocone.

LanCocone expands the composite maps. Generic contracts check its
conversion to the existing Composite natural transformation and
compare factorization with whiskerRight. The record projections use
separate functor and cocone carrier levels. The runtime solver is
generic in its target category and functor, and uses an arrow lift
whose image computes to the original middle arrow.

The suite checks 294 definitions: three left Kan specializations,
two lightweight category cores and the fixtures. It checks exact
inventories, positive completed families, unchanged builtins, no
new axioms, four computations and fifteen refusals. The nominal
check uses only its two category cores, avoiding a fourth unused
left Kan specialization. The runtime gate compares four exports at
two payloads on the kernel, Node and Wasmtime.

Validation on the final sources:

- Build: PASS, zero warnings.
- PRELUDE-HETEROGENEOUS-LEFT-KAN: PASS, entries=294, instances=3,
  computations=4, negatives=15.
- PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME: PASS, cases=4, hosts=3,
  mutation=1.
- CLI arity refusal: PASS.
- Mutation replay: eight controls caught, zero timeouts, restored
  hashes and suite PASS, distinct diagnostics.
- Full battery: 55 of 57 PASS, exit 1. The new kernel gate
  exceeded its initial 120-second tier. TRUSTED-LINES also failed:
  kernel=4208/3000, encoder=246/900, the inherited D-A-1 bound.
- Kernel recheck: PASS under the final 300-second SUITE tier. This
  resolves its timeout and leaves TRUSTED-LINES as the remaining
  failure across 56 validated gates of 57.

The new kernel gate uses the existing SUITE tier, like the original
left Kan suite. Its recheck took 81830.000 ms. The runtime gate
uses SLOW and took 79688.763 ms in the full battery. The two
gate-source hashes and the separate recheck are retained as evidence.
The complete logs and SHA-256 bindings are under
dev/validation/port-uat-u1-heterogeneous-left-kan/.

Stage C and U1 remain open on general instance reuse, identity and
repeated composition APIs, and source-type parity. Typed mapping
and PRELUDE-CHECKED remain due.

## Stage C / U1 heterogeneous left Kan extensions review (2026-09-15)

Review findings, the fixes and the legs that prove them:

- L3-1 (medium): the replay ran the restored full suite under the same
  120-second bound that the battery had already proved too small for that
  suite. run() now takes a per-call budget. The eight author controls keep
  120 s, and the restored suite gets the 300-second SUITE allowance of its
  gate leg. No control bound and no gate tier moves. Proof: the replay item
  of the review ladder prints TIMEOUTS count=0.
- L4-2 (low): the "56 of 57 gates" sentence of the validation README named no
  basis. It now states that the recheck resolves the kernel leg and that the
  count holds across the battery capture and the recheck. No count, bound or
  tier moves.
- L3-2 (low): no control targeted the generic fixture. The replay adds the
  ninth control fixture-solution-pin, which raises the Wide solution-sort pin
  of test/fixtures/prelude/heterogeneous-left-kan.mech from Type 3 to Type 4.
  The control exits 1 with an empty stderr and a distinct diagnostic. The
  control count moves from eight to nine in README.md, dev/MUTATION-LOG.md
  and dev/PORT-UAT-U1-HETEROGENEOUS-LEFT-KAN.md. The author's capture rows
  stay as captured. Proof: the replay item of the review ladder.
- L4-1 (low): the battery paragraph of README.md held one 86-column line and
  a singular pronoun for a plural subject. The paragraph is re-wrapped and
  reads "Their validation record". No number, bound or tier moves.
- L3-3 (low): uniqueWitness of the runtime fixture instantiates the
  uniqueness rule reflexively. A comment above it states that limit and names
  MixedUniqueContract and WideUniqueContract as the pins of the two-mediator
  statement. The fixture text is otherwise unchanged. Proof: the suite and
  runtime items of the review ladder.

The fixture comment and the new control tuple change the sha256 of
test/fixtures/prelude/heterogeneous-left-kan-runtime.mech and
dev/heterogeneous-left-kan-mutations.py against the captured rows of
dev/validation/port-uat-u1-heterogeneous-left-kan/sources.sha256,
mutations.json and checks.json. The captured rows stay as captured.
test/fixtures/prelude/heterogeneous-left-kan.mech is unchanged, because
the new control mutates only the temporary copy of that fixture.

TRUSTED-LINES stays red on the inherited D-A-1 bound. This review moved no
bound and no gate tier.

Gate result: the review ladder printed PASS count 56 of 57, load at that
item 8.88 14.32 17.07. The one FAIL row is
TRUSTED-LINES kernel=4208/3000 encoder=246/900, the inherited bound.

The three OK lines the ladder printed, verbatim:

```
PRELUDE-HETEROGENEOUS-LEFT-KAN-OK entries=294 instances=3 computations=4
negatives=15
PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME OK cases=4 hosts=3 mutation=1
HETEROGENEOUS-LEFT-KAN-MUTATIONS passed=True controls=9 restored=1
```

Gate tier: the kernel leg stays at SUITE=300. gate-tier-change.json binds
dev/gates.sh before sha256
ea1cbc049611458515ee1e0fd8a3a1513ec3a3db155f5755098e741ecdd6fe47 and
after sha256 ff46fb94f32a8dbe8557fa2e496724fd162cc94a5f5bb9e08c239cc08c8840a4.

Pin: vendor/kanon stays at 936a43a92dd59a04698648f24fa5ae94cdb532df.

Staged counts: 61 paths before this round, 61 paths after. The round
edited files already staged from the fix round and added no new path.

Evidence delta: sources.sha256, checks.json and mutations.json each keep
their captured rows for
test/fixtures/prelude/heterogeneous-left-kan-runtime.mech and
dev/heterogeneous-left-kan-mutations.py. checks.json and mutations.json
each carry a new review_delta key with the 2026-09-15 date, the reason,
and the captured and the final sha256 of both paths. The hash-check
prints TOTAL 88 BAD 5; the review-delta-check explains all five rows;
the fifth row is the evidence row of mutations.json, moved by its own
review_delta key and listed as a third path in checks.json; that
checks.json entry was added at 23:35Z during the close ladder, which no
ladder item reads (item 9 prints the hash-check only, and verify-final
recomputes from the staged blobs).
See dev/validation/port-uat-u1-heterogeneous-left-kan/README.md.

Close ladder (2026-09-15 23:28Z, ladder-HLK-close.log in the review
workspace): RUNNER-EXIT 0, but item 4 (the runtime harness with its
shipped 110 s budget) timed out at load 35.74. The battery of the same
ladder passed its runtime leg at load 24.07, so the item 4 result is a
load flake of the same class as the HW close. A recheck ladder
(2026-09-16 00:24Z, ladder-HLK-recheck.log) printed
PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME OK cases=4 hosts=3 mutation=1 at
load 21.43, battery 56 of 57 at load 21.95 (TRUSTED-LINES only), replay
passed=True controls=9 restored=1, hash check TOTAL 88 BAD 5 (the five
rows above). The final verification reads both ladder logs and the
recheck-round gates and replay files: VERIFY-FINAL rc=0.

Run record: finders and the fixer ran on opus medium, the judge and the
verifier ran on opus high, the final verifier died on the account
five-hour rate limit so main verified the five judged items by hand, and
the closer ran on sonnet medium.

## Stage C checked family reuse (2026-09-15)

Base: 2ff1eb9. The design is dev/M0-STAGE-C-REUSE.md.

Closed specializations accept explicit bindings from internal template
families to existing caller families. Each selected declaration is kernel
rechecked in a temporary table and compared exactly with the existing
certificate. The caller keeps its original families, and all fresh members
check against them. Independent heterogeneous functors can therefore share
category cores and compose their results again. MechIdentityFunctor adds a
lightweight checked identity API over one reusable category core.

The final suite passes 14 source refusals, six parser refusals and five raw
API controls. Positive cases cover exact inventories, preserved families,
partial/nested/dependent reuse, computations and mixed-universe functor
contracts. Runtime validation compares object composition, arrow
composition, identity objects and identity arrows at two payloads on the
kernel, Node and Wasmtime. All four cases pass on all three hosts.

Validation:

- Build: PASS, zero warnings.
- TEMPLATE-REUSE of 2026-09-15: PASS, negatives=14 parser=6 raw=5. The
  battery row for TEMPLATE-REUSE ran the battery-era suite
  (negatives=13); dev/validation/stage-c-reuse/kernel-recheck.stdout
  captures the negatives=14 predicate of that day. The review rounds
  superseded both counts: the delivered suite prints negatives=17
  parser=6 raw=6.
- TEMPLATE-REUSE-RUNTIME: PASS, cases=4 hosts=3 mutation=1.
- Full battery: 54 of 59 PASS. Failed rows: PIN-DELTA, PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME, PRELUDE-CATEGORY-RUNTIME, PRELUDE-FUNCTOR-RUNTIME, TRUSTED-LINES.
- PIN-DELTA recheck: PASS after recording surface deltas 373/64/208.
- PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME: FAIL on a standalone recheck.
- PRELUDE-CATEGORY-RUNTIME: PASS on a standalone recheck.
- PRELUDE-FUNCTOR-RUNTIME: PASS on a standalone recheck.
- Unresolved after rechecks: PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME, TRUSTED-LINES.

The battery ran the initial 13-refusal suite. A final recheck covers the
added dependent-family refusal, exact nominal diagnostic and identity at
object/hom levels (2, 3); production
code did not change. The full logs, final source and executable hashes,
and recheck details are under dev/validation/stage-c-reuse/.

The left Kan runtime timeout reproduced on committed base 2ff1eb9 with
byte-identical inputs and the unchanged runner limit. The category and
functor runtime rechecks passed with their original watchdogs. The failed
left Kan recheck and its baseline reproduction remain in the record.

The inherited TRUSTED-LINES failure remains kernel=4208/3000,
encoder=246/900, pending D-A-1. Existing gate predicates, watchdogs, kernel,
encoder, vendor pin, mapping verdicts and denominators are unchanged.
The new reuse gate predicate was strengthened with the added refusal.
Stage C and U1 remain open on symbolic reuse inside groups, additional
shared category APIs and source-type parity. Typed mapping and M0 exit
remain due.

## Stage C checked family reuse review (2026-09-16)

Review round 1 on base 2ff1eb9 fixed seven judged items. In the middle of
review round 1 the kernel suite printed TEMPLATE-REUSE-OK negatives=17
parser=6 raw=5. The L1-3 fix of the same round added one raw case, so the
delivered suite prints TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6. The
whole line pin of dev/gates.sh, the counts of dev/M0-STAGE-C-REUSE.md and
the counts of dev/validation/stage-c-reuse/README.md carry the delivered
numbers.

L1-1 (medium, surface/family_poly.ml): check_reuse rechecks a reused
family under the single group name, so a family of a mutual group whose
constructor mentions a sibling could never match the template. The new
helper self_rec_only finds that cause, the refusal reports it, and
surface/family_poly.mli states the restriction. The negative case
mutual-group-member pins the text.

L1-2 (low, surface/family_poly.ml): check_reuse compares f_level with
Level.equal_budget and keeps the structural test for every other field,
so a level argument that is not normal, for example (max 0 0), is
accepted. The positive check level_reuse covers it.

L2-1 (low, test/template_reuse.ml): raw () returns the number of
scenarios that it runs and the suite prints that number with %d, as
test/template_composition.ml does. The number is five, as before.

L2-3 (low, surface/elab.ml): the reuse refusal speaks only for a name
that Poly.arity resolves, so a misspelled template name with a reuse list
reports the unbound name. The negative case unknown-template-name pins
it.

L2-2 (low, test/template_reuse.ml): the check partial_dependent covers
reuse of one member of a mutual group with a fresh companion, and the
negative case partial-reverse pins the refusal of the reverse binding.

L2-4 (low, dev/reuse-mutations.py): a mutation driver over
_build/default/test/template_reuse.exe on a copy outside the repository,
with its record in dev/validation/stage-c-reuse/reuse-mutations.json.

L3-1 (low, documentation): the evidence README and the block above now
say that the battery row for TEMPLATE-REUSE ran the pre strengthening
suite.

The edits of surface/, test/template_reuse.ml and dev/gates.sh re-open the
stale hash class of dev/validation/stage-c-reuse/sources.sha256. The
captured rows stay unchanged.

Review round 2 (2026-09-16), one builder, six judged findings.

L3-1 (medium, evidence): the bundle described a superseded suite. The
headline of dev/validation/stage-c-reuse/README.md now says that the 54
of 59 battery ran the battery-era suite, in which TEMPLATE-REUSE ran
negatives=13, the count sentences of that README, of checks.json and of
the validation block above name the delivered counts negatives=17
parser=6 raw=6, and a new rechecks row of checks.json records a
standalone run of the staged gate predicate. The capture pair is
template-reuse-recheck-2.stdout, which holds that line, and its empty
stderr. sources.sha256 and final_sources_sha256 are re-recorded over the
delivered bytes and hold two added rows for dev/reuse-mutations.py and
dev/validation/stage-c-reuse/reuse-mutations.json. A new rechecks row
records that the executables_sha256 rows stay the rows of the build of
2026-09-15.

L1-2 (low, documentation): surface/family_poly.mli said that the
semantic level test accepts every level of the certificate that is not
normal. A control with the delivered mech.exe refuses
`specialize R ((max 0 0)) as Y with (R := E)` with `mismatch: the reused
family E does not match the template`, and accepts `specialize R (0)`,
because check_reuse compares the family level with Level.equal_budget and
every other field structurally. The interface sentence now names the
family level and says that a level argument that is not normal in a
parameter type refuses the reuse. The comparison keeps its conservative
behaviour, so no count and no refusal changes.

L1-3 (low, test): the raw API count of test/template_reuse.ml was
`2 + List.length replacements`, so the deletion of three of the six raw
checks left the count unchanged. Every raw check is now one row of a
labelled list, the suite prints the length of that list, and the
delivered line is TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6. The
whole-line pin of leg SLOW TEMPLATE-REUSE at dev/gates.sh, the counts of
dev/validation/stage-c-reuse/README.md and checks.json, the count
sentence of dev/M0-STAGE-C-REUSE.md and the sentences above move with it.

L2-4 (low, evidence): dev/validation/stage-c-reuse/reuse-mutations.json
shipped `"passed": false`, because control C-REUSE-M1 expected the
wrong-level verdict while that mutant defeats the whole structural
comparison and the suite stops at the wrong-constructor scenario. The
control of dev/reuse-mutations.py now expects the verdict that the suite
prints, and control C-REUSE-M5 expects raw=5 after the deletion of one
replacement, which is the count of the shortened list. A rerun of the
driver in a work directory outside the repository, with no ladder
running, printed {"passed": true, "killed": 5, "controls": 5} with
baseline and restored lines TEMPLATE-REUSE-OK negatives=17 parser=6
raw=6. That results.json is the delivered record.

L3-2 (low, documentation): dev/M0-STAGE-C-REUSE.md named a per-payload
cap of 50 seconds. The runtime driver caps the kernel and emit step at 50
seconds and each host run at 10 seconds, both inside the 110-second total
budget, and the sentence now names those steps. No value of
test/reuse_runtime.py changes.

L3-3 (low, documentation): three lines of the evidence README were up to
164 columns wide. Lines 5 to 7 are rewrapped at the 76-column width of
the file, and no count changes with the rewrap.

The edits of test/template_reuse.ml, surface/family_poly.mli,
dev/gates.sh, dev/PIN-DELTA.md and dev/reuse-mutations.py re-open the
stale hash class of dev/validation/stage-c-reuse/sources.sha256, which
this round re-records. The captured stdout and stderr rows of the bundle
stay unchanged.

Review round 2 fixed three more judged items on the same base. L3-1 and
ND-1-1 name one false sentence of this block, which dated the counts of
the middle of round 1. The block now states the delivered counts
negatives=17 parser=6 raw=6. L1-2 makes the reuse accept a closed level
argument that is not normal in a parameter type, an index type or a
constructor argument: surface/family_poly.ml normalizes each closed level
argument once, before it maps the template, so the mapped telescopes carry
the same written level as the reused family. A closed level above the
search bound keeps its written form and refuses, as before. The new
positive case param_level_reuse of test/template_reuse.ml specializes the
parameterized template R with the level argument (max 0 0) and requires
the reused family E to stay unchanged. The case adds no refusal and no raw
control, so the suite line stays TEMPLATE-REUSE-OK negatives=17 parser=6
raw=6 and no pin moves. ND-1-2 re-records the ledger row of
surface/elab.ml in dev/PIN-DELTA.md as 378, the delivered diff, so that
zsh dev/pin-delta.sh ends PIN-DELTA OK. The round-2 edits of
surface/family_poly.ml, surface/family_poly.mli and test/template_reuse.ml
re-open the stale hash class of dev/validation/stage-c-reuse/sources.sha256.
The captured stdout and stderr rows of the bundle stay unchanged.

Review round 3 closed the three rows that round 2 left open, on the same
base and with no ladder running. ND-2-3 rewrapped dev/M0-STAGE-C-REUSE.md
to the 76-column width of the file, and no count or claim changes with the
rewrap.

ND-2-4 re-ran dev/reuse-mutations.py on the delivered sources in a work
directory outside the repository. The first run killed 4 of the 5
controls: C-REUSE-M3, which replaces the budgeted level test
Level.equal_budget of surface/family_poly.ml with OCaml structural
equality of the two written levels, survived the suite. The survivor is
separable, not an equivalent mutant: surface/elab.ml stores the declared
universe in fam_level as written (elab_univ and elab_fam_decl, lines 913
to 937), lib/check.ml copies it into f_level (line 461) and Level.max is a
plain constructor (lib/level.ml line 7), so a family whose own declaration
names the non-normal level (max (succ 0) (succ 0)) keeps that form. The
delivered compiler accepts the reuse of such a family by a template that
instantiates to succ 0 (rc 0), and a compiled M3 mutant refuses it with
"mismatch: the reused family E does not match the template" (rc 1). The
new positive case decl_level_reuse of test/template_reuse.ml runs that
reuse and requires the reused family E to stay unchanged. The case adds no
refusal, no parser case and no raw control, so the suite line stays
TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6, the gate predicate of
dev/gates.sh:207 is unchanged and no pin moves. No control of
dev/reuse-mutations.py was removed, no expected diagnostic was edited and
no scenario was reordered or weakened. The driver re-run on the round-3
sources printed {"passed": true, "killed": 5, "controls": 5} with baseline
and restored lines TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6, and that
results.json is the delivered dev/validation/stage-c-reuse/
reuse-mutations.json.

L3-1 (hash half), with ND-2-1 and ND-2-2, re-records the bundle over the
delivered bytes: all 18 rows of dev/validation/stage-c-reuse/sources.sha256
and all 18 keys of final_sources_sha256 in
dev/validation/stage-c-reuse/checks.json are set from the staged blobs,
after the new record was staged, and a row-by-row check of both files
against the staged blobs prints sources ok=18 bad=0. No other key of
checks.json changes.

test/template_reuse.ml is the only OCaml source that round 3 changes; the
production sources of surface/ and lib/ are untouched, and the close
ladder is the proof for the changed suite.

Close. Two workflow passes ran over the same four cached finder results.
Pass 1 kept seven findings (L1-1 medium, L2-1 medium, L1-2 low, L2-3 low,
L2-2 low, L2-4 low, L3-1 low) and dropped L1-3 as a duplicate of L2-1; all
seven were fixed in round 1. Pass 2, after a resume that re-ran verify and
judge on the fixed tree, kept six findings (L3-1 medium, L1-2 low, L1-3
low, L2-4 low, L3-2 low, L3-3 low) and refuted L2-5 and L3-4 in both
passes; all six were fixed in round 2. Check 1 of round 2 added ND-1-1,
ND-1-2 and the GATE-1 PIN-DELTA leg, all fixed in round 2. Check 2 of
round 2 confirmed eight of nine fixed findings, found L3-1 not fixed in
its hash half, and added ND-2-1, ND-2-2, ND-2-3 and ND-2-4, all closed by
hand in round 3 and round 3b. Round 3b ruled the round-3 survivor
C-REUSE-M3 SEPARABLE, not an equivalent mutant, and added the positive
case decl_level_reuse of test/template_reuse.ml as the separating case; no
production source changed in round 3b. The suite line stays
TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6 and the gate predicate of
dev/gates.sh:207 is unchanged. The close ladder launched after this
paragraph is the proof of the round 3 and round 3b state; its verdict is
recorded in WORK/ladder-CR-close.log and WORK/review-CR-report.md, not in
this file.

## Stage C: symbolic family reuse (2026-09-16)

Base 65faa7e, the committed checked family reuse increment. The user
requested continued mechanism-lang development with all changes staged.
The design is dev/M0-STAGE-C-SYMBOLIC-REUSE.md.

Group dependencies now accept explicit family bindings. Targets can be
caller families or families imported by earlier dependencies. The family
catalog checks reused certificates under the group's universe scope,
retains fresh families only, and rechecks imported and local members
against the shared table. Nested and closed specialization preserve the
sharing. Unknown, duplicate, forward, incompatible and colliding bindings
remain refusals. A group must introduce at least one fresh family.

prelude/cat/shared-functor-chain.mech packages independent functors,
repeated composition and target identity over three shared category cores
at six universe parameters. Stage C and U1 remain open on additional
shared category APIs and source-type parity. No kernel, WASM, vendor,
mapping verdict or denominator source changes.

The build passes with zero errors and warnings. TEMPLATE-SYMBOLIC-REUSE
passes twelve precise source refusals, six parser refusals and five raw
API controls, plus round trips, nested and dependent sharing, exact
inventories, computations and a mixed-universe category client. Its runtime
gate compares four exports on the kernel, Node and Wasmtime at 37 and 41.
All four isolated compiler mutations are detected, with passing baseline
and restored runs. The obsolete parser refusal for valid symbolic reuse
becomes a malformed-clause refusal; the new suite covers the accepted form.

The full battery output, command verdicts, mutation captures and source
hashes are recorded in dev/validation/stage-c-symbolic-reuse/. Existing
watchdog tiers and the TRUSTED-LINES bounds are unchanged. PIN-DELTA is
updated to syntax=67 and parser=209; elab remains 378.

The initial battery hit its 30-minute capture limit after 35 passing
gates and three watchdog timeouts: heterogeneous whiskering at 120 seconds,
heterogeneous left Kan at 300 seconds and natural transformations at
300 seconds. System load reached 110.80. The remaining 23 gates had not
finished. A scoped recheck verifies 383 unchanged source files against the
tested snapshot, reuses the 35 passing verdicts and runs the 26 failed or
unfinished gates with their original predicates and watchdog tiers. Both
attempts are retained in the validation directory.

The scoped recheck completes all 61 gate verdicts in combination with the
initial passes. An isolated retry of its four runtime timeouts verifies
the same 383 source files again and passes symbolic reuse, natural
transformations and left Kan within the original limits. The aggregate
result is 59/61 passing gates. Heterogeneous left-Kan runtime still times
out during kernel evaluation and emission at 110 seconds. TRUSTED-LINES
remains kernel=4208/3000 and encoder=246/900. The runtime retry captures and
all four detected compiler mutations are retained with source hashes;
there is no claim of a passing uninterrupted battery.

## Stage C: symbolic family reuse review (2026-09-16)

L1-1, low, test/template_symbolic_reuse.ml. The cancellation check of the
raw suite used a poll that returns true at once, so the refusal came from
the entry poll of Family_poly.compose and covered no path of this slice.
The check now counts the polls, lets the entry poll and the first
dependency poll pass, and asserts the Budget_exhausted refusal, that no
member callback ran, and that the refusal came at the third poll. The
check count stays 5.

L2-1, low, test/template_symbolic_reuse.ml. The refuse-before-members
check asserted two conditions that hold by construction, because the
globals table and the catalog are immutable values. Both are deleted. The
check keeps the exact refusal and the member-callback condition, and it
adds a condition a failing call could break: a later successful
composition on the same catalog returns a schema of arity 1.

L4-3, low, prelude/README.md and dev/M0-STAGE-C-SYMBOLIC-REUSE.md. All
three functors of the runtime fixture map arrows with the identity
function, so the two arrow exports keep their value if the composition
order or an object map changes. The two documents now state that the
arrow exports pin the accessors only. No fixture, no export and no
runtime budget moves.

L2-3, low, surface/syntax.ml. The reuse_text definition moved above the
SC-D5 comment, which again sits directly above decl_text, and its
nonempty arm is a cons pattern instead of a bare variable arm. The
printed text does not change.

L3-1, low, SPEC.md. The dependency production of a composed template now
shows the optional with clause and a production for a binding, which the
parser and the shipped template already accept.

L3-4, low, ROADMAP.md. The added sentence is re-wrapped under 72 columns,
which is the width of the file.

L3-2, low, README.md. The sentence that called the bundle the latest full
battery now states that the record is an aggregate of one interrupted
capture, one scoped recheck and one isolated retry.

L1-1, low, test/template_symbolic_reuse.ml, round 2. The counting poll of
round 1 still refused inside the mapping code, because the third poll is
the entry poll of map_family. The cancellation check now searches for the
smallest poll allowance that reports the certificate mismatch of
check_reuse, then asserts that one poll less refuses with budget
exhaustion, that the allowance is larger than three, and that no member
callback ran. The check count stays 5, so no pin moves.

Close: round 1 fixed L1-1, L2-1, L4-3, L2-3, L3-1, L3-4 and L3-2, all low.
Round 2 re-fixed L1-1 and closed GATE-1. Gate verdict is BOUND-ONLY:
TRUSTED-LINES stays kernel=4208/3000 and encoder=246/900, red until the
user rules D-A-1. The baseline timeouts on PRELUDE-NATTRANS-RUNTIME and
PRELUDE-LEFT-KAN-RUNTIME both passed exit 0 in the fix-2 ladder, ruled
load, not a slice regression. checks.json now carries a review_delta key
recording the captured and final hashes of the two review-edited paths.
The close ladder launched after this paragraph is the proof of the round
2 state; its verdict is recorded in WORK/ladder-SR-close.log and
WORK/review-SR-report.md, not in this file.

## Stage C / U1: shared natural transformations (2026-09-16)

Base: a168a09. MechSharedNatTrans shares Source, Middle and Target
category families between the First, Second and Composite natural
transformation APIs and the existing whiskering group. Independently
specialized functors interoperate through closed reuse. hcomp is a
checked source definition: it vertically composes H's map of alpha
with beta at G's object. Its naturality follows from the existing
checked proofs.

The build passes with zero errors and warnings. The kernel suite
checks 591 definitions, 11 families, four computations and six
precise refusals across three universe assignments. Round trips,
family certificates and the absence of new trusted entries are
checked. The runtime gate compares four exports on the kernel, Node
and Wasmtime at payloads 37 and 41. Its horizontal fixture maps the
first coordinate, then swaps coordinates, and its vertical fixture
uses noncommuting operations. Both component values and order are
observable.

Six mutation controls are detected. Baseline and restored suites
pass, and canonical and copied source hashes remain unchanged. The
OCaml static audit has no findings, Python and shell syntax checks
pass, and diff --check is clean. The full battery passes 62 of 63
gates, with only the inherited TRUSTED-LINES failure at
kernel=4208/3000 and encoder=246/900. Both new gates pass under the
existing SUITE tier. No existing tier or predicate changes.

Commands, outputs, replay results and source hashes are retained in
dev/validation/port-uat-u1-shared-nattrans/. The design is
dev/PORT-UAT-U1-SHARED-NATTRANS.md. Stage C and U1 remain open on
shared left Kan APIs and source-type parity. Kernel, WASM, vendor,
mapping verdicts and frozen denominators are unchanged.

## Stage C / U1: shared natural transformations review (2026-09-16)

A multi-agent workflow reviewed the slice above on base a168a09.  The slice adds `poly (u, v, w, z, p, q) group MechSharedNatTrans` with member hcomp to prelude/cat/shared-nattrans.mech, two fixtures, six negatives under test/neg/shared-nattrans, the kernel suite test/prelude_shared_nattrans.ml, the runtime harness test/shared_nattrans_runtime.py, the mutation driver dev/shared-nattrans-mutations.py with six controls, and two gates.sh legs.  Four finder lenses ran read-only, four adversarial verifies followed, and a judge kept seven findings.  One builder fixed all seven in round one.  The workflow run halted at its gate stage when its ladder process was killed by a signal.  The review was finished by hand: the fix-1 ladder was relaunched under a signal-proof wrapper, and an independent verifier (opus, high effort) checked the seven fixes read-only.

Findings kept by the judge, all fixed in round one:

J-1 (medium, dev/shared-nattrans-mutations.py): the horizontal controls shared one short kill prefix, so a wrong horizontal mutant could pass as killed.  HORIZONTAL_HEAD is now a 161-character head, and the three controls use three distinct predicates: a shared `mismatch: the term has type (Lan SPi w hom` prefix, horizontal-endpoint = HEAD + `F as self`, horizontal-order = HEAD + `G as self`.

J-2 (medium, test/neg/shared-nattrans/first-sort): the pinned error text did not name the sort failure.  first-sort.mech is now eta-expanded and first-sort.err reads `mismatch: the term has type Type 4 and the expected type is Type 3` (66 characters).

J-3 (low, test/neg/shared-nattrans): the vertical-middle and horizontal-endpoint pins were too short to separate the two failures.  vertical-middle.err is now 148 characters and ends `(Ran SPi w _ C D) H as self`.  horizontal-endpoint.err is now 170 characters and ends `G as self`.

J-4 (low, test/fixtures/prelude/shared-nattrans-runtime.mech): horizontalFirst duplicated another computation.  horizontalFirst now applies the composite to (3, sharedNatInput), the harness guard reads `references = 2 if name in {'horizontalFirst', 'identityValue'} else 1`, and the answers are `[payload, payload + 6, payload + 11, payload]`.  The suite computation pin moved 7 to 37.

J-5 (low, test/fixtures/prelude/shared-nattrans.mech): naturalityWitness was reflexive.  It is now quantified over (0 x : Nat) -> (0 y : Nat) -> (f : Run_Composite_Base_Source_Hom Nat IndexedEnd x y) with body Run_Composite_naturality Nat Point IndexedEnd PairEnd FH GI Horizontal.

J-6 (low, dev/gates.sh:258 and test/shared_nattrans_runtime.py): the 210 s runtime budget was one number for everything.  The budgets are now TOTAL_BUDGET 290, EMIT_BUDGET 240, HOST_BUDGET 30, the expiry text reads `timed out after the {TOTAL_BUDGET} s total budget`, and dev/PORT-UAT-U1-SHARED-NATTRANS.md records the three budgets.

J-7 (low, test/fixtures/prelude/shared-nattrans.mech): the Wide instance was inert.  wideHorizontal now instantiates at C : Type 5, D : Type 3, E : Type 1 and is listed in fixture_members.  The definition count moved 590 to 591 in dev/gates.sh:256, test/prelude_shared_nattrans.ml:11, and the evidence bundle.

V-1 (close-stage correction, this file): the block above still read `checks 590 definitions`.  The close stage moved it to `checks 591 definitions` to match the J-7 count.

Kit pin note: the review kit pins the kernel row as `entries=591`, repinned from 590, because fix J-7 adds a fixture entry and the author's own dev/gates.sh pin moved with it.  The kit bounds FLOOR 60, LEGS 63 and BASELINE_PASS 62 never moved.  The kit's repin tool was not applied because its derived floor of 58 would have moved a bound for machine load.

Evidence delta: the seven fixes edited 11 of the 27 hashed sources after the captures were recorded, namely dev/PORT-UAT-U1-SHARED-NATTRANS.md, dev/gates.sh, dev/shared-nattrans-mutations.py, the two fixtures, three negative files, test/prelude_shared_nattrans.ml and test/shared_nattrans_runtime.py.  The captured rows of the evidence sources.sha256 stay as recorded.  The checks.json review_delta key lists each path with its captured and final hash, and the evidence README ends with one sentence that points to it.  The fix-1 ladder's source check read ok=27 bad=0 through that delta.

Ladders: the baseline ladder, before the fixes, exited 0 at 07:55Z on 2026-09-17 and passed 60 of 63 legs; it failed the inherited TRUSTED-LINES leg, plus TEMPLATE-REUSE-RUNTIME and PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME on a timeout at load 35.  TEMPLATE-REUSE-RUNTIME was rechecked by id and cleared.  Mutation replay passed with six of six killed.  The kit verdict read `PASS raw=60 cleared=1 floor=60`.  The fix-1 ladder, after the fixes, exited 0 at 10:24Z on 2026-09-17 and passed 62 of 63 legs, failing only the inherited TRUSTED-LINES leg.  The kernel row read `PRELUDE-SHARED-NATTRANS-OK entries=591 families=11 computations=4 negatives=6`, the runtime row read `PRELUDE-SHARED-NATTRANS-RUNTIME OK cases=4 hosts=3 mutation=1`, sources read ok=27 bad=0, and mutation replay passed with six of six killed.  The kit verdict read `PASS raw=62 cleared=0 floor=60`, GREEN.  The close ladder runs after this block is written.  Its verdict is in the review kit report and in the commit message.

The inherited TRUSTED-LINES failure predates this slice and is not a regression of this review.

## Stage C / U1: shared left Kan extensions (2026-09-17)

Base bbebbff. `MechSharedLeftKan` reuses exactly three category families
between the shared natural transformation APIs and heterogeneous left
Kan extensions. `unitNat` and `descNat` expose the unit and mediator in
the shared transformation APIs. Independent functors and transformations
participate through checked reuse. The client chooses two distinct
mediators, composes them in an observable order, and supplies an external
transformation to the factorization and uniqueness contracts.

The new kernel suite passes with 944 definitions, 13 checked families,
six computations and seven refusals. The runtime recheck passes all six
exports at inputs 37 and 41 on the kernel, Node and Wasmtime. Five
mutations are detected; baseline and restored suites pass and source
hashes are unchanged. The sharing diagnostic pin was corrected against
the retained execution, with all final predicates rechecked and the
original record preserved.

Two new gates use the existing CATEGORY tier. Emitting the twelve
exports measured 278.820 seconds in the diagnostic run, above the
initial 240-second limit. The final limits are 480 seconds for emission,
540 seconds total and 30 seconds per host. Concurrent and serial
rechecks expired at the emit limit. The new harness now opts into
reachable function selection after full checking and erasure. The
selector preserves type groups and postulates and follows global calls,
lifted closures, captures and branches. Its self-test passes, and a
closure fixture agrees on all three hosts in both original and selected
modes. The final matrix passes with unchanged budgets. Existing
harnesses retain their original mode. No existing gate predicate or
watchdog tier changed. Static checks and the axiom audit
pass. TRUSTED-LINES remains the inherited kernel=4208/3000 and
encoder=246/900 failure. The full battery was not rerun.

Commands, successful results, timeouts, mutation executions and source
hashes are in `dev/validation/port-uat-u1-shared-left-kan/`. The contract
is `dev/PORT-UAT-U1-SHARED-LEFT-KAN.md`. Stage C and U1 remain open on
transformation equations and source-type parity. Kernel, elaborator,
WASM, vendor, mapping verdicts and frozen denominators are unchanged.

## Stage C / U1: shared left Kan extensions review (2026-09-17)

L4-3, medium,
`dev/validation/port-uat-u1-shared-left-kan/verify-mutations.py`. The
bundle verifier now reads the rows of `mutation-predicates.json`. It
compares the row ids with the control ids, and each row against its own
control: the killed flag, the expected prefix recomputed from the driver,
the retained output hash, the retained output size and the retained exit
code. It also checks that the killed total, the control total and the
number of controls agree, and that the four predicate flags are true.

L3-3, low, `dev/validation/port-uat-u1-shared-left-kan/verify-mutations.py`.
Each check of that verifier is now a call of a `require` helper that prints
a `MUTATION-EVIDENCE FAIL` line and exits with status 1. The script no
longer depends on `assert`, so `python3 -O` keeps every check.

L2-1, low, `test/prelude_shared_left_kan.ml`. The negative block reads the
seven refusal texts first, then computes the head that all of them share.
A refusal text must now be longer than 64 characters and longer than that
shared head. A shortened text can no longer accept a different refusal.
The suite reports the same counts.

L2-4, low, `test/prelude_runtime.ml`. A `--reachable` command line with
three words now gives the usage error. Before the fix it fell through to
the arm for a command line without the option, and it opened a file named
`--reachable`.

L2-3, low, `prelude/README.md`. The sentence about the two
PRELUDE-SHARED-LEFT-KAN gates is split. The first gate checks the
contracts, the clients and the seven refusals on the kernel. The second
gate compares the six computations on the kernel, Node and Wasmtime at two
payloads.

L4-1, low, `dev/validation/port-uat-u1-shared-left-kan/checks.json`. The ten
`capture_artifact` values are now repository-relative `.kanon-exec` paths.
Eight of them named a directory under an unrelated project root. The output
hashes of the rows are unchanged.

L3-4, low, `test/shared_left_kan_runtime.py`. The harness gives two
different messages: `duplicate export name` for a repeated export, and
`reserved alternate name already present` for a source that already holds
the alternate name of an export.

The refusal-text floor of `test/prelude_shared_left_kan.ml` and the message
split of `test/shared_left_kan_runtime.py` are the two behaviour changes of
this round. The contract sentences are in
`dev/PORT-UAT-U1-SHARED-LEFT-KAN.md`. The hashes of
`dev/validation/port-uat-u1-shared-left-kan/sources.sha256` and the suite
hash of `mutation-executions.json` cover the edited paths, so the round
that follows recomputes those rows.

ND-1-1, medium,
`dev/validation/port-uat-u1-shared-left-kan/mutation-executions.json`. The
`suite_source_sha256` row held the hash of `test/prelude_shared_left_kan.ml`
from before the L2-1 fix, so the bundle verifier of this evidence tree
exited 1. The row now holds the hash of the file in the tree, and
`execution_results_sha256` of `mutation-predicates.json` holds the hash of
the edited executions file. No execution row, no output hash and no
predicate result changed. The sentence of the bundle `README.md` about an
immutable report now names the restated row. The documented command
`python3 -I dev/validation/port-uat-u1-shared-left-kan/verify-mutations.py`
prints `MUTATION-EVIDENCE OK controls=5 sources=24` and exits 0.

ND-1-2, medium,
`dev/validation/port-uat-u1-shared-left-kan/sources.sha256`. Six of the 29
rows held the hash from before the fix round: `dev/M0-BUILD-LOG.md`,
`dev/PORT-UAT-U1-SHARED-LEFT-KAN.md`, `prelude/README.md`,
`test/prelude_runtime.ml`, `test/prelude_shared_left_kan.ml` and
`test/shared_left_kan_runtime.py`. Every row is recomputed from the staged
file, in the same order and the same format. The gate leg that compares the
rows counts 29 of 29 again.

HV-2-1, low, `dev/M0-BUILD-LOG.md`. The first line of the L4-3 block of
the round before was 79 columns. It is rewrapped at 76 columns with the
same words.

HV-2-2, low, `prelude/README.md`. The line about the shared natural
transformation and the left Kan API was 109 columns. The paragraph is
rewrapped at 76 columns with the same words.

ND-2-1, medium,
`dev/validation/port-uat-u1-shared-left-kan/mutation-executions.json`,
`executables.sha256`, `mutation-predicates.json` and the bundle
`README.md`. The `executable_sha256` row and the two rows of
`executables.sha256` held the hashes of the binaries from before the
suite fix and the runtime fix, so the report named a binary that did
not run the restated suite source. The binaries are rebuilt from the
staged sources with the recipe build, which prints the OK build line
with 0 errors and 0 warnings. The rows now hold
`29c7b7956dbcc331ddd8a68b0dc9348a9aac7b7d617ca414ced2dd7e368e2f43` for
`_build/default/test/prelude_shared_left_kan.exe` and
`2246172bb2ceaa51bc9da49adb9950c9025d3642f7a677adf698b99948567947` for
`_build/default/test/prelude_runtime.exe`. The
`execution_results_sha256` row of `mutation-predicates.json` holds the
hash of the edited executions file. No execution row, no output hash
and no predicate result changed. The bundle `README.md` says that the
review rebuilt the binaries from the staged sources on 2026-09-17. The
documented command
`python3 -I dev/validation/port-uat-u1-shared-left-kan/verify-mutations.py`
prints `MUTATION-EVIDENCE OK controls=5 sources=24` and exits 0. The
`sources.sha256` rows of `dev/M0-BUILD-LOG.md` and `prelude/README.md`
are recomputed from the staged blobs in this round.

Close (2026-09-17). Findings kept: L4-3 medium, L3-3 low, L2-1 low, L2-4
low, L2-3 low, L4-1 low, L3-4 low, ND-1-1 medium, ND-1-2 medium, HV-2-1
low, HV-2-2 low, ND-2-1 medium; all fixed at the paths recorded above.
Refuted: L2-2, L4-4. Merged: L3-1 into L4-1. Cut by the finding cap: L4-6,
L4-2, L4-5, L4-7. Gate verdict BOUND-ONLY, kernel=4208/3000,
encoder=246/900. The TRUSTED-LINES leg stays red until the user rules
D-A-1. The gates leg PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME stays red on
the 110 s per-case emit budget its own driver sets at
test/heterogeneous_left_kan_runtime.py:63, and three calm rechecks at 1-min
loads 23.53, 27.01 and 20.93 timed out at 109.998 s. The slice does not
reach the leg: the harness passes no --reachable flag, the default arm at
test/prelude_runtime.ml:22 keeps the old expression, the inputs of the leg
are unstaged, and dev/M0-BUILD-LOG.md:3163 already reported the leg
unresolved after rechecks before the slice, so no pin moved.

## Veil kernel migration (2026-09-17)

At the user's request, the kernel dependency changes from Kanon
936a43a92dd59a04698648f24fa5ae94cdb532df to the current Veil commit,
a7534cedeac82d396de8e23058ee6bc990560f65. The new submodule path is
vendor/veil. Mechanism's universe and family overlays remain active,
with Veil's checker, circuit reader, erasure, syntax and emitter changes.
The build, shape audit, pin checks and delta inventory use the new pin.
The CLI inherits circuit and build, plus Veil's axiom disclosures.

Build, Veil template and circuit regressions, the three private-shape
examples on Node and Wasmtime, and the CPU auction checks pass. The full
battery and clean Kanon comparisons retain every failure in
dev/validation/veil-kernel/. No existing timeout or trusted-source limit
changes. TRUSTED-LINES now reports kernel=5475/3000 and encoder=246/900,
including Veil's circuit reader and interface.

See dev/VEIL-KERNEL.md for scope, reproduction and the upstream FHC
example limitation. The original denominator file and historical
validation records retain their original source pins.

## Veil kernel migration review (2026-09-17)

L4-1, medium, dev/gates.sh. The VEIL-CIRCUIT oracle held a count-free
pattern, so a deleted boundary case stayed green. The oracle now holds
`^CIRCUIT-BOUNDS 18/18$`, the value of Veil's own battery. The suite
prints `CIRCUIT-BOUNDS 18/18` at exit 0. No tier and no frozen bound
moves.

L3-2, medium, dev/validation/veil-kernel/README.md. The record did not
say that it is a capture of the author's runs. A new paragraph names
the author copy and its baseline sibling as the working directories,
and names check-veil-migration.py and the Veil build outside this tree
as tools that the repository does not ship.

L4-3, medium, dev/veil-gates.py and dev/VEIL-KERNEL.md. Veil's HOST and
HOST-NAT legs, with their fixtures host-nat.kan, nat-bytes.kan and
zk-instance.kan, are not inherited. The gate comment and the regression
coverage list now record that cut and name the reactor imports that the
three pack fixtures alone cover.

L4-2, low, dev/veil-gates.py and dev/VEIL-KERNEL.md. The gate now diffs
Veil's circuit fixtures mu-dependent-layout and one-fields and its
test/circuit-spine.kan against their circuit goldens, because the rule
for a field of an introduction at SMu decides whether those rows read a
depth or a refusal. The gate line stays `VEIL-KERNEL OK shapes=3
hosts=2`, because the shape and host counts do not move.

L3-3, low, dev/MUTATION-LOG.md and dev/veil-mutations.py. The three new
legs landed with no mutation driver. The new driver builds three
mutants of lib/rules.ml in a copy outside the repository and requires
VEIL-TEMPLATES or VEIL-CIRCUIT to refuse each one. The log records the
three controls and states that the replay is not recorded yet, so a
skipped round is no longer indistinguishable from an unrecorded one.

L1-3, low, dev/veil-gates.py. The gate hardcoded exit 1 for every
circuit run and treated any stderr byte as the verdict. It now ignores
the exit code of the circuit command, keeps exit 0 for check, axioms,
build and the two host runners, compares the golden against stdout, and
prints a unified diff and stderr in the failure message.

L1-4, low, .github/workflows/gpu-auction.yml. The workflow checked out
the new submodule but ran no leg. The cpu job now runs the PIN leg and
the two FAST Veil legs after the build step, so a gitlink that leaves
PIN_SHA behind is red in CI.

ND-1-1, medium, dev/validation/veil-kernel/README.md. The review round
adds two changed sources, dev/veil-mutations.py and the modified
dev/MUTATION-LOG.md, while sources.sha256 keeps its 33 captured rows.
The README said that the record lists the final changed sources, which
the two new files made false. The README now says that the record holds
the 33 author sources and that the two files of the review round stay
out of the hashed set.

GATE-1, high, no source path. The only red leg beyond TRUSTED-LINES is
PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME. The recorded failure is a budget
expiry of its runtime harness at 110 s under a machine load of 24 to 32,
not a refusal and not a wrong result. No source changed for it.

Close (2026-09-17). Two fix rounds close eight findings: L4-1
(medium, dev/gates.sh), L3-2 (medium,
dev/validation/veil-kernel/README.md), L4-3 (medium,
dev/veil-gates.py and dev/VEIL-KERNEL.md), L4-2 (low,
dev/veil-gates.py and dev/VEIL-KERNEL.md), L3-3 (low,
dev/MUTATION-LOG.md and dev/veil-mutations.py), L1-3 (low,
dev/veil-gates.py), L1-4 (low,
.github/workflows/gpu-auction.yml) and ND-1-1 (medium,
dev/validation/veil-kernel/README.md). GATE-1 (high, the gate
ladder) closes with no source change: the fix-1 red row was a
110 second budget expiry of
PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME under load 24 to 32, and
the fix-2 recheck passed at load 14. No finding in this slice
carries a ruled verdict. Ten findings were dropped before the
fix rounds as refuted or merged: L2-4, L1-1, L2-1, L3-1, L1-2,
L4-5, L4-4, L2-2, L2-3 and L2-5; their reasons stand in
review-VK-report.md. The close ladder verdict is BOUND-ONLY:
kernel=5475/3000, encoder=246/900, PASS 67 of 68, the one
inherited red leg TRUSTED-LINES. TRUSTED-LINES stays red until
the user rules D-A-1. No bound moves for it.

## Pointwise natural transformation laws (2026-09-18)

Base 59f48f1. MechNatTransLaws adds component equality, reflexivity,
symmetry, transitivity, both vertical identity laws, associativity
and vertical composition congruence. Four independent universe
parameters and checked category reuse connect it to independently
specialized transformation APIs. The proofs use existing category
laws and equality operations; no kernel, axiom or mapping change is
needed. See dev/PORT-UAT-U1-NATTRANS-LAWS.md for the contract.

The compact build reports zero errors and zero warnings. The new
kernel suite passes with 234 definitions, ten families, twelve
computations and six refusals. The runtime suite compares six exports
at two payloads on the kernel, Node and Wasmtime. All seven isolated
mutation controls are detected, and baseline/restored runs pass with
unchanged source hashes. The heterogeneous and shared natural
transformation suites also pass. Shell syntax and diff whitespace
checks pass.

The first mutation capture stopped after two detected controls
because a newline in its next anchor was escaped incorrectly.
The corrected driver was rerun in a fresh directory. Both the
interrupted attempt and the completed result are identified in
dev/validation/port-uat-u1-nattrans-laws/.

Validation is scoped to this additive prelude increment; the full
battery was not rerun. TRUSTED-LINES was rechecked and remains red
at kernel=5475/3000 and encoder=246/900. The existing limit and all
watchdog tiers are unchanged. Horizontal and whiskering equations,
whole-record equality and source-type parity remain open.

## Pointwise natural transformation laws review (2026-09-18)

A review of the staged slice gave seven findings. Round one fixes all
seven in the sources of the slice.

The suite guard for a recorded refusal prefix moves from 20 to 40
characters (`test/prelude_nattrans_laws.ml`). A prefix cut back to the
generic sentence `mismatch: the term has type` is now refused.

The wrong-endpoint negative records 150 characters of its message, which
holds the functor endpoints of the equation. The trans-middle negative
records the whole message, because the first 240 characters do not
separate the two transitivity premises. Both fixture changes keep the
negative inventory at six.

`test/nattrans_laws_runtime.py` computes the completion guard and the
counts of the gate row from the function inventory and the payload list.
The printed row does not change.

`dev/nattrans-laws-mutations.py` lengthens the expected diagnostic of the
five controls that shared a generic sentence. Each expected string now
holds the first segment that separates the recorded outputs.

The record of `dev/validation/port-uat-u1-nattrans-laws/` changes in two
rows. DIFF-WHITESPACE reads the staged slice with
`git diff --cached --check`. The headline row of PRELUDE-NATTRANS-LAWS
carries a working directory, an exit code and the captured row.
`mutations.json` is recaptured. `sources.sha256` is re-pinned for every
path of this round.

Measured after the fixes:

```
PRELUDE-NATTRANS-LAWS-OK entries=234 families=10 computations=12 negatives=6
PRELUDE-NATTRANS-LAWS-RUNTIME OK cases=6 hosts=3 payloads=2
{"passed": true, "killed": 7, "controls": 7}
```

Round two corrects the record defect of round one, and it clears the gate
ladder of round one.

ND-1-1 (medium, `dev/validation/port-uat-u1-nattrans-laws/checks.json`).
The record held the digest of the suite executable from before the round
one edit of `test/prelude_nattrans_laws.ml`. The record now holds
`bafe802fd4f2f7615b50b49ba56bedb02e3a92fc53bed591868000d4a8fca564` for
`_build/default/test/prelude_nattrans_laws.exe`. That digest is the
digest of the file on disk, and it is the digest that `mutations.json`
holds. The capture time of the block moves to the time of this
recapture. The three other executable digests do not change. No bound
and no tier moves.

GATE-1 (high, the gate ladder). Item 5 of round one stopped with the row
`REPLAY-DIR-REUSED`. The replay driver refuses a working directory that
exists, and the directory of that round was present before the ladder
ran. No source of the slice is a cause of that row. Round two runs the
replay in a fresh working directory, and the ladder of this round
measures the summary row again.

Close. Four finder lenses raised thirteen raw findings. Adversarial
verifiers refuted two: L1-2 (nominal-equality body insensitivity is the
documented nominal/category refusal, confirmed by two body mutants that
give the same trace row) and L2-3 (the runtime harness fails closed on a
duplicate erased function; no silent overwrite occurs). The judge kept
seven and round one fixed all seven: L3-1 (medium), L4-3 (low), L1-3
(low), L1-1 (low), L4-2 (low), L2-2 (low) and L2-1 (low). Round two fixed
ND-1-1 (medium), a record defect that the round one source edit
introduced, and closed GATE-1 (high), a reused replay directory. GATE-2
is the ladder script's leg parser reading the summary row `FAIL ids:` as
a leg named `ids:`; it is a script artifact, not a slice defect, and
carries no fix. No kept finding is left unfixed. The close ladder
verdict is GREEN (attempt 4, 2026-09-18): battery PASS 69 of 70 at
FLOOR 68 with the FAIL row TRUSTED-LINES (inherited), the pinned
budget-bound leg PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME passed,
mutation replay 7 of 7 controls killed, sources.sha256 ok=41,
RUNNER-EXIT 0.

## Stage C / U1: pointwise whiskering preservation laws (2026-09-18)

Base: `c7b99fe06ca80da954dad15f6f1c8f1a651022d9`. `MechWhiskeringLaws` supplies six checked preservation
laws for precomposition and postcomposition: identity, ordered vertical
composition and pointwise congruence. Its shared operations and three
pointwise law imports reuse the same category families. All six universe
parameters remain independent. Stage C and U1 remain open.

Validation on this slice:

```text
PRELUDE-WHISKERING-LAWS-OK entries=1176 families=12 computations=24 negatives=6
PRELUDE-WHISKERING-LAWS-RUNTIME OK cases=12 hosts=3 payloads=2
mutations: passed=true killed=6 controls=6
partial battery: PASS 46, TIMEOUT 2, completed 48 of 72
DENOMINATORS: PASS
TRUSTED-LINES kernel=5475/3000 encoder=246/900 FAIL
```

The broader battery was stopped after two unchanged regression tests timed
out: PRELUDE-HETEROGENEOUS-LEFT-KAN at 300 seconds and
PRELUDE-COMPOSABLE-FUNCTORS at 120 seconds. At diagnosis the host load
averages were 134.32, 80.59 and 51.45. Both new gates passed before this
load spike. The remaining 24 checks were not completed, and the two
timeouts remain unresolved. A full regression pass is not claimed.

The separate trusted-line check retains the inherited size failure.
The active kernel, surface implementation, Wasm implementation, vendor,
pin, mapping inventory, old gate predicates and old watchdog tiers are unchanged.
The two new gates use the existing SUITE tier. The build reports zero
errors and warnings. Runtime checks compare both sides of each equation
on the kernel, Node and Wasmtime at payloads 37 and 41.

- PRELUDE-WHISKERING-LAWS: 206.504 seconds, exit 0.
- PRELUDE-WHISKERING-LAWS-RUNTIME: 157.205 seconds, exit 0.

Six symbolic source mutation controls replace the left identity and
composition proofs with reflexivity, discard each congruence premise,
reverse the right composition operands and break target-family sharing.
Every control requires exit 1, empty stdout and its exact pinned stderr
SHA256. Baseline and restored sources pass; source hashes remain unchanged.
These controls check symbolic source types. The runtime suite separately
observes noncommuting maps and both component coordinates.

The validation record is `dev/validation/port-uat-u1-whiskering-laws/`.
Iterated whiskering, horizontal equations, whole-record equality and
source-type parity remain due.

## Pointwise whiskering preservation laws review (2026-09-18)

Seven findings were kept: L1-1, L1-2, L2-2, L3-3, L4-1, L4-2, L4-3, ND-1-1.
All seven were fixed. L1-2, L2-2 and L4-2 were fixed by the Workflow round
(wf_5d8b809f-5d4). L1-1, L3-3, L4-1 and L4-3 were fixed by hand in round 3.
ND-1-1, a stale sources.sha256 pin, was cleared by the repin in this close
stage. No finding was refuted or dropped.

L1-1 gave the mutation replay a unique anchor at
test/fixtures/prelude/whiskering-laws.mech:21 with a seventh control,
right-identity-target; replay now reports killed=7 controls=7. L4-1 moved
the runtime fixture onto the functor Embed with an observer x + 13 probe
carrier, replacing (Constant 7), and proved dependence at 10007 against
5007 on an identity-morphism copy. That change raised the suite leg to
PRELUDE-WHISKERING-LAWS-OK entries=1177 families=12 computations=24
negatives=6 and the runtime leg to PRELUDE-WHISKERING-LAWS-RUNTIME OK
cases=12 hosts=3 payloads=2 comparisons=48, both pinned in dev/gates.sh
rows 273 and 276 and in the bundle README row 23 as 1,177. L4-3 added a
harness guard that compares the produced case set against the required
set. L3-3 backticked 17 of 18 record names in the bundle README; the
README does not name itself, which is acceptable.

Two close ladder runs were needed. Run 1 (00:39 to 01:14 PDT) went RED
because the kit's own suite and runtime row pins (SUITE_AUTHOR,
RUNTIME_AUTHOR) still held the pre-L4-1 counts (entries=1176, no
comparisons row); the 72-leg battery itself was healthy at 71 of 72. Run 2
(01:23 to 01:58 PDT), after the kit pins were moved to entries=1177 and
comparisons=48, passed items 3 and 4 and the verify-ladder-WL.py verdict
read LADDER explicit GREEN.

Close ladder run 2: PASS 71 of 72 against CLOSE-FLOOR 71, 72 legs seen.
The one FAIL is TRUSTED-LINES at kernel=5475/3000, the inherited size
failure the baseline ladder also carried at 71 of 72; it is never a gate
failure. Mutation replay reported passed=true, killed=7, controls=7.
sources.sha256 checked ok=41 bad=0. RUNNER-EXIT 0.

The kernel record still holds entries=1176 from the author's original
capture, kept by design; the suite and harness legs now report 1177 and
48 comparisons after the L4-1 fix. The record files are unchanged by this
close stage apart from the sources.sha256 repin below; the battery legs
pin no record files.

This slice stays staged for the user; no commit was made in this stage.


## Stage C / U1: horizontal composition laws (2026-09-19)

Base: dbaa955. MechHorizontalLaws adds horizontal identities, congruence,
naturality exchange and vertical interchange over the existing shared
three-category API, with six independent universes. The exchange and
interchange proofs take an explicit object argument: naturality can
inspect its component morphism. NatTransEqAt exposes the proposition at
that object; the earlier erased-object relation remains the conclusion
of identity and congruence. No kernel, vendor, axiom or mapping change is
needed. See dev/PORT-UAT-U1-HORIZONTAL-LAWS.md.

Validation:

```text
build: 0 errors, 0 warnings
PRELUDE-HORIZONTAL-LAWS-OK entries=1205 families=12 computations=28 negatives=7
PRELUDE-HORIZONTAL-LAWS-RUNTIME OK cases=14 hosts=3 payloads=2 comparisons=56
PIN-DELTA: PASS
DENOMINATORS: PASS
TRUSTED-LINES: inherited FAIL kernel=5475/3000 encoder=246/900
```

Both new gates use the existing CATEGORY watchdog. The initial shorter
kernel and emission probes timed out; the final serial checks passed
under the documented limits. Refusals pin complete diagnostic fingerprints
as well as readable prefixes. Their coverage includes an attempt to erase
the object required by naturality. Full commands, results, attempted-run
captures and source hashes are in dev/validation/port-uat-u1-horizontal-laws/.
The full battery was not run and no full regression pass is claimed.
Stage C and U1 remain open on iterated whiskering, horizontal associativity,
whole-record equality and source-type parity.

## Horizontal composition laws review (2026-09-19)

L3-1 (low, dev/PORT-UAT-U1-HORIZONTAL-LAWS.md): the Validation
paragraph now names the third kernel-fixture specialization,
`MechCategoryCore (4, 5)` as `SeparateTarget`, and states that the
nominal-target refusal coerces into it.

L4-1 (medium, test/fixtures/prelude/horizontal-laws-runtime.mech,
test/prelude_horizontal_laws.ml, test/horizontal_laws_runtime.py): a
second hcompExchange runtime pairing, exchangeSwapped, holds the First
edge at the identity and varies Beta on Second, so the interchange law
is checked under a second concrete pairing. Suite and runtime counts
move together: entries 1200 to 1205, computations 24 to 28, cases 12
to 14, comparisons 48 to 56; families, negatives, hosts and payloads
are unchanged.

GATE-1 (high, gate ladder): no source under this slice changed. The
lone non-inherited red leg from fix-1, PRELUDE-HETEROGENEOUS-LEFT-KAN-
RUNTIME, sits outside this slice (no kernel or encoder edit here) and
is the TIMING HAZARD load flake named in the fix brief; it is recheck-
by-id, not a code defect, and stays open for the gates stage to
reconfirm at calm load.

Round 3 (2026-09-19, by hand after the two Workflow fix rounds):

ND-2-1 (medium, dev/validation/port-uat-u1-horizontal-laws/sources.sha256):
the round-2 re-pin wrote the 16 entries of the 8 edited files with no
newline between them, so 7 lines held two or more entries and the check
reported ok=47 bad=7. The file is regenerated from the staged blobs of
the same 63 paths in the same order, one entry per line. The check
reports ok=63 bad=0.

Evidence refresh (dev/validation/port-uat-u1-horizontal-laws/kernel.stdout
and runtime.stdout): both captures still held the rows from before L4-1.
They are captured again from the suite executable and the runtime
harness, and hold the two rows dev/gates.sh pins:
PRELUDE-HORIZONTAL-LAWS-OK entries=1205 families=12 computations=28 negatives=7
PRELUDE-HORIZONTAL-LAWS-RUNTIME OK cases=14 hosts=3 payloads=2 comparisons=56

ND-2-2 (medium, review kit only): the kit's own suite and harness row
pins moved to the same two rows. No repository file is involved.

Close (2026-09-20): two findings are fixed, one low (L3-1) and one
medium (L4-1); none refuted, none dropped. The close ladder ran after
the round-3 repair and passed 73 of 74 rows against a CLOSE-FLOOR of
73. The sole FAIL id is TRUSTED-LINES, the inherited red leg at
kernel=5475/3000 encoder=246/900, explained and outside this slice.
The slice stays staged for the user to commit.

## Stage C / U1: iterated whiskering (2026-09-20)

Base: aaeb4bd. Contract: `dev/PORT-UAT-U1-ITERATED-WHISKERING.md`.
The new `MechIteratedWhiskering` group shares four category families
across four existing whiskering APIs and proves precomposition by a
composite, postcomposition by a composite, and mixed whiskering.
All eight universe parameters remain independent. The proofs concern
components at erased objects; horizontal associativity, whole-record
equality, and source-type parity remain open.

The kernel suite passes with 902 definitions, 14 checked families,
18 computations, and six pinned refusals. The runtime suite passes for
six functions and three payloads on the kernel, Node, and Wasmtime,
with 36 external-host comparisons. Both new gates use the existing
CATEGORY tier. No previous gate, budget, denominator, compiler module,
or vendor file changes.

Build, PIN-DELTA, and DENOMINATORS pass. TRUSTED-LINES remains the
inherited failure at kernel=5475/3000 encoder=246/900.
The full battery was not rerun for this source-only increment.
Captures, development attempts, executable hashes, and source hashes
are in `dev/validation/port-uat-u1-iterated-whiskering/`.

## Iterated whiskering review (2026-09-20)

Workflow wf_3d15e1d1-53e ran the finders, verifier, judge, and fixer
for this slice. The kit-reason was a HALT at the round-2 ladder
wait. The review finished by hand and the workflow was never
resumed.

Four findings in round 1. L2-1 (low) and L3-1 (low) are fixed in
round 1, L1-1 (medium) in round 2, and round-1 L3-2 is refuted.
L2-1: the suite test/prelude_iterated_whiskering.ml now hashes a
256-character excerpt of the negative message instead of the whole
message, as ruled; five test/neg/iterated-whiskering/*.digest
values were regenerated to the md5 of that excerpt. L3-1 is fixed by
hand: the dev/M0-BUILD-LOG.md row 4094 heading is normalized to
"## Stage C / U1: iterated whiskering (2026-09-20)".

L1-1 (medium, confirmed) is fixed in round 2. The runtime fixture
test/fixtures/prelude/iterated-whiskering-runtime.mech specializes
MechCategoryCore four times, as Source, Middle1, Middle2, and
Target, and binds Run over the four roles, so the harness exercises
the four-category shape. The suite row moved to entries=902
families=14 computations=18 negatives=6, and the gate row of
dev/gates.sh moved with it. The record README and the port document
carry the same counts.

Round-1 L3-2 is refuted by the verifier.

Baseline ladder: GREEN-FUNCTIONAL, raw 70 plus 5 CLEARED-LOAD
rechecks by id, for 75 against BASELINE_PASS 75 and FLOOR 75.
Fix-1 and fix-2: RED on item 6 only, sources.sha256 stale after the
L2-1 fix, battery raw 75 both rounds. Fix-3: GREEN, raw 74 plus 1
CLEARED-LOAD PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME, for 75; sources
ok 45; every item rc 0; pin-delta OK; RUNTIME_SECS observed 64
against a pin of 216. TRUSTED-LINES stays the inherited red at
kernel=5475/3000 encoder=246/900.

Round 2 ran as Workflow wf_88e779e2-6b2 after the L1-1 fix (ten
agents, no errors). The judge carried five findings: L3-2 (medium),
L2-2 (low), L3-3 (low), L1-3 (low), and L3-4 (low). The fixer fixed
all five, the not-fixed list is empty, and no finding stays open.
The L1-1 fix itself was made by prep Workflow wf_aac981c3-a5c, a
builder on Fable xhigh, before round 2: the suite row moved from
entries=864 families=11 to entries=902 families=14 with a 44-pin kit
cascade. The round-2 fixes are records and documents only. L3-2
(medium): this block described L1-1 as open with the earlier
one-category fixture and its entry pin; the L1-1 paragraph above now
states the four-category fixture and the 902 row. L2-2 (low): the
port document said full-message digests while the suite hashes the
256-character excerpt; the port sentence and the L2-1 row above now
say so. L3-3 (low): the diff-check row of checks.json ran git diff
--check on the empty unstaged diff; it is re-recorded as git diff
--cached --check, rc 0. L1-3 (low): the whiskerRightComp table row of
the port document named the inverted operation order; it now reads L
then K. L3-4 (low): the port document said 18 exports; it now says 18
evaluations of the six exports. No code, fixture, count, bound, or
tier moved. The dev/M0-BUILD-LOG.md and
dev/PORT-UAT-U1-ITERATED-WHISKERING.md rows of sources.sha256 are
re-pinned from the staged blobs.

Close ladder: GREEN on 2026-09-21. The battery passed 75 of 76 legs
against CLOSE-FLOOR 75, and RUNNER-EXIT was 0. The one FAIL is
TRUSTED-LINES at kernel=5475/3000 encoder=246/900, the inherited red
row; it is informational and never a gate failure. sources.sha256
checked ok=45 bad=0. PIN-DELTA OK. The ladder runtime was 44 s
against the 216 s pin. The chain launched at load1 11.89 and load5
14.51, and the battery started at load 10.59 with 0 load polls. The
ladder of record is ladder-IW-close.log and the gates log is
gates-IW-close.log, both in the review kit directory.

Round 1 ran every unit and every Workflow stage on sonnet with the
explicit tier markers, so the round-1 finder, verifier, builder, and
closer tiers are unmet. Round 2 (wf_aac981c3-a5c and wf_88e779e2-6b2)
ran the finders on Fable xhigh, the verifiers and the judge on Fable
max, and the builder and the fixer on Fable xhigh, so the finder,
verifier, and builder tiers are met. The round-2 close-fill unit ran
on the wf-closer roster type, Opus 5 medium, so the closer tier is
met for this unit.

The slice stays staged for the user to commit.

## Stage C / U1: horizontal associativity (2026-09-21)

Base: 747ab466d1052433b26a21fe40705c848b34934e, the committed iterated
whiskering slice. `MechHorizontalAssociativity` shares four category
families across four instances of the existing horizontal-composition API.
Its `hcompAssoc` source proof uses functor preservation of composition,
congruence, and target associativity at an explicit object. All eight
object/hom universes remain independent. No axiom or kernel rule is added.

Validation: the build has zero errors and warnings. The kernel suite passes
with 2,328 definitions, 15 checked families, six computations, and seven
refusals. Two symbolic specializations use distinct universe levels in
opposite orders. The runtime fixture reuses four distinct external roots.
Both parenthesizations compute `103*x+814` at 0, 37, and 41 on the kernel,
Node, and Wasmtime, including 12 external-host comparisons. Replacing the
proof with reflexivity is rejected by the kernel's constructor-index check.

Pin-delta, frozen-denominator, gate syntax, and diff checks pass. The full
battery was not rerun. TRUSTED-LINES retains the inherited failure at
`kernel=5475/3000 encoder=246/900`. Existing gate expectations, watchdogs,
compiler sources, and the vendor pin remain unchanged.

`dev/PORT-UAT-U1-HORIZONTAL-ASSOCIATIVITY.md` defines the API contract.
`dev/validation/port-uat-u1-horizontal-associativity/` records commands,
complete captures, executable and source hashes, and development attempts.
Stage C and U1 remain open on equality of whole transformation records
and source-type parity. No commit is created.

## Horizontal associativity review (2026-09-21)

A four-lens review of this slice kept seven findings: one medium and six
low. The relay fix round repaired all seven. The medium finding added an
abstract statement pin for `hcompAssoc`. The low findings split the refusal
oracle over seven distinct negatives, made `incompatible-endpoints` apply
the law, gated a reflexivity control, wrote the record capture paths
relative to the repository, corrected the functor named in
`prelude/README.md`, and rewrapped six prose rows. No finding was refuted
and no finding was left unfixed. Of the 13 raw findings, two were
downgraded and six were merged into the kept ids.

Gates: the close ladder is the ladder of record. It ran 78 legs and 77
passed against CLOSE-FLOOR pass=77 floor=77. The one FAIL id is
TRUSTED-LINES (kernel=5475/3000 encoder=246/900), the inherited red that
is present at HEAD before this slice; it is INFO only and never a gate
failure. RUNNER-EXIT 0.

The slice stays staged for the user. No commit is created.

## Stage C / U1: natural transformation units (2026-09-21)

Base: 1e4f3716cefa1aa20c153cd01c42c19bd21b69f0, the committed horizontal
associativity slice. `MechNatTransUnits` shares two category families
between source/target identity functors and two shared natural
transformation triangles. Four laws prove identity whiskering and left
and right horizontal units at an explicit object. All four object/hom
universes remain independent. No axiom or kernel rule is added.

The kernel suite passes with 1,281 definitions, nine checked families,
15 computations, and six distinct refusals. It also checks unchanged
initial globals, absence of axioms, abstract whiskering and horizontal
statements, and the heads and names of the runtime unit terms.
Two specializations use opposite universe
orders. The runtime specialization reuses two distinct external families.
Five law-certified component applications compute `203*x+707` at 0, 37,
and 41 on the kernel, Node, and Wasmtime, with 30 external-host comparisons.

The build has zero errors and warnings. Pin-delta and frozen denominators
pass. The full battery was not rerun. TRUSTED-LINES retains its inherited
failure at `kernel=5475/3000 encoder=246/900`. Existing gate expectations,
watchdogs, compiler sources, and the vendor pin remain unchanged.

`dev/PORT-UAT-U1-NATTRANS-UNITS.md` defines the API contract.
`dev/validation/port-uat-u1-nattrans-units/` records complete command
captures, source and executable hashes, and development attempts.
Stage C and U1 remain open on equality of whole transformation records
and source-type parity. No commit is created.

## Natural transformation units review (2026-09-21)

A Workflow review of the staged slice raised 13 findings. The judge kept 7
and the fix rounds fixed all 7. Three are medium: L4-1, the two whiskering
law statements that every level could trivialise; L4-3, the four runtime
unit terms pinned by name only; and L3-1, the source manifest that pinned
no staged document. Four are low: L1-1, the reflexivity control that
replayed the right law only; L4-2, downgraded from medium, the two
reflexivity oracles that shared one digest window; L3-2, the record bundle
without a README and with a divergent kernel argv; and L3-3, a kill claim
with no mutant run. The verifiers refuted two findings. L2-3 fails because
the harness source list and the suite path list differ as the finder cited
them. L3-5 fails because row 40 of the PORT document holds 76 columns.
Four findings were not kept: L2-1, L2-2 and L4-4 were merged into L4-3,
L3-2 and L4-1; L3-4 was cut at the cap of 7 as a cosmetic wrap of two
build-log rows. Every kept fix was verified on the tree by a separate
verifier.
Gates: the close ladder is the ladder of record. It ran 80 legs and 79
passed against CLOSE-FLOOR pass=79 floor=79. The one FAIL id is
TRUSTED-LINES (kernel=5475/3000 encoder=246/900), the inherited red that
is present at HEAD before this slice; it is INFO only and never a gate
failure. RUNNER-EXIT 0.
The slice stays staged for the user. No commit is created.

## Stage C: chosen member export names (2026-09-22)

Base: 510ff32, the committed natural transformation units slice.
`Family_poly.instantiate` and `Family_poly.instance_names` accept an optional
`~exports` mapping from template member names to chosen global names. Omitting
the option keeps the instance-prefix names. Supplying it requires every member
exactly once, with targets that are distinct and clear of `as_name`, the
installed family names, the reused family names and every constructor label
of the template's families or of an ambient family. Export validation shares
the caller's check budget and polls once for each binding. All references to a
renamed member move together, including references from later members.
`dev/M0-STAGE-C-MEMBER-EXPORTS.md` defines the API contract.

Validation after the review fixes: the build has zero errors and zero
warnings. The gates over this code path print

    FAMILY-POLY-OK cases=34
    PRELUDE-POLY-OK templates=2 instances=5 negatives=2
    FAMILY-MEMBERS-OK cases=35
    PRELUDE-TRANSPORT-OK templates=2 instances=7 negatives=5
    PRELUDE-EQUALITY-OPS-OK instances=10 computations=11 negatives=8

The family-member suite gains 16 export cases, from 19 to 35. They cover
dependent member renaming and computation, mapping order, missing and
duplicate sources, unknown members, target collisions with `as_name` (also
under reuse), families, companion families, constructor labels of the
instance and of an ambient family, ambient globals and templates, family
reuse, name planning, the export share of the check budget and late budget
rollback. PRELUDE-TRANSPORT installs the four type-equality operations
under chosen names over a reused family, and its client computes through the
exported cast with the reused family's own witness.
`dev/MUTATION-LOG.md` records the export mutants under
`## Stage C: chosen member export name controls (2026-09-22)`.

The full battery was not rerun. This run is interrupted, not green: it stopped
after the gates that cover the API change and before the remaining long
suites. No kernel rule, gate expectation, watchdog or vendor pin changes. The
source change stays inside `surface/family_poly.ml`, its interface and the two
suites. The slice stays staged for the user. No commit is created.
