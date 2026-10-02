# Bend gate migration

## R3 comparator and preparation log

The user ruling of 2026-09-21 makes Bend 2 the compilation comparator for M1
and M3, with an acceptance ratio at most 1.000. The measured mechanism pipeline
includes parsing, elaboration, kernel checking, erasure, Wasm emission and
validation. M1 retains its warm per-kloc measurement and M3 its full corpus.
Both require equivalent workload pairs, pinned tools, the same machine and
matched cache conditions. Both ratios remain UNMEASURED: the paired source
manifest, Bend validation endpoint and cross-language line denominator are
still unspecified. The original OCaml denominator file remains byte-identical.

On 2026-09-30, `BEND=.../v2.0.27/bend/bin/bend gtimeout 120 make acceptance-build`
passed and prepared 24 native modes plus the CLI. The captured command is
`.kanon-exec/run-kDQfuW`; it has exit code 0 and quiet stderr. This is the
prepared build check, not a measurement of either R3 ratio. The roadmap update
is staged for the user's change-control commit.

## Acceptance and mutation checks

`dev/gates.sh` builds through `make acceptance-build` and preserves all 85
original named protocol and runtime checks, with the original watchdog tiers,
host limits and mutation checks. All 33 existing Python gate commands,
including runtime and mutation checks, remain required. The importer grammar
leg runs the original assertion port against the full corpus and then checks
the lowering protocol. Corpus paths, digests, parity counts, and denominator
checks remain mandatory. Mutation builds retain their original caller limits:
300 seconds by default, 180 for composition, 120 for dependency exports, and
1800 for reuse. Closure and Veil mutation builds retain 300 and 900 seconds.
Standalone native preparation has a separate 3600 second hang ceiling.

The 2026-09-29 completion replay finished all 87 legs: 67 passed and 20 exceeded
their execution deadlines. Native importer routing subsequently passed its
original 30-second limit. Surface checking and both importer assertion drivers
now use native artifacts during acceptance setup. Prepared builds pass the
original 120-second build limit after redundant shared-bundle validation was
removed. Setup first requires every expected mode; individual launcher
installation still performs its original validation. Seven real bundles
passed 56 refusal controls for missing, extra or duplicate modes, invalid
entry points, compiler identities, artifact hashes and source pins.

Shared mutation execution keeps the original 300-second limit, with 180 seconds
for composition, 900 for reuse, and 120 for the dependency-export fixture.
The 25 recorder controls also verify these limits on native and JavaScript
process calls. The recorder also observes build calls and checks their caller
limits, including the separate 1800 second build and 900 second runtime limits
used by reuse mutations.

Additional Bend units, compiler refusal tests and frozen corpus comparisons
run through `dev/bend2-test.py --suite extras --no-build`, with a 7200 second
outer watchdog around their individual limits. The full `make test` command
also runs every original assertion protocol and requires `BEND2 TESTS PASS`.

Pinned non-compiler Veil assets are tracked under `test/veil`. Its `pinned/`
tree preserves original bytes, while the active `test/` tree applies the eight
existing WAT overlays. The PIN leg verifies a frozen provenance digest, every
asset hash, the complete file inventory, and overlay freshness. No vendor
checkout or generated Wasm binary is required for these inputs.

The test compatibility drivers are `wasm_protocol.bend`, `prelude_protocol.bend`,
and `mapping_protocol.bend` under `bend2/tests`. They retain Wasm WAT golden and
kernel/Node comparisons, 34 prelude assertions plus the empty-environment audit,
and 14 mapping assertions. The retired executables are replaced by Bend
assertion ports with compatibility launchers. Frozen golden suites add
comparisons against recorded original outputs. Equivalence of the complete
assertion inventory remains part of migration review; success markers do not
establish that review.
The `circuit-bounds` protocol preserves all 18 named pinned circuit-reader
cases, including the 63-bit integer ceiling and primitive/case overflow, and
requires the exact `CIRCUIT-BOUNDS 18/18` marker. Its source is
`bend2/tests/wasm_circuit_bounds.bend`.

Closure mutation controls now edit the Bend linker and emitter, compile a fresh
CLI entry point, and retain the original Node and Wasmtime runtime failure
predicates. Shape mutation controls edit the Bend polymorphic shape traversal
and run the complete four-shape metadata scope and specialization suite. The
bounded mutation compiler deletes prior artifacts and requires Bend 2.0.27.
Compiler rejection does not count as a killed semantic mutation.
`BEND_MUTATION_BACKEND=native` explicitly selects emitted C compiled with
`clang -O1`; the default is JavaScript. Reports record this choice and no
automatic backend fallback occurs.

Prepare artifacts with `make acceptance-build` before running the acceptance
script. Its build leg repeats that command under the original 120-second
watchdog. Preparation builds the ordinary tools followed by `make native-tests`.
The latter validates or
builds six native relational bundles, two native template drivers, an isolated
native CLI and the two template mutation caches before timed checks. It records
source, tool and artifact pins in `build/bend2-native-tests.json`. Ordinary
`make build` uses JavaScript test shards followed by the full native production
CLI. The test runner's default automatic build uses the same ordering, while
explicit uniform backend options remain available. Build deadlines remain
unchanged, including the test runner's shared 7200-second build deadline.

