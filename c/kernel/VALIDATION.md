# Native kernel validation

The active kernel is implemented in C and built with tcc (`make c-build`). The observations below that name Bend, JavaScript or a native binary were made on the Bend 2.0.25 port that these files replaced (commit 8c90b64); each C driver named here is the port of its Bend driver. Public contracts are in API.md. The port uses native arbitrary precision signed base-32768 arithmetic, explicit Result/Maybe failures, and threaded poll budgets. It has no OCaml subprocess path.

Source mapping:

- Local `lib/{level_var,level_eq,level,level_scope,eval,conv,check,rules}.ml` map to the corresponding native modules. `check_engine.c` holds the mutually recursive checker, conversion and shape rules behind `check.c`, `conv.c`, and `rules.c` public APIs.
- Pinned `vendor/veil/lib/{bignum,quantity,literal,shape,term,value,error,global,prim,positivity,pp}.ml` map to the same native names.
- Pinned `order.ml` and `totality.ml` map to `order.c` and `totality.c`; the latter also exposes Comp wrappers for frontend budget composition.
- Pinned `erase.ml` maps to `erase.c` (term/declaration/program traversal), `erase_repr.c` (runtime representations and nominal layouts), and `erase_util.c` (capture discovery, pruning and runtime reindexing).
- Pinned `spec_count.ml` maps to `spec_count.c`, derived from native Term, Shape and Rules metadata.
- Backend-owned Eterm and Circuit integrate through `eterm.c` and `circuit.c`.

Observed passing execution suites:

- `c/tests/kernel_numbers.c`: large exact integer arithmetic.
- `c/tests/kernel_eval.c`: beta reduction, primitives, closure readback.
- `c/tests/kernel_quantity.c`: linear path usage.
- `c/tests/kernel_check.c`: linearity and raw universe scope.
- `c/tests/kernel_declarations.c`: sequential declarations, family installation, erased index rejection, late constructor budget exhaustion. A compiled native binary executed successfully.
- `c/tests/kernel_conversion.c`: function eta, exact universe equality, unequal literals, type distinction, proof irrelevance, universe inhabitants. On the Bend 2.0.25 port (commit 8c90b64), Bend execution and a compiled native binary passed the first three checks and the JavaScript unit shard passed all six.
- `c/tests/levels_exact.c`, `c/tests/levels_budget.c`: universal comparison, arbitrary precision offsets, late budget exhaustion.
- `c/tests/levels_regression.c`: 17 algebra regression families from original `test/levels.ml`.
- `c/tests/levels_checker.c`: symbolic schemes, Prop impredicativity, rejected finite-sampling counterfeits, ten raw syntax scope positions, family scope, budget rejection.
- `c/tests/kernel_budget_dag.c`: a successful universe projection does not render a shared level DAG of depth60; a shared term DAG of depth40 exhausts its100-poll budget.
- `c/tests/kernel_recursive.c`: exact original recursive-values source, eight normalization comparisons, guarded neutral functions, neutral recursive calls, opacity and raw mu/self rejection.
- `c/tests/kernel_spec_count.c`: emitted the exact eight-row R0 block in SPEC.md.

The Bend 2.0.25 port (commit 8c90b64) used `bend FILE --check-only` for checking, `bend FILE` for native compilation and execution of the IO test entry points, and `bend FILE -o OUTPUT` for explicit output compilation; all its imported kernel definitions checked with no remaining TODO laws, and its unsafe annotations marked the explicitly recursive implementation. The C port is built by `make c-build` (tcc), and `make c-test` runs its drivers.

Erasure differential: `c/tests/kernel_erase_driver.c` checks source, erases checked entries, and prints erased code. On the Bend 2.0.25 port (commit 8c90b64), its official Bend JavaScript output matched all 80 original `vendor/veil/test/fixtures/*.kan` erased goldens byte for byte, after removing the single final newline added by the driver's IO.print. Default Node stack passed 79 cases; `mu-cata-depth` passed with `node --stack-size=16384`. Report and runner are currently `/private/tmp/kernel-erase-diff.json` and `/private/tmp/kernel-erase-diff.py`; deep-case successful output is `/private/tmp/kernel-erase-deep.stdout`.

A frontend boundary regression found that conversion's proof probe initially recovered Budget_exhausted as false. The integrated conversion now rethrows that variant, matching `lib/conv.ml`. Frontend exact-budget tests then passed: 32 polls succeeds, 31 fails for a two-declaration program, and a larger template/member/specialization boundary also passed.

