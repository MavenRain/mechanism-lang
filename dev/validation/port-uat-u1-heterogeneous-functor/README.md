# Heterogeneous functor validation

Base: 75b835e64b73348d7303a4c207b946959bb17bb0.
Validation was run on the canonical repository.
The code and fixture hashes are in `sources.sha256`. The mutation replay
also records every source input and the exact test executable hash.

The full build passed with zero errors and warnings. The new kernel gate
passed in 9.496 seconds, checking 90 entries, three instances,
three computations and twelve refusals. The new runtime gate passed in
7.737 seconds, comparing three exports on the kernel, Node
and Wasmtime at payloads 37 and 41. Both use SLOW (120 seconds).

All six mutation controls were detected, including type-correct changes
to object and arrow computation. The restored suite passed. Source and
executable hashes were checked again before copying the evidence.

The full battery passed 41 of 49 gates. Its failures were:

- FAIL PRELUDE-CATEGORY
- FAIL PRELUDE-FUNCTOR
- FAIL PRELUDE-CATEGORY-RUNTIME
- FAIL PRELUDE-CATEGORY-ACCESSORS
- FAIL PRELUDE-FUNCTOR-RUNTIME
- FAIL PRELUDE-NATTRANS-RUNTIME
- FAIL PRELUDE-LEFT-KAN-RUNTIME
- FAIL TRUSTED-LINES

Watchdog exits (code 124): PRELUDE-CATEGORY, PRELUDE-FUNCTOR.
The five runtime failures are subprocess timeouts after 210 seconds.
The complete failed-leg diagnostics and all timings are in `gates.log`.
The battery ran under heavy shared machine load, which reached about
102 during the first category suite. Compiler, vendor, mapping,
denominator and trusted-line bound files have no changes from the base.
The full battery result remains red when any of these recorded gates fail.

The category and functor suites were rerun separately at starting load
27.33, alongside the battery's runtime checks. Their reports retain
the command result, load, timeout, source hash and executable hash.
The original battery rows above remain unchanged.

- category: PASS, 551.434 seconds, unchanged 900-second limit.
- functor: PASS, 517.899 seconds, unchanged 900-second limit.

Unresolved after the standalone reruns:
PRELUDE-CATEGORY-RUNTIME, PRELUDE-CATEGORY-ACCESSORS,
PRELUDE-FUNCTOR-RUNTIME, PRELUDE-NATTRANS-RUNTIME,
PRELUDE-LEFT-KAN-RUNTIME, TRUSTED-LINES.

Files:

- `checks.json`: verdicts, timings, base and executable hash.
- `gates.log` and `gates.stderr`: complete captured battery output.
- `mutations.json`: six controls and the restored suite, with source hashes.
- `reruns.json`: standalone results for the two expired suites.
- `sources.sha256`: code and fixtures, relative to the repository root.

Reproduce the new checks after `zsh dev/dunecho.sh build`:

```sh
_build/default/test/prelude_heterogeneous_functor.exe .
python3 -I test/heterogeneous_functor_runtime.py
python3 -I dev/heterogeneous-functor-mutations.py /tmp/heterogeneous-replay.json
```

The replay destination must not already exist. Run the full battery with
`zsh dev/gates.sh`. No existing watchdog or trusted-line bound was changed.

Review round 1 fixed the suite source and the mutation script after this
bundle was captured; `checks.json` and `mutations.json` each carry a
`review_delta` key with the captured and final hashes of the changed
paths, and the captured rows above stay as recorded.
