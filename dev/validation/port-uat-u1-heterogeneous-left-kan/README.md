# Heterogeneous left Kan extension validation

Base: 171513f5fbfb48690789ddb6544883fa5f20c34d. Recorded on 2026-09-15.

## Results

- Build: PASS, zero warnings.
- Kernel suite: PASS, 294 entries, three left Kan specializations,
  two category cores, four computations and fifteen refusals.
- Runtime: PASS, four exports at payloads 37 and 41 on the kernel,
  Node and Wasmtime.
- CLI arity refusal: PASS, five universe arguments where six are
  required.
- Mutation replay: eight controls caught, zero timeouts, distinct
  diagnostics, restored source hashes and restored suite PASS.
- Full battery: 55 of 57 PASS, exit 1. The new kernel gate
  reached its initial 120-second watchdog. TRUSTED-LINES also failed,
  at kernel=4208/3000 and encoder=246/900.
- Kernel recheck: PASS under the final 300-second SUITE watchdog.
  The recheck resolves this leg, so 56 of 57 gates hold across the
  battery capture and the recheck; TRUSTED-LINES remains the failure.

The trusted-line limit is inherited. The new kernel gate uses the
existing SUITE tier, as the original left Kan suite does. Its recheck
took 81830.000 ms. The runtime gate took 79688.763 ms in the
battery under the 120-second SLOW tier and its 110-second inner budget.
The battery and recheck are separate captures. The only subsequent
gate-source change is the new kernel leg's tier. gate-tier-change.json
binds both versions, and checks.json retains both source fingerprints.

## Mutation controls

| Control | Result | Elapsed ms |
| --- | --- | ---: |
| uniqueness-proof | caught | 20182 |
| factor-projection | caught | 17529 |
| solution-sort | caught | 8682 |
| unit-projection | caught | 17071 |
| functor-map | caught | 62401 |
| mediator-component | caught | 54457 |
| cocone-selection | caught | 56395 |
| negative-corpus | caught | 62146 |

The replay mutates temporary source copies. A timeout is a failure,
never a caught control. The restored suite runs after all controls.
Its output, the source hashes and the executable hash are stored in
mutations.json. Review round 1 adds a ninth control,
fixture-solution-pin, on the generic fixture; the rows above are the
eight-control capture that precedes it.

## Evidence and reproduction

Each named check has a .log and .stderr capture. suite.stderr and
kernel-recheck.stderr hold /usr/bin/time output. The other stderr
files are empty except arity.stderr, which holds the exact refusal.
checks.json binds command results, executable hashes and evidence.
sources.sha256 binds all selected source files to their final bytes.
The review fixture comment and the ninth control moved two sources past
their captured rows; the review_delta key of checks.json and
mutations.json records the captured and the final sha256 of each.
checks.json also explains the moved evidence row of mutations.json
itself, which its own review_delta key shifted past its captured row.

Run from the repository root:

```sh
zsh dev/dunecho.sh build
_build/default/test/prelude_heterogeneous_left_kan.exe .
python3 -I test/heterogeneous_left_kan_runtime.py
python3 -I dev/heterogeneous-left-kan-mutations.py \
  /tmp/left-kan-replay.json
zsh dev/gates.sh
```

The replay destination must not exist. To reproduce the arity check,
concatenate the four prelude files listed in the design document,
append `specialize MechHeterogeneousLeftKan (0, 1, 2, 3, 4) as Bad`,
and run `_build/default/bin/mech.exe check FILE` on that input.
