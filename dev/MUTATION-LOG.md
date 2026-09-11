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
## Stage C foundation (2026-09-07)

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-M1 | Replace MechUnit and its constructor with postulates in a temporary copy | AXIOMS rejects `prelude postulate: MechUnit` | killed |
| C-M1-extra | Append CounterfeitInit as a postulate | Empty-environment audit rejects the exact added name | killed |
| C-M2-restricted | Give MechProofEq a Type-valued motive | PRELUDE requires the subsingleton large-elimination refusal | killed |
| C-M3-control | Check the new prelude with the unchanged Stage B driver | Exit 1: mechProofReflCtor needs an expected type | control |
| C-M4 | Give a parameterized constructor a false result index | PRELUDE rejects the index computed from its field | killed |
| C-M5 | Use wrong family parameters, nested fields or dependent fields | PRELUDE rejects each file with its specific diagnostic prefix | killed |
| C-M6 | Widen omega_reflection of import/mapping.ml to the prefix `Lean.Omega` | test/mapping.exe fails only-exact-public-omega-namespace-is-never and tsv-schema-is-stable, prints MAPPING-FAIL and exits 1; the MAP-INVENTORY mode of dev/prelude-gates.py exits 1 | killed |
| C-M7 | Keep the name-scan constructor record in elab_ctor_ref of surface/elab.ml | test/prelude.exe fails duplicate-constructor-name with `mismatch: the constructor mechInl expects family AltSum` and exits 1 | killed |

C-M1 is executed inside dev/prelude-gates.py on each AXIOMS run.  C-M2,
C-M4 and C-M5 are negative source fixtures, executed on each PRELUDE run.
C-M3 is a previous-implementation control, with capture at
`/Users/oobi/Documents/gpt16/.kanon-exec/run-eoLViJ`.
C-M6 and C-M7 were replayed by the review fix round of 2026-09-08, one
mutant for each new implementation file, in one scratch copy of the
repository outside the tree.  The copy is
`/Users/oobi/Documents/mechanism-lang-c-review/probes/fix/mutants/copy`,
each build passed with zero errors and zero warnings, and the transcripts
are `probes/fix/L4-5/c-m6.txt` and `probes/fix/L4-5/c-m7.txt` under
`/Users/oobi/Documents/mechanism-lang-c-review`.  The same C-M7 build
refuses the three duplicate-name probes of `probes/fix/L1-1`, which the
fixed elaborator accepts.
The full final battery is captured at
`/Users/oobi/Documents/gpt16/.kanon-exec/run-Rw7Kuk`.

At the foundation checkpoint, the generic Eq large-elimination mutation was blocked at the
declaration step.  eq-data-index and eq-constructor-field require the two
universe-bound refusals.  C-M2-restricted covers only the proof-endpoint
family and does not discharge the generic Eq obligation.

## Stage C data equality (2026-09-08)

The equality increment replaces eq-data-index with checked positive
equality declarations.  eq-constructor-field remains a rejection fixture;
eq-multiple-large-elim separately rejects a two-constructor Prop family
with a data-valued motive.  The field bound and large-elimination
restrictions remain active.

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-EQ-M1 | Run the new prelude with the committed foundation driver | Refuses the MechEq index y at universe 1 above family universe 0 | killed |
| C-EQ-M2 | Change the runtime fixture payload from 37 to 41 | Literal and captured-closure results change from 37/42 to 41/46 on kernel, Node and Wasmtime; recursive payload still returns 2 | observed |
| C-EQ-M3 | Apply the Prop index exception to every family | EQUALITY fails only type-index-above-bound, with 12 cases still passing | killed |
| C-EQ-M4 | Exempt erased constructor fields from the family universe bound | EQUALITY fails only prop-erased-data-field, with 12 cases still passing | killed |

C-EQ-M2 runs inside EQUALITY-RUNTIME on every battery invocation.  Its
fixed expected results ensure runtime transport uses the payload and
preserves captured values across erasure.

C-EQ-M3 and C-EQ-M4 were built in an isolated scratch copy, each with
zero errors and zero warnings.  Both targeted suites exited 1 on their
named refusal oracle.  The scratch source was restored after the controls;
the validated main copy was never mutated.  Full captures and the exact
mutation definitions are recorded in
`/Users/oobi/Documents/gpt2/mechanism-equality-evidence/SUMMARY.md`.

