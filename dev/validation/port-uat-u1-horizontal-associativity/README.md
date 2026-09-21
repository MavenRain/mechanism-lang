# Horizontal associativity validation

Base: 747ab466d1052433b26a21fe40705c848b34934e. Date: 2026-09-21.

The build, new kernel suite, runtime driver, pin-delta, frozen-denominator,
gate syntax, and diff checks pass. The `control-replay` check records the
proof-reflexivity control, which is rejected as expected. That recorded
control is a one-time run tied to `generated_source_sha256`. The gated
negative `reflexivity-control` of the kernel suite holds the same refusal on
every gate run. TRUSTED-LINES retains the inherited failure at
`kernel=5475/3000 encoder=246/900`. The full battery was not rerun.

The kernel result is 2,328 definitions, 15 checked families, six
computations, and seven refusals. Two fresh specializations use eight
distinct universe levels in opposite orders. Runtime specialization reuses
four distinct external categories. Inventories are exact, family positivity
certificates are checked, initial globals are unchanged, and no axioms are
introduced.

The runtime result covers both associations at three inputs on the kernel,
Node, and Wasmtime, with 12 external-host comparisons. Expected outputs
come from `103*x+814`; every observation consumes the checked associativity
law through an erased proof argument.

`checks.json` records exact commands, expected and observed exit codes,
complete capture hashes, and executable hashes. `all_checks_passed` is false
because of the inherited trusted-line failure; `slice_expectations_met`
is true. The control's exit code 1 is expected and must have the recorded
constructor-index mismatch prefix, rather than a timeout or unrelated error.

The `capture_artifact` values name `.kanon-exec/run-<id>` relative to the
build copy. The capture manifests sit in the gitignored `.kanon-exec` of
that build copy. Only the stdout and stderr hashes replay from the
repository root. The template input of the erased-object attempt was not
kept.

The review round recorded the `kernel` capture again at the repository root
after the count change. That row keeps the `cwd`, the manifest hash and the
`capture_artifact` of the first run.

`proof-reflexivity.py` regenerates the control from the pinned dependencies
and replaces only the law body, retaining all its lambda arguments.
Its generated input hash is recorded in `checks.json`. Replay from the root:

```sh
python3 -I dev/validation/port-uat-u1-horizontal-associativity/proof-reflexivity.py /tmp/hcomp-refl.mech
_build/default/bin/mech.exe check /tmp/hcomp-refl.mech
```

The second command must exit 1 with a `categoryRefl` constructor-index
mismatch. Its complete, large diagnostic is retained in the capture.

`attempts.json` and `attempts/` preserve the initial erased-object failure,
the unsupported targeted build invocation, the reused-family reference
failure, and the diagnostic-oracle recording run. These precede the final
validated source.

`sources.sha256` pins changed and supporting source files, including all
refusal oracles, complete prelude dependencies, documentation, and gate
registration. Validation captures are excluded to avoid circular hashes.
From the repository root:

```sh
shasum -a 256 -c dev/validation/port-uat-u1-horizontal-associativity/sources.sha256
```

Both new CATEGORY gates retain the existing 900-second watchdog and
the runtime driver retains its 540/480/30-second budgets. No existing
gate expectation or bound changes.