Integration validation remains coordinated by the parent: full first-party equality/family/template/prelude and negative diagnostic corpus, runtime emitter integration, and CLI compatibility. Negative integer indices and arities cannot be constructed in the native public API because their types are Nat; malformed textual inputs remain parser errors. The native tests do not claim to replace every original catalog/CLI fixture individually.

An importer stress case exposed eager diagnostic rendering on successful universe inference. `universe_value`, type equality, former-universe equality and index equality now build their diagnostic text only on failure. This preserves results and poll counts while avoiding repeated unbudgeted quoting/level rendering. A fresh official build, `build/import-pipeline-lazy.js`, completed importer case1102 in 19.755 seconds with its exact expected budget error and empty stderr. Its SHA256 is `ef4fdd528d15ebfe2994f11cbbe392750d29d68e5fd62ebba4eb883d94e53ddf`; measurement is `build/import-pipeline-lazy-case1102.json`. A separately instrumented copy observed exactly 100001 poll calls, including the final failing poll, in 52.748 seconds under concurrent load (`build/import-pipeline-lazy-polls1102.json`).

Callback budget restoration, builder observations on 2026-09-23 on the Bend 2.0.25 port (commit 8c90b64; each file is named by its C port):

- `c/kernel/budget.c`, the Bend check (`--check-only`) of its source at commit 8c90b64: passed, capture `.kanon-exec/run-R54jOn`.
- `c/tests/kernel_budget_callbacks.c`, its Bend source at commit 8c90b64: native compilation and execution passed, capture `.kanon-exec/run-PiExrA`. Covers exact finite state, callback invocation boundaries, successor state across runs, refused-poll recovery, non-budget recovery, attempt, prevention of later continuations, lazy cancellation of a million-poll computation, IO callbacks, and an expired clock deadline.
- `c/tests/kernel_budget_callback_check.c`, its Bend source at commit 8c90b64 compiled to `build/kernel-budget-callback-check.js`: passed, capture `.kanon-exec/run-qumYpe`; execution with `node --stack-size=16384` passed, capture `.kanon-exec/run-EnCbY4`. Covers exact and late universe-comparison exhaustion, conversion's two-poll proof-probe recovery boundary, subsequent shared-state exhaustion, raw shared-DAG scope cancellation, totality's error message, and a clock-backed refusal of real kernel comparison.
- `c/tests/surface_boundary.c`, its Bend source at commit 8c90b64 compiled to `build/surface-boundary-callbacks.js`: passed, capture `.kanon-exec/run-dhLSyD`; execution passed, capture `.kanon-exec/run-ASHeDN`. Existing scope, universality, member cancellation, nested budget and inventory assertions remain. The cancellation case now uses a real successor callback rather than directly replacing finite state.

Installed clock source evidence of the Bend 2 toolchain (commit 8c90b64): its `effs/now.js` returns `BigInt(Math.floor(performance.now()))`. `effs/now.c` returns `io_tick() / 1000000`; the installed compiler's embedded native runtime defines `io_tick` using `clock_gettime(CLOCK_MONOTONIC)` and converts seconds/nanoseconds to nanoseconds. Thus both callbacks observe monotonic milliseconds. Cancellation remains cooperative, with no promised maximum elapsed time between kernel polls.

The callback refactor supersedes the state-function representation used by earlier binaries. Independent callback review, refreshed whole-runner validation and refreshed differential evidence remain pending with the parent; earlier hashed binaries do not establish the new representation's coverage. `c/tests/surface_program.c` and `c/import/lower.c` received direct-application adapters and await that shared integration replay. No callback-equivalence certification is claimed by these builder observations.

Continuation performance correction: identical LevelEq comparisons at nesting depths 100/200/400/800 required 54133/208233/816433/3232833 bind dispatches in the first free-poll representation. The continuation-passing representation requires 1816/3616/7216/14416, matching the old direct-state implementation exactly; all three returned identical results and remaining budgets. The instrumented generated-JavaScript comparison is retained under `build/budget-performance/`, with pinned old/new inputs. Timings include JIT warmup and are observations, not production latency promises. The older category timeout preceded callback support and is not attributed to this defect.

After the continuation correction, native `c/tests/kernel_budget_callbacks.c` passed (`.kanon-exec/run-Vhu2xm`) and a fresh official build of `c/tests/kernel_budget_callback_check.c` passed (`.kanon-exec/run-tpdMR1`) followed by its JS execution (`.kanon-exec/run-JFFIRi`). Public finite and callback runner APIs are unchanged. The prior independent callback review covered the previous representation; this correction still needs independent review and whole-suite replay.
