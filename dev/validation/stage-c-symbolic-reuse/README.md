# Symbolic family reuse validation

Date: 2026-09-16. Base: 65faa7e. The design is
`dev/M0-STAGE-C-SYMBOLIC-REUSE.md`.

`checks.json` records the final commands, exit statuses, gate counts and
failed gate names. `gates.stdout` and `gates.stderr` retain the initial
battery attempt. Its 30-minute capture limit expired after 35 passing
gates and three watchdog timeouts, leaving 23 gates unfinished. System load
reached 110.80 during the run. `gates-measures.txt` retains the completed
timing rows.

The scoped recheck verifies 383 source files against the tested snapshot,
then reuses those 35 passing results and runs the 26 failed or unfinished
gates with their original predicates and watchdog tiers. Its output is in
`rechecks.stdout` and `rechecks.stderr`; REUSED lines refer explicitly to
the initial capture. A final isolated retry runs the four runtime gates
that timed out in the scoped recheck, after verifying the same 383 source
files again. `runtime-retries.stdout` and `runtime-retries.stderr` retain
that attempt. `checks.json` records the combined verdict and both runner
hashes.

Across these attempts, 59 of 61 gates pass. The new symbolic-reuse runtime,
natural-transformation runtime and left-Kan runtime pass on their isolated
retry within the original limits. PRELUDE-HETEROGENEOUS-LEFT-KAN-RUNTIME
still times out during kernel evaluation and emission at its unchanged
110-second limit. TRUSTED-LINES reports kernel=4208/3000 and encoder=246/900;
the kernel bound remains an open ruling. This is an aggregate result, not
a passing uninterrupted battery. No watchdog or trusted-code bound changed.

`kernel.stdout` records the new suite: twelve precise source refusals, six
parser refusals, five raw API controls, source round trips, nested and
dependent sharing, closed reuse after symbolic sharing, computations and
mixed-universe category contracts. The group declaration alone leaves
caller globals empty, and the category client creates exactly three
families with no axioms.

`runtime.stdout` records four exports at payloads 37 and 41 on the kernel,
Node and Wasmtime. Repeated composition computes `(n + 2) * 2 + 5` for
objects and `(n + 3) * 2` for arrows. Identity returns n and n + 7.

`mutations.json` records four isolated compiler controls, each compiled
successfully and detected by the named suite verdict. It retains the edit,
source and mutant hashes, diagnostics, build output, and passing baseline
and restored runs. `sources.sha256` binds the production, test and gate
inputs used by this validation. Earlier validation directories describe
their own historical source snapshots.

Reproduce from the repository root, choosing a new directory for mutations:

```sh
zsh dev/dunecho.sh build
_build/default/test/template_symbolic_reuse.exe .
python3 -I test/reuse_runtime.py --symbolic
python3 -I dev/reuse-mutations.py /tmp/mechanism-symbolic-reuse-replay --symbolic
zsh dev/gates.sh
shasum -a 256 -c dev/validation/stage-c-symbolic-reuse/sources.sha256
```

The `review_delta` key of `checks.json` records the captured and final
hashes of the two paths that the review fixes changed, `surface/syntax.ml`
and `test/template_symbolic_reuse.ml`.
