# M0 mutation log

## Stage 0

Every mutation runs on a copy under the scratch directory
/private/tmp/claude-501/-Users-oobi-Documents-claude4/0c2b51e5-60d5-46cb-b020-ae52f441e18e/scratchpad/stage0,
and never on the repository files.  The judge ran each check again on
2026-09-07 with the runner script judge-mutants.sh, which copies .git,
.gitmodules, .gitignore, PIN, dune, dune-project, dev/, lib/ and vendor/
into the scratch directory before it changes one byte.  The runner
prints the repository file after each mutation, so the log shows that
the repository stayed unchanged.

| id | leg | command | result |
| --- | --- | --- | --- |
| S0-M1 | PIN | copy the repository, write `836a43a92dd59a04698648f24fa5ae94cdb532df` to the copy's PIN file, then `zsh COPY/dev/gates.sh --leg pin` | killed |
| S0-M2 | PIN-DELTA | copy the repository, write `git -C COPY/vendor/kanon show 936a43a92dd59a04698648f24fa5ae94cdb532df:lib/term.ml` to the copy's `lib/term.ml`, which dev/PIN-DELTA.md holds no row for, then `zsh COPY/dev/pin-delta.sh` | killed |
| S0-M3 | DENOMINATORS | copy dev/denominators.json and dev/DENOMINATORS.sha256, change the date field of the JSON copy from 2026-09-07 to 2026-09-08, then `shasum -c DENOMINATORS.sha256` from inside the copy directory | killed |

S0-M1.  The leg printed
`FAIL PIN pin=836a43a92dd59a04698648f24fa5ae94cdb532df gitlink=936a43a92dd59a04698648f24fa5ae94cdb532df head=936a43a92dd59a04698648f24fa5ae94cdb532df want=936a43a92dd59a04698648f24fa5ae94cdb532df`
and exited 1.  The repository PIN file still reads
936a43a92dd59a04698648f24fa5ae94cdb532df.

S0-M2.  The control run on the unchanged copy printed
`PIN-DELTA file diff expected` and `PIN-DELTA OK` at exit 0.  The
overlay file is 4,788 bytes and dev/PIN-DELTA.md holds no `| lib/` row.
The mutated copy printed `PIN-DELTA lib/term.ml NO-ROW FAIL` and then
`PIN-DELTA FAIL`, and exited 1.  The repository lib/ directory still
holds .gitkeep only.

S0-M3.  The control run printed `denominators.json: OK` at exit 0.  The
mutated copy printed `denominators.json: FAILED` and
`shasum: WARNING: 1 computed checksum did NOT match`, and exited 1.  The
digest of the repository file is still
3148d714a481696297e75ef416255618cbc2cd9b56ab84c34f5fb9464fa7cd51, the
value dev/DENOMINATORS.sha256 records.

No mutant survived.

## Stage A (2026-09-07)

The final control suite has 55 cases and prints `LEVELS-OK`.  Each mutant
is an independent source copy under
`/Users/oobi/Documents/gpt1/mechanism-m0a-mutations`, excluding Git,
build and gate scratch directories.  Exactly one OCaml source file
differs from the final control in each copy.  Every mutant builds through
`zsh dev/dunecho.sh build` with exit 0, then runs its own
`_build/default/test/levels.exe` and exits 1 at the behavioral refusal
listed below.  A compile error is never treated as a killed mutant.

| id | mutation | failing test and diagnostic | result |
| --- | --- | --- | --- |
| A-M1 | Change `Rules_lvl.imax` from `Level.imax` to `Level.max`. | `point-former-preserves-prop-impredicativity: expected level 0, got 1` | killed |
| A-M2 | Replace the zero/positive branch recursion in `Level_eq.decide` with recursion over the positive case only. | `imax-symbolic-right-is-not-max: zero branch of imax was discarded`, and, measured in the review replay below, the checker case `scheme-refuses-imax-with-zero-branch-deleted: expected mismatch refusal` | killed |
| A-M3 | Replace `Check.scope` with `Ok c`, retaining the direct Univ rule guard but bypassing the raw-field scope pass. | `ignored-section-shape-refuses-free-level: expected universe refusal` | killed |

A-M1 initially survived the 53-case suite because the algebra checks
called Level.imax directly.  Two real checker tests were then added:
inference of `(P : Prop) -> P` at Prop, and a universally checked Pi
into a closed proposition.  All three mutants were rebuilt and rerun
against the final 55-case test file.  A-M1's initial survival is recorded
as a discovered test gap, not as a passing mutation check.

The first attempted build command supplied a target to dunecho, which
accepts only a mode.  Those commands exited 124 with usage output; all
actual builds used the supported command above.  The initial attempt
logs and first behavioral run remain under `first-run/` in the scratch
evidence directory.

The mutation runner compared SHA-256 manifests of all 62 final-source
OCaml implementation/interface files before and after the final run.
Both manifest files hash
`fe7d603f997262d6d0deb70d72bc1ff51ec1d68b6628f7d91f7d826ab30ea10c`.
No final-source path changed during mutation testing.  The final
test/levels.ml hash is
`b97414312b920a85dc1251e54a3ef9aceff02229948766593fbd19f0bf407172`.
The scratch directory holds report.json, exact mutation diffs and
separate build/test logs for every mutant.

