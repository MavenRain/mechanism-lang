# Checked family reuse validation

Base: 2ff1eb9. Date: 2026-09-15.

Full battery: 54 of 59 PASS on the battery-era suite, in which
TEMPLATE-REUSE ran negatives=13. Failed rows: PIN-DELTA,
PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME, PRELUDE-CATEGORY-RUNTIME,
PRELUDE-FUNCTOR-RUNTIME, TRUSTED-LINES.
PIN-DELTA passed after its ledger update. Unresolved gates:
PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME, TRUSTED-LINES.
The battery row for TEMPLATE-REUSE ran the battery-era suite
(negatives=13), and kernel-recheck.stdout captures the negatives=14
predicate of that day, which the review rounds superseded.

- PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME: FAIL on a standalone recheck.
- PRELUDE-CATEGORY-RUNTIME: PASS on a standalone recheck.
- PRELUDE-FUNCTOR-RUNTIME: PASS on a standalone recheck.

The inherited TRUSTED-LINES bound remains kernel=4208/3000 and
encoder=246/900, subject to D-A-1.

The final build passed with no warnings. The kernel recheck of 2026-09-15
printed `TEMPLATE-REUSE-OK negatives=14 parser=6 raw=5`. Review round 1
(2026-09-16) strengthened the suite and review round 2 made every raw
check countable, so the delivered suite prints
`TEMPLATE-REUSE-OK negatives=17 parser=6 raw=6`, and dev/gates.sh holds
that pin. `template-reuse-recheck-2.stdout` captures a standalone run of
that staged gate predicate. Runtime validation printed
`TEMPLATE-REUSE-RUNTIME OK cases=4 hosts=3 mutation=1`.

## Evidence

The stdout/stderr pairs retain the full build, battery, new runtime checks,
standalone rechecks and baseline runtime check. Empty stderr files record
no diagnostics. `checks.json` hashes
these captures, the final executables and the delivered source files.
`sources.sha256` repeats the final source bindings for shell verification.

The full battery ran the original 13-refusal suite. The final recheck adds
dependent-family incompatibility and pins the full nominal-separation
diagnostic, bringing the count to 14. Identity also checks at independent
object/hom levels (2, 3). Its gate predicate was strengthened
accordingly. Review round 1 raised that count to 17 and review round 2
raised the raw count to 6, so the delivered counts are the counts of
`template-reuse-recheck-2.stdout`, not the counts of the battery row.
The other recheck resolves the recorded PIN-DELTA
failure after updating its three surface rows. Both original results and
recheck outputs are retained.

Any runtime rechecks listed above retain their original budgets and their
standalone output. The original battery failures stay in the
captured log and receipt.

The baseline comparison runs the unchanged composable-functor suite on
the committed and current compilers. Both pass. The single sequential
comparison records 15.04 versus 14.23 CPU seconds and 22.08 versus 15.81
wall seconds in `baseline-comparison.json`.

The left Kan runtime timeout also occurs on committed base
`2ff1eb9` with byte-identical fixture and prelude inputs and
the unchanged 110-second runner limit. The baseline attempt took
110.25 seconds. This gate remains
failed on both trees. `baseline-left-kan-runtime.json` records its input
and executable hashes; the paired stdout/stderr files retain the result.

The raw suite controls reject three corrupted family certificates and an
exhausted budget, accept unchanged root reuse and confirm that a failed
reuse keeps the original family: six counted checks. The source mutation
driver `dev/reuse-mutations.py` records its verdict in
`reuse-mutations.json`. The runtime control
changes the payload from 37 to 41 and compares four exports on all three
hosts. No compiled compiler-mutant replay is claimed by this record.

## Reproduce

Run from the repository root:

```sh
zsh dev/dunecho.sh build
_build/default/test/template_reuse.exe .
python3 -I test/reuse_runtime.py
zsh dev/pin-delta.sh
zsh dev/gates.sh
shasum -a 256 -c dev/validation/stage-c-reuse/sources.sha256
```

The full battery returns nonzero while any listed gate remains unresolved.
Existing watchdog limits are unchanged. The new checks use SLOW.
