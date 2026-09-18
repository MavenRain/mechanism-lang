# Shared left Kan validation

Base: bbebbff. Scoped validation for the 2026-09-17 increment.

The incremental build and the new kernel suite pass. The suite checks
944 definitions, 13 checked families, six computations and seven refusals.
The final runtime recheck passes all six exports on the kernel, Node and
Wasmtime at inputs 37 and 41 under the final harness budgets.

The initial 240-second emit budget expired. A diagnostic run passed with
278.820 seconds spent in emission. A subsequent run at the final
480-second emit limit expired under load above 40 while mutation replay
was concurrent. A serial run also expired at 480 seconds. The harness
now selects the functions each export can reach after the whole source
checks and erases. Type groups and postulates remain intact. The final
recheck passes with the same budgets. All three timeouts and the
diagnostic timing matrix are retained.

The helper's reference traversal self-test passes. A closure fixture
produces 42 on the kernel, Node and Wasmtime in both the original and
selected modes. Its WASM size decreases from 1832 to 1816 bytes when
unreachable code is omitted. `runtime-selection.json` retains the
fixture and all results. Existing runtime harnesses keep their original
mode; only this new harness opts into selection.

The mutation baseline and restored run pass. All five mutations fail for
their intended reasons. The first execution report counted sharing as
unkilled because its diagnostic pin was wrong. `mutation-predicates.json`
records the corrected result after verifying every source, executable,
mutant and output hash against the unchanged executions. The final
prefixes are pairwise distinct. The review round of 2026-09-17 restated
the `suite_source_sha256` row and the `executable_sha256` row of the
original report, and the two rows of `executables.sha256`, because the
suite source `test/prelude_shared_left_kan.ml` and `test/prelude_runtime.ml`
changed in that round and the binaries were rebuilt from the staged
sources during the review on 2026-09-17. The
`execution_results_sha256` row of `mutation-predicates.json` follows
that chain. No execution row, no output hash and no predicate result
changed.
`mutation-outputs.json.gz` retains the complete outputs, including the
large unit-accessor type diagnostic, without expanding the repository
by several megabytes of repeated type text. Run
`python3 -I dev/validation/port-uat-u1-shared-left-kan/verify-mutations.py`
from the repository root to verify sources, mutants, complete outputs
and final predicates. This reads retained execution evidence; it does
not execute the kernel or claim to validate a newly built binary.

Static checks cover Python parsing, shell syntax, whitespace, refusal
prefix separation, and exact preservation of all pre-existing gates and
tiers. The OCaml suite handles I/O and elaboration through `Result`.
The effscan formatting warning is recorded in checks.json; no claim of
a sound purity verdict is made. The axiom audit passes.

The inherited TRUSTED-LINES failure remains kernel=4208/3000 and
encoder=246/900. No counted source changed. The full gate battery was
not rerun for this prelude increment. Kernel, elaborator, WASM,
vendor and mapping sources, and frozen denominators are unchanged.

The kernel trace is retained losslessly in `kernel.stderr.gz`; its recorded
stderr hash applies to the decompressed bytes.

Commands and exits are in `checks.json`. `sources.sha256` and
`executables.sha256` pin the reviewed inputs and binaries. The source
hashes are relative to the repository root. The executable hashes are
the binaries rebuilt from the staged sources during the review on
2026-09-17; diagnostic captures preceded the helper change.
Reproduce with the build
command, the new suite, runtime harness and mutation driver named there.
