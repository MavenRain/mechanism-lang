# Composable functor validation

Base: c1a60759225799d2617c610a46b4db750a0333a6.
Validation ran in the canonical repository. `sources.sha256` pins the
source inputs; `checks.json` records the executable hashes, focused
command results and every full-battery measurement. `mutations.json`
retains the replay inputs, seven distinct failures and the restored
suite. Its hashes were compared against the final source and binary.

The build passed with zero errors and warnings. The kernel suite
checks 158 definitions, three instances, four computations and twelve
refusals. It passed in 17.113 seconds in the battery.
The runtime gate passed in 24.947 seconds, comparing four
exports on the kernel, Node and Wasmtime at payloads 37 and 41.
Both gates use the existing SLOW, 120-second watchdog.

All seven mutation controls were detected with distinct outputs, and
the restored suite passed in 38.040 seconds.
The unchanged heterogeneous-functor regression suite also passed.

Full battery: 47 of 51 PASS, exit 1.
Failed gates:

- PRELUDE-LEFT-KAN
- PRELUDE-CATEGORY-RUNTIME
- PRELUDE-CATEGORY-ACCESSORS
- TRUSTED-LINES

Watchdog timeout: PRELUDE-LEFT-KAN, exit 124.
Subprocess timeout: PRELUDE-CATEGORY-RUNTIME, 210 seconds.
Subprocess timeout: PRELUDE-CATEGORY-ACCESSORS, 210 seconds.

The complete results are retained in `gates.log` and `gates.stderr`.
The inherited trusted-kernel bound remains 3,000 lines. No compiler,
vendor, mapping, denominator or existing watchdog source was changed.

Reproduce after `zsh dev/dunecho.sh build`:

```sh
_build/default/test/prelude_composable_functors.exe .
python3 -I test/composable_functors_runtime.py
python3 -I dev/composable-functors-mutations.py /tmp/composable-replay.json
zsh dev/gates.sh
```

The replay destination must not already exist. Verify the input files
from the repository root with `shasum -a 256 -c` and the bundle's
`sources.sha256`. Hashes of the source list, logs and mutation report
are recorded in `checks.json`.

The 2026-09-14 review fixed prelude/cat/composable-functors.mech,
test/prelude_composable_functors.ml and
dev/composable-functors-mutations.py after this capture. The captured
rows above stay as they are. See the `review_delta` key of
`mutations.json` for the captured and the final hash of each path. checks.json carries the same key for its `executables` row (the review fixes rebuilt the suite executable) and for the `evidence_sha256` row of mutations.json.
