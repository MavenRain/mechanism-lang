# Category validation evidence

Base: 1f5d7bf18d490dd7af13977cd3bcc12bd12a6d7a.
Validated build: `mechanism-category`.
The review round of 2026-09-10 restaged the mutation rows and the suite
line. The gate logs stay as they were recorded.

- `build.log`: clean restored build, zero errors and warnings.
- `category-suite.log`: 49 definitions, seven computations, nine
  refusals, three quotation checks. The suite also checks seven
  families and four instances.
- `mutations.json`: ten isolated controls killed, restored suite passed.
- `results.json`: the same replay report under the name that
  `--verify` reads.
- `C-CAT-M*.stdout` and `C-CAT-M*.stderr`: the recorded output of every
  control, so the stricter verification is repeatable in the repository.
- `mutation-verification.json`: stricter diagnostics verified against
  the saved outputs and matching current source hashes, ten verified.
- `gates-initial.log`: 35 passing legs, only the trusted bound fails.
- `gates-final.log`: 34 passing legs, trusted bound failure and a
  30-second concrete-category runtime watchdog expiry.
- `runtime-retry.log`: isolated concrete-category runtime retry passed
  under `gtimeout 30`, the same ceiling used by the battery.
- `generic-accessors.log`: the separate generic-accessor probe reports
  eight WASM cast failures and ends `host_checks=8 failing_checks=8`.
  Its source checks and kernel answers pass.

The recorded gate logs come from before the harness change of the
review round. The concrete category mode now makes twelve
subprocesses. The mutation variant runs only the export whose answer
depends on the mutated definition, and the category modes make no
separate check pass.

The full battery and scoped retry jointly cover all 35 behavioral legs.
No timeout or acceptance threshold changed. TRUSTED-LINES remains open
at kernel=4208/3000 and encoder=246/900. Generic category accessor WASM
parity remains open, as recorded in `dev/M0-STAGE-C-CATEGORY.md`.

Commands use `env -u OPAM_SWITCH_PREFIX -u CAML_LD_LIBRARY_PATH`:

```sh
zsh dev/gates.sh
gtimeout 30 python3 -P test/prenex_runtime.py --category
python3 -P test/prenex_runtime.py --category-accessors
python3 -I dev/category-mutations.py NEW_WORK_DIRECTORY
cp -R dev/validation/stage-c-category OUT_OF_TREE_COPY
python3 -I dev/category-mutations.py --verify OUT_OF_TREE_COPY
```

The `--verify` mode writes `verification.json` into its work directory.
Copy this evidence directory to a location outside the repository
first, because no tool writes inside the repository.

The accessor diagnostic intentionally returns nonzero. It is separate
from the concrete-projection gate. A host trap prints `host_checks=`,
and a failing kernel-side invoke prints `kernel_checks=`. The vendored
pin, denominator files, mapping verdicts, watchdog tiers and trusted
bounds remain unchanged.
