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
