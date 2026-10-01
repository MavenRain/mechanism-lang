# U1 left Kan mediator round-trip validation

Base: `c76fe38`. The dedicated native kernel gate passes two laws,
two mixed-universe contracts, eight computations and four refusals.
The OCaml checker accepts the same final focused program. Four certified
exports pass on the kernel, Node and Wasmtime at inputs 37 and 41,
for 24 comparisons.

The captures in this directory retain commands, exit codes and complete
stdout and stderr. `checks.json` pins the changed sources.
`oracles.stdout.json` preserves the oracle stdout as a JSON string, including
its trailing blank line, with the original byte hash.
`focused-source.mech` retains the selected definition bodies verbatim;
`focused-extraction.json` pins every input. `kernel-build.json` records
the native driver's signature and binary hash. `runtime-reuse.json`
records the reused driver's hash and all 118 checked input hashes.

- `build.*`: pinned Bend native driver build.
- `oracles.*`: four distinct mismatch diagnostics recorded under
  `test/neg/left-kan-roundtrip/` and subsequently checked by the kernel gate.
- `kernel.*`: `PRELUDE-LEFT-KAN-ROUNDTRIP-OK laws=2 contracts=2 computations=8 negatives=4`.
- `ocaml-check.*`: final focused program accepted by the OCaml checker.
- `runtime.*`: `PRELUDE-LEFT-KAN-LAWS-RUNTIME OK cases=4 hosts=3 payloads=2 comparisons=24`.
- `selectors.*`: 114 build-selector regressions pass.
- `proof-consumption.*`: positive extraction passes; replacing a certified
  export with its reference function is rejected for the missing proof.
- `extraction-regressions.json`: six existing extraction modes are
  byte-identical to the committed base, with output hashes.

The `runtime-initial.*`, `runtime-concurrent.*`, `runtime-isolated.*`
and `runtime-function-concurrent.*` captures retain earlier development
attempts. `runtime.*` records the final isolated run.

The full battery, full left Kan suite and older runtime modes were not
rerun. Existing test budgets and compiler PIN are unchanged. No kernel or
Wasm compiler code changes, and no axiom is introduced.

Commands and API details are in `dev/PORT-UAT-U1-LEFT-KAN-ROUNDTRIP.md`.