## Stage C equality review, fix round 1 (2026-09-08)

The review round adds one accepted-field case, one two-arm budget case and
one source negative for the Type-family index bound.  Three kernel mutants
replay in a scratch copy of the repository, each built alone with zero
errors and zero warnings, with the source restored after each build.

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-EQ-M5 | Refuse every constructor field at check.ml:483 | EQUALITY fails only data-field-at-bound, with 14 cases passing; the prelude no longer checks | killed |
| C-EQ-M6 | Apply the Prop index exception to every family | EQUALITY fails type-index-above-bound and prop-index-skips-level-poll, with 13 cases passing; PRELUDE fails reject-eq-type-index | killed |
| C-EQ-M7 | Remove the Prop index exception | EQUALITY fails the three Prop index cases and prop-index-skips-level-poll, with 11 cases passing | killed |

C-EQ-M5 is the twin of C-EQ-M4.  C-EQ-M4 refuses erased fields alone and
the accepted-field case did not exist, so a refusal of every field stayed
invisible to EQUALITY.  C-EQ-M6 and C-EQ-M7 move the index exception in
both directions and each one now fails the budget case.  The mutant
driver and the three captures are `probes/fix/mutate.py`,
`probes/fix/L4-2/c-m5.txt`, `probes/fix/L4-1/c-m6.txt` and
`probes/fix/L4-1/c-m7.txt` under
`/Users/oobi/Documents/mechanism-lang-eq-review`.

## Stage C family templates (2026-09-08)

Controls ran in `/Users/oobi/Documents/gpt6/mechanism-family-mutations`.
Each implementation mutant built with zero errors and zero warnings before
the family suite ran.  The primary source copy was never mutated.

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-FAM-M1 | Replace Family_poly's universal kernel judgment with Ok () | The universal field-bound counterfeit, mixed result sort and negative recursive field are accepted; their rejection tests fail | killed |
| C-FAM-M2 | Make the instance rename callback return its input unchanged | Eq, Sum, recursive clients and retry checks fail to find the requested instance | killed |
| C-FAM-M3 | Preserve motive.m_ind instead of applying the name callback | Stored recursive motive name stays stale; a dependency hidden in motive metadata escapes rejection | killed |
| C-FAM-M4 | Remove map_term's entry budget poll | Increasing nested annotation depth adds no substitution budget; the strengthened budget case fails | killed |
| C-FAM-M5 | Change the source client's cast payload from one to zero | PRELUDE-POLY rejects the indexed computation witness with a constructor index mismatch | killed |

C-FAM-M4 initially survived because the earlier test counted shape,
address and leg polls together with Term polls.  The corrected test varies
only nested annotation depth and subtracts each input's measured closed
kernel cost.  It observes growing substitution work without pinning an
exact poll count.  The unmodified implementation passes every case;
the same missing-Term-poll mutant now fails only that budget assertion.

Captures under `/Users/oobi/Documents/gpt6/.kanon-exec/`: C-FAM-M1
`run-dKwq9q`, C-FAM-M2 `run-utNFa6`, C-FAM-M3 `run-3IoMUA`, C-FAM-M4
initial `run-OG5ily` and corrected `run-zuVLgF`, C-FAM-M5 `run-5Yo3Ut`.
The final unmodified build and suite are `run-avVNFK`.  Scratch source
files were restored to the validated primary source after the controls.

## Stage C family-template review (2026-09-09)

Controls ran in a copy of the primary source at
`mechanism-lang-fam-review/probes/fix/copy`.  Each mutant built with zero
errors and zero warnings before the suite ran.  The primary source was
never mutated.

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-FAM-M6 | Weaken the kernel arity scope guard at lib/check.ml to accept every positive arity | FAMILY-POLY prints `FAIL family-poly kernel-scheme-refuses-a-free-header-level: expected refusal: universe: universe level is outside the global parameter scope` and exits 1 | killed |
| C-FAM-M7 | Replace the endpoint negative client with a plain constructor mismatch | PRELUDE-POLY prints `PRELUDE-POLY-FAIL wrong refusal: mismatch: the constructor mechInl expects family MechSum` and exits 1 | killed |
| C-FAM-M8 | Hold the free level of the ignored shape in scope | FAMILY-POLY prints `FAIL family-poly hidden-free-level-in-ignored-shape-is-refused: expected refusal: universe: universe level is outside the global parameter scope` and exits 1 | killed |