All three final mutants were killed.  The known trusted-line gate failure
is documented separately in M0-BUILD-LOG.md.

## Stage A review round 1 (2026-09-07)

The review round replayed A-M2 and ran two controls of its own.  Each
run changed one file of the repository, built it with
`zsh dev/dunecho.sh build`, ran `_build/default/test/levels.exe`, then
restored the file.  `git diff` showed no change in the restored file
after each run.  The captured outputs are under
`/Users/oobi/Documents/mechanism-lang-a-review/probes/fix/`.

| id | mutation | observation | result |
| --- | --- | --- | --- |
| A-M2 replay | The A-M2 mutation of `Level_eq.decide`, rerun against the runner that now runs every case. | `imax-symbolic-right-is-not-max`, `le-imax-case-split`, and the checker case `scheme-refuses-imax-with-zero-branch-deleted: expected mismatch refusal` | killed |
| R1-C1 | `Poly.instantiate` adds a second entry under `as_name ^ "Pending"` beside the instance. | `failed-instances-preserve-environments: the failed instance leaked an entry into the next environment` | killed |
| R1-C2 | `Level_var.offset` back at its pre-fix test `Z.equal amount Z.zero`. | probe L1-1 printed `negative in_scope 0 = false` and `negative equals itself = false` | caught by the probe |

A-M2 replay.  The suite printed 52 PASS lines and three FAIL lines, then
exited 1.  Two failures are algebra cases and the third is a checker
case.  `scheme-refuses-imax-with-zero-branch-deleted` checks a 2-arity
scheme, so the Stage A observation of the plan, that a polymorphic
target fails to check, is now measured.  Before this round the runner
stopped at the first failing case, so only the first algebra diagnostic
was visible.

R1-C1.  With the same mutation in place and the new count assertion
removed, the suite printed `LEVELS-OK` at exit 0.  The new assertion is
the one check that kills this mutation.

R1-C2.  The probe under `probes/fix/L1-1` calls the builder with a
negative amount.  Before the fix the readers refused the result, as the
row above records.  After the fix the probe prints
`negative offset gives one back = true`, `negative in_scope 0 = true`
and `negative equals itself = true`.  No suite case observes this,
because lib/level.mli exposes no offset function.

## Stage B import foundation (2026-09-07)

Each mutant was built in a separate scratch copy with no change to the
live tree.  Both builds passed with zero errors and warnings, so each
failure below is a behavioral test observation.

| ID | Mutation | Observation | Result |
| --- | --- | --- | --- |
| B-M1 | Extend parse_metadata to accept format 3.2.0. | test/import.exe exits 1; version-3.2.0 reports malformed input was accepted. | killed |
| B-M2 | Let references accept missing name indices. | test/import.exe exits 1; 13 forward-name negatives report malformed input was accepted. | killed |

B-M2 failing cases: level-param-forward, const-forward-name,
lambda-forward-name, projection-forward-name,
declaration-level-parameter-forward, definition-all-forward,
theorem-all-forward, opaque-all-forward, inductive-all-forward,
inductive-ctors-forward, constructor-induct-forward,
recursor-all-forward and recursor-rule-ctor-forward.  These include names
in values and declaration metadata that type lowering does not consume.

The copies are /Users/oobi/Documents/gpt1/mechanism-m0b-mutations/b-m1 and
b-m2.  Their test artifacts are .kanon-exec/run-6Re25C and
.kanon-exec/run-k5m95o, respectively.  Each copy's export.ml differs from
the live source by its one intentional condition only.

## Stage B review mutants (2026-09-07)

The review round replayed two mutants that the staged legs did not kill.
Each mutant was built in one scratch copy outside the repository, with no
change to the live tree.  Both builds passed with zero errors and zero
warnings.  Each row records the observation after the matching fix.

| ID | Mutation | Observation | Result |
| --- | --- | --- | --- |
| B-M3 | Swap the DEFERRED and UNSUPPORTED columns of the PARITY-KIND row in import/report.ml. | dev/import-gates.sh counts exits 1 and prints no PARITY-COUNTS OK line. | killed |
| B-M4 | Map every Lean binderInfo to "default" in import/report.ml. | test/import_output.py exits 1 at its binderInfo comparison; test/import_cli.py exits 1. | killed |

The same round added the inductive group consistency check of
import/decls.ml.  That check refuses the constructor-induct-forward
negative before the name reference check.  A replay of B-M2 against the
current reader therefore cannot report that one negative as accepted.  The
B-M2 row records the observation of the build round, against the reader of
that round.

Before the fixes both review mutants were green.  B-M3 passed because the oracle
of dev/import-gates.sh stopped at the declared= column.  B-M4 passed
because the gated fixture of test/import_cli.py held no binder node.  The
copy is probes/fix/L4-3/mutant under the review directory
/Users/oobi/Documents/mechanism-lang-b-review, reused for both runs.  The
control transcripts are probes/fix/L1-2/control.txt and
probes/fix/L4-3/control.txt under that same review directory.
