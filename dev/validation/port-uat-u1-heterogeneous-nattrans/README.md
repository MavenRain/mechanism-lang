# Heterogeneous natural transformation validation

Base: d89fe5142c0dbe4b2ae1af0ecbc4298654e2ef6d. Recorded on 2026-09-14.

The compiled kernel suite passed with 150 entries, four specializations,
five computations and 13 refusals. The earlier runtime harness passed
five exports on the kernel, Node and Wasmtime at payloads 37 and 41.
All eight mutation
controls failed as expected with distinct diagnostics, and the restored
suite passed. The build reported zero errors and warnings.

Full battery before runtime harness batching: 41 of 53 PASS. Failed rows:

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


Bounded rechecks after the full battery, before runtime harness batching:

- heterogeneous-nattrans-runtime: FAIL (exit 1, 110327 ms)
- composable-functors: FAIL (exit 124, 120075 ms)

The final runtime harness checks both payloads in one program, avoiding
repeated checking of the category and functor definitions. It retains all
ten kernel results and twenty WASM host comparisons. Its first check under the
shipped 110-second total budget and 120-second outer timeout was
FAIL (exit 1, 111538 ms). Read runtime-final.json for the exact output and inputs.

The separate diagnostic was PASS (exit 0, 63103 ms). Its runner changes only
the two 110-second budget constants to 900 in memory, with a 930-second
outer timeout. Every comparison and the ten-second host limits remain.
This diagnostic does not establish a pass within the shipped gate budget.

The subsequent unchanged runtime harness check under the shipped
110-second total budget and 120-second outer timeout was
FAIL (exit 1, 110298 ms). Read runtime-confirmation.json for its exact output
and input hashes. Earlier timeout records remain part of this evidence.

TRUSTED-LINES retains the inherited kernel count of 4208 against its
3000-line bound; the encoder remains at 246 against 900. The slice does
not change these sources or limits. Read gates.log for all gate rows
and the measured durations and exit codes.

Initial source/runtime checks encountered timeouts while machine
load rose above 500. The kernel diagnostic run used optional phase
tracing and a 900-second outer ceiling. The normal runtime result used
the shipped 110-second total budget. Compilation shares that budget
with emission and host comparisons; host calls retain ten-second caps.
The two new battery legs retain the existing SLOW watchdog.

Reproduce from the repository root:

```sh
zsh dev/dunecho.sh build
_build/default/test/prelude_heterogeneous_nattrans.exe .
python3 -I test/heterogeneous_nattrans_runtime.py
python3 -I dev/heterogeneous-nattrans-mutations.py OUTPUT.json
zsh dev/gates.sh
```

To reproduce the longer diagnostic separately:

```sh
python3 -I dev/validation/port-uat-u1-heterogeneous-nattrans/runtime-diagnostic.py OUTPUT.json
```

Use a fresh OUTPUT.json path. The mutation runner refuses overwrites.
Add --trace after the kernel suite's root argument to print its phases
and refusal diagnostics.

sources.sha256 binds the code, fixtures, refusal prefixes and gate wiring.
checks.json binds the executable hashes and the retained evidence files.
kernel.log and build.log contain the successful focused results;
runtime.log is the earlier harness's passing result. The kernel trace's
full stderr is identified by its hash in
checks.json; it is omitted here because it expands the refused types.
mutations.json is the direct replay report, with source and executable
hashes, all control results and the restored result. static.json records
the syntax, mutation-anchor, whitespace and unchanged-source checks.
reruns.json records both earlier bounded rechecks and their input hashes.
runtime-harness-before.txt preserves that harness. checks.json binds its
hash separately from the final source hash. runtime-final.json records the
first bounded check of the final harness. runtime-diagnostic.json records
the separate longer diagnostic and the runner's hash; runtime-confirmation.json
records the subsequent check under the unchanged shipped deadlines.
The design and remaining work are in
dev/PORT-UAT-U1-HETEROGENEOUS-NATTRANS.md.