C-FAM-M6 answers the kernel arity scope guard, which no earlier row and no
gate leg observed.  C-FAM-M7 shows that the new PRELUDE-POLY oracle pins
the refusal text and not the error constructor alone.  C-FAM-M8 shows that
the ignored-shape case is not vacuous, because its visible head level is
now in scope and only the hidden shape carries the free level.

Captures are `probes/fix/L3-1/control.txt`, `probes/fix/L3-2/control.txt`
and `probes/fix/L4-3/control.txt` under
`/Users/oobi/Documents/mechanism-lang-fam-review`.  After every mutant was
restored, the same copy printed `FAMILY-POLY-OK cases=34` and
`PRELUDE-POLY-OK templates=2 instances=5 negatives=2`.

## Stage C polymorphic transport (2026-09-09)

Run `python3 -I dev/transport-mutations.py NEW_WORK_DIRECTORY` from the
repository root.  The script refuses an existing work directory, copies
the source, and builds each isolated mutation with `dev/dunecho.sh`.
A failed build never counts as a killed mutation.  Each selected test
must exit 1 with its named diagnostic.  The primary source is untouched.

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-TRANSPORT-M1 | Bypass member checking when the universe arity is positive | `definition-must-check` accepts an unbound body and fails its expected-refusal check | killed |
| C-TRANSPORT-M2 | Preserve member names during specialization | The second instance collides with the first instance's unprefixed `witness` | killed |
| C-TRANSPORT-M3 | Bypass member checking when the universe arity is zero | `closed-rechecking` accepts an incompatible replacement for the external `seed` and fails its expected-refusal check | killed |
| C-TRANSPORT-M4 | Leave member levels unchanged during specialization | `hidden-level-specialization` fails with the closed checker's universe-scope error | killed |
| C-TRANSPORT-M5 | Change the library cast client's payload from one to zero | The indexed computation witness reports a TransportIsOne constructor index mismatch | killed |

The exact replacement strings, selected cases and required diagnostics
are in `dev/transport-mutations.py`.  The final run is retained at
`/Users/oobi/Documents/gpt1/mechanism-transport-controls-final`.
`results.json` records all source and mutant hashes and reports 5/5
controls killed.  All five builds had zero errors and zero warnings.
After restoration, FAMILY-MEMBERS passed 18 cases and PRELUDE-TRANSPORT
passed two templates, seven instances and four negatives.

The first run counted only 4/5 because C-TRANSPORT-M4 expected the stored
syntax comparison to fail.  The closed checker had already rejected the
unsubstituted level with a scope error.  Its precise expected diagnostic
was corrected in the control, then all five controls and both restored
suites passed.  No implementation or acceptance gate was weakened.

## Stage C transport review (2026-09-09)

The fix round of the transport review added five controls to
`dev/transport-mutations.py`, so one run now covers ten controls.  Run
`python3 -I dev/transport-mutations.py NEW_WORK_DIRECTORY` from the
repository root.  Each mutant builds cleanly first.  Each selected test
must exit 1 with its named diagnostic.  The primary source is untouched.

| Control | Change | Observed result | Result |
| --- | --- | --- | --- |
| C-TR-M1 | Delete the budget poll at the head of `map_member` in `surface/family_poly.ml` | `declaration-member-budget` prints `member declaration polls 21, expected 22` | killed |
| C-TR-M2 | Delete the budget poll inside the `check_members` fold of `surface/family_poly.ml` | `specialization-member-budget` prints `member specialization polls 17, expected 18` | killed |
| C-TR-M3 | Delete the member name guard that `declare` runs before `map_member` | `member-template-collision` prints `wrong refusal: not yet: references between family schemas are not supported` | killed |
| C-TR-M4 | Replace `TransportHigher_refl MechNat mechZero` in the mixed-instance negative by `TransportData_refl MechNat mechZero` | PRELUDE-TRANSPORT prints `expected refusal: mismatch: the term has type (Lan SMu TransportHigher` | killed |
| C-TR-M5 | Check the unavailable-member negative in the installed scope instead of the template-free scope | PRELUDE-TRANSPORT prints `expected refusal: unbound: CastData_cast` | killed |

The run of this round is
`/Users/oobi/Documents/mechanism-lang-transport-review/probes/mutations-1`.
`results.json` records every source and mutant hash and reports killed 10,
controls 10.  All ten mutant builds had zero errors and zero warnings.
After restoration, FAMILY-MEMBERS passed 19 cases and PRELUDE-TRANSPORT
passed two templates, seven instances and five negatives.

