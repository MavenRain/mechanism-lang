# Iterated whiskering validation

Base: aaeb4bd331047957601249d4eb2193e1b77cfd05. Date: 2026-09-20.

The new kernel suite and runtime driver pass. The build, pin-delta,
frozen-denominator, gate shell syntax, and diff formatting checks pass.
TRUSTED-LINES remains the inherited failure at
`kernel=5475/3000 encoder=246/900`. The full battery was not rerun.

`checks.json` records exact commands, exit codes, capture hashes, and
the three executable hashes. Its `all_checks_passed` field is false
because of the inherited failure; `slice_expectations_met` is true.
Each check has complete stdout and stderr files beside this document.

The kernel result is 902 definitions, 14 checked families, 18 computations,
and six refusals. Two fresh specializations use eight distinct universe
levels in opposite orders. Runtime specialization binds four distinct
external categories to the four roots. The tests verify exact
inventories, checked family certificates, unchanged initial globals, and
no axioms.

The runtime result covers six functions at three payloads on the kernel,
Node, and Wasmtime, with 36 external-host comparisons. Every observation
consumes one of the checked laws through an erased proof argument.

`attempts.json` and `attempts/` preserve earlier development outcomes.
They include the unsupported targeted build invocation, the tuple
annotation error, diagnostic-oracle recording, and the sandbox failure
of the first pin-delta attempt. These attempts do not describe the final
source. The final successful captures are listed in `checks.json`.

`sources.sha256` pins 45 changed or supporting source files,
including the suite, fixtures, complete refusal oracles, prelude dependencies,
gate registration, build configuration, and documentation. Validation
captures are excluded to avoid circular hashes. From the repository root:

```sh
shasum -a 256 -c dev/validation/port-uat-u1-iterated-whiskering/sources.sha256
```

The two new CATEGORY gates retain the established 900-second watchdog.
No existing gate expectation or budget changes.
