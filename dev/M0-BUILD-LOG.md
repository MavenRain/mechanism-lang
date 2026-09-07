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