## Stage C polymorphic equality operations (2026-09-09)

Run `python3 -I dev/equality-ops-mutations.py NEW_WORK_DIRECTORY` from
the repository root.  The script copies the source, builds each mutant,
requires the selected diagnostic from PRELUDE-EQUALITY-OPS, and restores
the original file in a finally block.  A compiler failure never counts
as a killed control.  Exact replacements and hashes are stored in
`results.json`, with one stdout and stderr capture per build and test.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-OPS-M1 | J returns y instead of its reflexivity case | quantity: erased binder y read at runtime |
| C-OPS-M2 | Reverse the symmetry motive's endpoints | MechEq endpoint type mismatch |
| C-OPS-M3 | Transitivity eliminates its first proof | MechEq endpoint type mismatch |
| C-OPS-M4 | Congruence claims the mapped left endpoint twice | mapped endpoint type mismatch |
| C-OPS-M5 | Reverse the type-symmetry motive's endpoints | MechTypeEq endpoint type mismatch |
| C-OPS-M6 | Type transitivity returns reflexivity instead of the first proof | reflexivity index A where B is required |
| C-OPS-M7 | Change the J fixture's returned natural from one to zero | OpsIsOne constructor index mismatch |
| C-OPS-M8 | Replace the mixed-instance negative by its accepted control | expected refusal is missing |
| C-OPS-M9 | Ask the normalizer oracle for a natural instead of the proof-indexed data constructor | wrong computation for opsJ |

Final run:
`/Users/oobi/Documents/kanon-inference/mechanism-equality-ops-mutations-2`.
All nine mutants built with zero errors and zero warnings; all nine were
killed.  The restored suite reports ten instances, eleven computations
and eight negatives.  In the first run C-OPS-M1 was rejected by the
quantity check before type comparison; the expected diagnostic was
corrected to match that earlier guard.  No implementation or gate was
weakened.

## Stage C equality operations review (2026-09-09)

The review round added one control to `dev/equality-ops-mutations.py`.
It continues the C-OPS series of the block above.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-OPS-M10 | Swap the J motive's proof and endpoint arguments at prelude/equality.ml:68 | PRELUDE-EQUALITY-OPS prints mismatch: the term has type (Lan SMu MechEq [right] |

C-OPS-M10 exists because C-OPS-M1 dies at the quantity guard before the
J elimination typing runs.  Replay directory:
`/Users/oobi/Documents/mechanism-lang-eqops-review/probes/mutations-EQ-1`.
Its `results.json` reports passed true, killed 10, controls 10.  Its
`C-OPS-M10.stdout` starts with `PRELUDE-EQUALITY-OPS-FAIL mismatch: the
term has type (Lan SMu MechEq [right] (Sec SColl 2 [ => A;  => x])) and
the expected type is A`.  Correction to the block above: its "Final run"
and "All nine mutants ... all nine were killed" describe the earlier run
`/Users/oobi/Documents/kanon-inference/mechanism-equality-ops-mutations-2`
with nine controls.  The total after the review round is killed 10,
controls 10.  Round 2 restored that block to its staged text after round
1 had edited it in place.

## Stage C dependent functions and pairs (2026-09-09)

Run `python3 -I dev/dependent-mutations.py NEW_WORK_DIRECTORY` to replay
these controls.  The script copies the source into a new external work
directory, requires every mutant to compile cleanly, checks its exit code
and diagnostic prefix, restores the changed file, and reruns the restored
suite.  results.json records exact replacements and source/mutant hashes.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-DEP-M1 | Replace Pi's imax with max | symbolic former universe mismatch |
| C-DEP-M2 | Drop the fiber universe from Sigma's result | symbolic former universe mismatch |
| C-DEP-M3 | Return the second field from the first projection | dependent field type mismatch |
| C-DEP-M4 | Return the first field from the second projection | dependent field type mismatch |
| C-DEP-M5 | Pass the first field twice to the recursor step | step argument type mismatch |
| C-DEP-M6 | Swap the pair constructor's point and fiber | point type mismatch |
| C-DEP-M7 | Replace pairOne's second component with zero | wrong computation for pairSecond |
| C-DEP-M8 | Replace the wrong-fiber negative with its accepted control | missing expected refusal |
| C-DEP-M9 | Make the elimination motive refer to the outer pair | dependent branch type mismatch |

