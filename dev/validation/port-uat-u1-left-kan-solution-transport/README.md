# Left Kan solution transport validation

`kernel.stdout` records the focused native gate: two constructors, two
independent universe contracts, eight computations and six refusals.
`native-oracles.stdout` retains the complete native refusal messages.
Its final empty line is normalized in the text copy; `native-oracles-raw.json`
retains exact stdout, its byte count and SHA-256 hash.
`refusal-pins.json` records their prefixes, MD5 and SHA-256 digests.

`runtime.stdout` records the final runtime attempt reaching the unchanged
480-second emission limit. Complete host comparisons remain unverified.
The optional mode is configured for four certified exports at inputs 37
and 41 on the kernel, Node and Wasmtime. `runtime-reuse.json`
pins the reused driver, compiler and all 118 verified inputs.
`proof-consumption.stdout` records four refused reference substitutions.
`runtime-large-oracles.json` and its captures retain the initial run that
reached the unchanged 480-second emission limit. The dedicated runtime
fixture uses smaller affine functions while the native computation fixture
retains its larger arithmetic cases.
`runtime-compact-eager.json` and its captures retain the smaller-oracle
attempt that also reached the limit. The final fixture wraps quantity-zero
proof arguments in checked functions to defer eager proof evaluation;
the final attempt also reached the limit.

`legacy-extractions.json` pins ten older focused programs that remain
byte-identical to base f230f34. `selectors.stdout` records 116 passing
build-selector checks. `full-driver-check.stdout` records the Bend check
of the updated full left Kan harness. `ocaml-check.json` records the
reference check of the focused source.
`runtime-source-check.json` records the separate reference check of the
final runtime fixture. Its execution and emission performance remain open.

Each execution receipt records the command, exit code, elapsed time and
capture sizes. `native-build.json` retains the native binary fingerprint.
`focused-source.mech` and `runtime-focused-source.mech` retain the checked
contracts and runtime programs, with corresponding extraction inventories.
`checks.json` pins the final inputs and scopes the recorded validation.
`source-checks.stdout` records 33 checked paths. That check does not
include `dev/M0-BUILD-LOG.md`; `git diff --cached --check` covers it.

The full gate battery, full left Kan suite and older runtime modes were
not rerun. Existing validation time limits are preserved.