A recorded acceptance build passed in 2143 seconds under the original
3600-second limit. Publication freshness, PIN-DELTA and R0-AUDIT passed for that
build. Later review found that its native compiler pins covered Apple's launcher
shim without the selected compiler. The corrected builders record both, plus
the resolver and selection environment, and check for drift before cache reuse,
compilation and publication. Compiler invocation still uses the original
entrypoint. Old native artifact, bundle and mutation-cache identities require
fresh builds; JavaScript signatures are unchanged. The canonical behavioral
battery and separate mutation and auction checks still require their fresh runs.

The process helper preserves the command's timeout verdict during cleanup.
It sends TERM before forcing KILL and bounds its final output drain. Cooperative
main-thread nesting is tested through four helper layers. The reserved child
environment key `__BEND2_PROCESS_CLEANUP_DEPTH` selects decreasing cleanup grace;
it is observable and caller mappings are copied before the key is replaced.
Arbitrary detached sessions, uncooperative or severely starved wrappers and
arbitrary nesting depths have no cleanup guarantee. Worker-thread calls support
their own timeout, without guaranteeing process-level cancellation propagation.
The all/extras suites retain the original six-case process regression and add
`bend2/tests/relational_process_nested.py` with three nested-process cases.

All 18 category, functor, natural transformation, Kan extension and associated
law assertion protocols passed on the earlier frozen Bend 2.0.27 source within
their original limits. A later canonical replay records 22 timeouts and the
existing trusted-line failure. Current evidence and the stopped mutation
sequence are recorded in `MIGRATION-BEND2.md`. Their source coverage is recorded
in `bend2/tests/relational_coverage.json`. Associated
Wasm runtime checks and remaining mutation controls are still in progress.
The separate 120-second heterogeneous-left-kan mutation baseline times out;
its longer assertion-protocol limit does not satisfy that mutation gate.

Independent source-control replays pass all three shape and four closure
mutations with fresh JavaScript builds. These auxiliary harnesses are separate
from the runtime legs in `dev/gates.sh`; the receipts establish JavaScript
coverage only. Compiler rejection or timeout never counts as a killed mutation.

`BEND2-BASELINE.json` records production Bend source hashes, physical line counts,
the original Veil pin, and shape-dispatch module scope. The inventory and its
13 shape modules have been independently reviewed. Source changes after
recording fail PIN-DELTA. R0-AUDIT verifies
the production source manifest emitted by `make build`, rejects stale builds,
and rejects shape names outside the recorded scope.
Combined and production-only builds publish that manifest after successful
builds and a final source-inventory consistency check. Check-only, tests-only,
failed and source-drifting builds preserve the previous manifest.

TRUSTED-LINES counts every active `bend2/kernel/**/*.bend` file and the native
`bend2/wasm/gc_encode.bend` encoder. On 2026-09-26 the user approved a
9000-line Bend kernel limit; the encoder limit remains 900.
The current production inventory measures 8668 kernel lines and 289 encoder lines.
This ruling raises the permitted trust footprint and excludes no kernel modules.
Historical runs that failed the earlier 3000-line limit retain their recorded results.

R3 remains **unmeasured**. The frozen denominator files are unchanged.
`dev/bench.sh` includes shell and Bend/Node process startup in each measurement,
performs one warmup, and reports the requested positive number of measured runs.
Validation records cover the official JavaScript backend with Node 23.10.0 and
a 16384 KiB Node stack, and native C compiled at `-O1`. The isolated full native
CLI passed 37 import/certificate and 56 core comparisons, plus 113 backend
observations under the existing classifier. Its category diagnostic passed in
73.3 seconds under the original 240-second cap. Raw backend differences remain
recorded separately; this does not replace a fresh full auction or Linux CI run.

CI pins Bend 2.0.27, Binaryen 130, and Wasmtime 48.0.1 release archives with
their official SHA-256 digests and verifies executable versions. The workflow
itself still needs execution on its Linux and provisioned GPU runners.


## Reachable runtime exports

The runtime driver fully checks the source and rejects declared axioms,
including declarations unused by the requested exports. With `--reachable`,
it then erases the shared export dependency closure once. Lifted functions
use the existing runtime catalog, and emitted declarations keep their checked
order. Each export retains the existing final function slice.

`PRELUDE-RUNTIME-REACHABLE` is a CATEGORY gate. It compares complete and
reachable erasure with captured closures and shared, reordered and
repeated exports, for 27 kernel, Node and Wasmtime comparisons. Four
refusal cases check missing exports, invalid paths, unused axioms and
unused ill-typed declarations. The driver's selection self-check also
compares erased code with complete erasure.

```sh
BEND=/path/to/bend-2.0.27 python3 -P dev/bend2-build.py --target tests --test-mode prelude-runtime --backend javascript
python3 -I test/prelude_runtime_reachable.py
```

The solution-transport host mode still reaches its 480-second emission limit
and remains opt-in. Its full host comparisons have not passed. The records
in `dev/validation/runtime-reachable/` distinguish passing checks from this
timeout and the unsuccessful native build attempt.