Final replay:
`/Users/oobi/Documents/gpt2/mechanism-dependent-mutations-2`.
All nine mutants built with zero errors and zero warnings and were killed.
The restored suite reports six templates, 29 instances, twelve computations
and six negatives.  The first run observed the universe guard for M1 and M2
instead of the anticipated mismatch guard; the two diagnostic expectations
were corrected before the final replay.  No implementation changed.

## Stage C dependent functions and pairs review (2026-09-09)

The review round added one control to `dev/dependent-mutations.py` and
pinned the exact measured line of five controls of the block above.  It
continues the C-DEP series of that block.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-DEP-M3 | Replace the first projection by the second in mechSigmaFst | PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point A (APt w x) B) and the expected type is A |
| C-DEP-M4 | Take the first branch in mechSigmaSnd | PRELUDE-DEPENDENT-FAIL mismatch: the term has type A and the expected type is (Out SPi w point A (APt w x) B) |
| C-DEP-M5 | Pass the first component to the recursor step payload | PRELUDE-DEPENDENT-FAIL mismatch: the term has type A and the expected type is (Out SPi w point A (APt w x) B) |
| C-DEP-M6 | Swap the two components of the constructed pair | PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point A (APt w x) B) and the expected type is A |
| C-DEP-M9 | Make the elimination motive refer to the outer pair | PRELUDE-DEPENDENT-FAIL mismatch: the term has type (Out SPi w point (Lan SPi w point A (Out SPi w point A (APt w point) B)) (APt w (In SPi w point A (APt w x) [y])) P) |
| C-DEP-M10 | Drop the caller budget in the Poly.declare fold at prelude/dependent.ml:78 | PRELUDE-DEPENDENT prints budget: ... |

The five earlier controls shared the prefix `PRELUDE-DEPENDENT-FAIL
mismatch:` before this round.  Each now pins its measured line.  C-DEP-M3
and C-DEP-M6 print one and the same line, and C-DEP-M4 and C-DEP-M5 print
one and the same line, so those two pairs stay indistinguishable from each
other.  The pinned text is measured, not a description.

C-DEP-M10 exists because no control observed the `?budget` thread of
`Dependent.catalog`.  Its mutant builds cleanly and the suite refuses it.

Replay directory:
`/Users/oobi/Documents/mechanism-lang-dep-review/probes/mutations-DEP-3`.
All ten mutants built with zero errors and zero warnings and were killed:
`{"passed": true, "killed": 10, "controls": 10}`.  The restored suite
passes.  The measurement replay of the same round is
`/Users/oobi/Documents/mechanism-lang-dep-review/probes/mutations-DEP-2`.

## Stage C independent-universe congruence (2026-09-09)

Replay `python3 -I dev/congruence-mutations.py NEW_WORK_DIRECTORY`.
Every control builds with zero errors and zero warnings before a test
failure counts.  The script pins exact replacements and diagnostic prefixes,
records source and mutant hashes, restores each file and checks both
restored suites.  No mutation changes the working repository.

| Control | Injected defect | Observed rejection |
| --- | --- | --- |
| C-CONG-M1 | Use domain universe for codomain B | Type u0 versus expected Type u1 |
| C-CONG-M2 | Replace result's right endpoint with f x | Result equality at f y versus expected f x |
| C-CONG-M3 | Leave companion family name unspecialized | Second instance collides with Second |
| C-CONG-M4 | Skip symbolic group-member checking | Member-family collision incorrectly accepted |
| C-CONG-M5 | Drop congruence catalog's caller budget | Exhausted-budget refusal missing |
| C-CONG-M6 | Change upValue's result from one to zero | Wrong computation: upValue |
| C-CONG-M7 | Replace mixed-instance negative with its accepted control | Wrong-instance refusal missing |

Final replay: `/Users/oobi/Documents/gpt2/mechanism-congruence-mutations-2`.
Result: `{"passed": true, "killed": 7, "controls": 7}`.  Restored outputs:
`FAMILY-GROUPS-OK cases=21` and
`PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=4`.
The first replay's C-CONG-M1 failed with a type mismatch rather than its
initial universe-error oracle; the final replay pins the measured refusal.

## Stage C family groups and congruence review (2026-09-09)

The fix round of the review added two controls to
`dev/congruence-mutations.py`, so one run now covers nine controls.
The seven earlier controls keep their pinned diagnostics.

| Control | Injected defect | Observed rejection |
| --- | --- | --- |
| C-CONG-M8 | Accept a template reference from a group member | Member type keeps the template name |
| C-CONG-M9 | Drop the occupied check of the install fold | Companion collision incorrectly accepted |

C-CONG-M8 rewrites the name closure of `declare_group` to `Ok n` and is
killed by `FAMILY-GROUPS-FAIL member-template-reference: wrong refusal:
unbound: the family Other is not declared`.  C-CONG-M9 removes the
`occupied` test of the install fold of `instantiate` and is killed by
`FAMILY-GROUPS-FAIL target-companion-collision-atomic: expected refusal:
mismatch: the name One_Second is already declared`.  Both mutants build
with zero errors and zero warnings.

Replay: `/Users/oobi/Documents/mechanism-lang-cong-review/probes/mutations-CONG-1`.
Result: `{"passed": true, "killed": 9, "controls": 9}`.  Restored outputs:
`FAMILY-GROUPS-OK cases=22` and
`PRELUDE-CONGRUENCE-OK instances=8 computations=4 negatives=5`.
The block "Stage C independent-universe congruence (2026-09-09)" above
reports killed 7, controls 7, `FAMILY-GROUPS-OK cases=21` and
`negatives=4`.  Those numbers name the run of that round.  The totals
after this round are killed 9, controls 9, cases 22 and negatives 5.

## Stage C textual prenex definitions (2026-09-10)

`dev/prenex-mutations.py` builds each control in an isolated copy, checks
the designated diagnostic, restores the source and checks the restored
suite.  Build failures do not count as killed mutations.

| Control | Mutation | Observed refusal after PRENEX-FAIL |
| --- | --- | --- |
| C-PRENEX-M1 | Resolve every universe binder to index zero | universe: the former lives at 0 and the expected universe is 1 |
| C-PRENEX-M2 | Lower imax as max | universe: the former lives at imax(u0, u1) and the expected universe is max(u0, u1) |
| C-PRENEX-M3 | Elaborate a template under universe arity zero | universe: universe level is outside the global parameter scope |
| C-PRENEX-M4 | Replace an exhausted source-check budget with unlimited | budget: expected refusal |
| C-PRENEX-M5 | Disable the program catalog's name reservation | template-collision: expected refusal |
| C-PRENEX-M6 | Reverse specialization arguments | universe: the former lives at 1 and the expected universe is 0 |
| C-PRENEX-M7 | Replace the one-valued data witness with zero | wrong computation: dataValue = (In SMu Tiny [] (ACtor tinyZero) []) |

Replay: `/Users/oobi/Documents/gpt11/mechanism-prenex-mutations-2`.
Result: `{"passed": true, "killed": 7, "controls": 7}`.  Every mutant
built with zero errors and warnings.  The restored suite reports
`PRENEX-OK entries=16 computations=4 negatives=25`.

The first replay had three diagnostic-oracle mismatches and one surviving
control.  M1, M2 and M6 reached universe refusals rather than mismatches.
M3 showed that plain identity did not exercise the elaborator's scope
check.  An annotated local function application now exercises that path.
The second replay kills all seven controls.  The final M3 oracle was
tightened against its retained output.  The only subsequent parser edit
moved a comment; final gates recheck that source.

PRENEX-RUNTIME also changes the literal payload from 37 to 41 and requires
that answer on the kernel, Node and Wasmtime, while the closure answer
remains 12 on every host.

The review replay of 2026-09-10 uses
`/Users/oobi/Documents/mechanism-lang-prenex-review/probes/mutations-PRENEX-2b`,
after the constructor-reservation fix.  It reports
`{"passed": true, "killed": 7, "controls": 7}`, and its baseline and
restored suites both report
`PRENEX-OK entries=16 computations=4 negatives=25`.  The seven controls
are unchanged.  The C-PRENEX-M7 row above holds the full refusal that
the review measured, in place of the earlier prefix.

## Stage C textual polymorphic families (2026-09-10)

`python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY --families`
replays seven isolated mutations. Every mutant builds without warnings
and must fail PRENEX-FAMILIES with its recorded diagnostic:

- C-PRENEX-FAM-M1 drops the elaborator's family-header universe scope.
- C-PRENEX-FAM-M2 reverses explicit universe arguments at specialization.
- C-PRENEX-FAM-M3 drops the family-template name reservation.
- C-PRENEX-FAM-M4 discards the installed closed family environment.
- C-PRENEX-FAM-M5 permits a specialized constructor to take a definition
  template's name.
- C-PRENEX-FAM-M6 discards the caller's family-check budget.
- C-PRENEX-FAM-M7 changes the boxed data payload from one to zero.

The first replay killed five of seven. M1 survived because direct Sort
headers did not exercise elaborator scope checking. A computed parameter
type now forces that check. The first version of this fixture lacked the
function ascription required by the surface language; its replay stopped
at the failing baseline, before mutation testing. The corrected fixture
passes normally and kills M1. M2 already failed semantically, but the
harness expected a mismatch instead of the observed universe error. Its
oracle now pins the exact universe diagnostic.

The final replay in mechanism-family-mutations-3 killed seven of seven;
baseline and restored suites report 13 families, 18 entries, five
computations and 26 negatives. The independent definition regression
replay killed its existing seven controls and restored its 16 entries,
four computations and 25 negatives. Both machine-readable reports are
retained in dev/validation/stage-c-prenex-families/.

PRENEX-FAMILIES-RUNTIME additionally changes the box payload from 37 to
41 and requires that change on all three hosts while the recursive list
sum remains 12. This is an observed execution check, not a type-only
mutation.

## Stage C textual polymorphic families review (2026-09-10)

The review of 2026-09-10 adds four controls to the `--families` replay,
which now replays eleven isolated mutations.  Each added mutant keeps
the type of the expression it changes, builds without warnings, and must
fail PRENEX-FAMILIES with its recorded diagnostic:

- C-PRENEX-FAM-M8 makes the post-install label check ask the family
  catalog for a name that no template holds, so a specialized
  constructor could take a family template's name.  Its oracle is
  `PRENEX-FAMILIES-FAIL late-family-template-collision: expected
  refusal`.
- C-PRENEX-FAM-M9 replaces the caller's budget at
  `Family_poly.instantiate` with an unlimited budget.  Its oracle is
  `PRENEX-FAMILIES-FAIL specialize-budget: expected refusal`.
- C-PRENEX-FAM-M10 breaks the new refusal of a family constructor whose
  label repeats the family name.  Its oracle is
  `PRENEX-FAMILIES-FAIL self-named-constructor: expected refusal`.
- C-PRENEX-FAM-M11 breaks the refusal of an instance constructor whose
  label repeats the specialized instance name.  Its oracle is
  `PRENEX-FAMILIES-FAIL instance-own-constructor: expected refusal`.

The block above records the pre-review suite: seven controls and 26
refusal checks.  Those numbers stay as measured then.  The suite of this
review runs 30 refusal checks against eleven controls.

The review replay in
`mechanism-lang-prenex-families-review/mutations-PFAM-2-families`
reports `{"passed": true, "killed": 11, "controls": 11}`, and its
baseline and restored suites both report
`PRENEX-FAMILIES-OK families=13 entries=18 computations=5 negatives=30`.
The definition replay in
`mechanism-lang-prenex-families-review/mutations-PFAM-2-defs` reports
`{"passed": true, "killed": 7, "controls": 7}` with
`PRENEX-OK entries=16 computations=4 negatives=25`.  The seven family
controls of the block above are unchanged.

## 2026-09-10: textual ordered groups and member controls

`python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY --groups` adds
eleven controls.  They require clean builds, designated suite refusals
and a passing restored suite.  M1 reverses family order; M2 reverses
member order; M3 reverses output rows.  M4 and M5 remove generated-name
checks against definition templates and constructors.  M6 removes the
label check against generated companion and member names.  M7 checks
only root-family labels.  M8 and M9 bypass declaration and specialization
budgets.  M10 changes the normalized payload.  M11 removes source-group
reservation against definition templates.

The first M1 run produced `unbound: the family Box is not declared`,
while the harness expected `unbound: Box`.  It was not counted as killed
in that attempt.  The exact diagnostic was corrected and the full replay
reports 11/11 killed with passing baseline and restored suites.  The
existing family and definition replays report 11/11 and 7/7.  Their
control expressions and expected mutation diagnostics are unchanged.

The old source tests that refused ordered groups now refuse explicit
mutual syntax.  Ordered groups have positive coverage in PRENEX-GROUPS.
The runtime control changes 37 to 41 through specialized member calls;
the independent call remains 12 on all three hosts.  Results, source
hashes and baseline/restored outputs are retained under
dev/validation/stage-c-prenex-groups/.

## 2026-09-10: ordered group review, fix round 1

The review round adds two controls to the `--groups` replay, for a total
of thirteen.  M12 removes the new label check against the family names
of the caller globals; its designated refusal is
`late-label-instance-name`.  M13 removes the group-name check against
existing constructor labels; its designated refusal is
`companion-constructor-name`.  The replay reports 13 of 13 killed with a
passing baseline suite and a passing restored suite.  The family and
definition replays report 11 of 11 and 7 of 7.  Their control
expressions and expected diagnostics are unchanged.

## 2026-09-10: checked categories and dependent records

`dev/category-mutations.py` builds an isolated copy and requires a clean
build before every rejection. The baseline and restored suites pass.
All nine controls were killed. The three broad prefixes in the first
replay were narrowed afterward and checked against the retained outputs
and matching current source hashes with `--verify`; all nine verify.
The replay report and stricter verification are recorded separately in
`dev/validation/stage-c-category/`.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-CAT-M1 | Drop the typed first projection in the surface motive | Elimination without a motive cannot be inferred |
| C-CAT-M2 | Compare constructor indices without their types | Function-category identity proof cannot convert its endpoint |
| C-CAT-M3 | Reuse the frozen motive syntax | Captured type has the wrong universe context |
| C-CAT-M4 | Reuse the frozen branch syntax | De Bruijn index 2 is outside the context |
| C-CAT-M5 | Do not extend the dependent index environment | De Bruijn index 0 is outside the environment |
| C-CAT-M6 | Accept unequal constructor indices | Unequal-endpoint negative unexpectedly succeeds |
| C-CAT-M7 | Return the left-identity field as right identity | Generic member type mismatch |
| C-CAT-M8 | Reverse noncommuting composition inputs | compositionValue normalizes to the wrong value |
| C-CAT-M9 | Drop the typed first projection in pair eta | Category eta witness cannot convert the neutral record |

The concrete-record runtime control changes its payload from 37 to 41
while preserving the other result at 12. The kernel, Node and Wasmtime
agree for both variants. The independent generic-accessor probe remains
a documented host failure and is not counted as a killed mutation.

## 2026-09-10: checked categories review

The review adds one control. `dev/category-mutations.py` now replays ten
controls. The tenth reverses the fresh binder order of the frozen
elimination quotation, which the previous suite did not observe.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-CAT-M10 | Reverse the fresh binder order of a quoted motive | Indexed motive body has the wrong type after quotation |

The replay of this round reports ten of ten killed with a passing
baseline suite and a passing restored suite. The nine earlier controls
keep their expressions and their diagnostics. The replay report, the
saved control outputs and the stricter verification are restaged in
`dev/validation/stage-c-category/`, so `--verify` runs against a copy of
that directory outside the repository. The mode writes its report into
the work directory, so it never writes inside the repository.

## 2026-09-11: dependent closure calls

`dev/closure-mutations.py` replays four compiler controls in an isolated
copy.  Each control builds without warnings and fails its designated
export on Node and Wasmtime.  The kernel checks pass.  Baseline and
restored suites pass with nine exports and two input values per export.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-CLOS-M1 | Use annotated arity for an indirect call | partialPayload traps on both WASM hosts |
| C-CLOS-M2 | Drop nullary closure recognition | nullaryPayload traps on both WASM hosts |
| C-CLOS-M3 | Give a nullary call the closure result representation | nonTailPayload traps on both WASM hosts |
| C-CLOS-M4 | Skip emission of a call with no runtime arguments | nullaryPayload traps on both WASM hosts |

`dev/wasm-golden-mutations.py` checks two gate controls in an isolated
input tree against the current build.  The restored suite passes.

| Control | Change | Observed rejection |
| --- | --- | --- |
| C-CLOS-G1 | Append a comment to one local WAT golden | d06-closure-capture golden differs |
| C-CLOS-G2 | Remove one local WAT overlay | Golden overlay inventory refuses |

The replay reports and complete control outputs are stored in
`dev/validation/stage-c-closures/`.  The generic category accessor
probe now passes and is included in the ordinary gate battery.
