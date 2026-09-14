# Template composition validation

Base: bfc398b.  The vendor pin is
936a43a92dd59a04698648f24fa5ae94cdb532df.

The final composition suite checks 24 source refusals, 9 parser
refusals and 10 direct API cases.  Its positive checks cover exact
inventories, ordered definitions, independent universe arguments,
nested prefixes, normalization and parity with explicit closed
declarations.  The category fixture uses the current source prefix
through assoc.  It checks four category families and forty
definitions, including a functor shape at separate universe pairs.

The runtime fixture compares three exports at payloads 37 and 41 on
the kernel, Node and Wasmtime.  The function orders differ.  The
nested export doubles its payload and takes its type from the group
body member project.  Every host must return the
exact expected output.

The complete battery passes 46 of 47 legs.  Its only failure is the
inherited TRUSTED-LINES bound: kernel=4208/3000, encoder=246/900.
Every behavior leg passes and no watchdog expires.  The new source
gate takes 1.615 seconds and its runtime gate takes 1.204 seconds
in that run.  All seven composition and twelve inherited mutation
controls pass on the final sources, as do the final CLI and runtime
checks.  The trusted-line ruling remains open.

## Evidence and scope

`composition-mutations.json` records seven compiled controls and
passing baseline and restored suites.  `congruence-mutations.json`
records twelve inherited controls on the final sources.  Each report
retains source and mutant hashes, expected diagnostics, exit codes,
and build and suite output.  Compilation errors never count as a
killed mutant.

`gates.log` and `gates.json` retain the full battery and measurements.
That battery began before the final generated-name reservation.
`battery-inputs.json` records its implementation and fixture hashes.
Those captured hashes predate the review fix rounds, and the
`review_delta` key lists the six changed paths with their final
hashes.  Only surface/family_poly.ml, test/template_composition.ml and
dev/composition-mutations.py changed after that build.  The change
adds a guard within the new composition API, two source refusals,
and a control for the guard.  Existing declaration and specialization
paths kept their code.

`final-checks.json` binds the final source files to the restored
mutation build and records the CLI and runtime checks on that build.
Those captured hashes predate the review fix rounds, and the
`review_delta` key lists the six changed paths with their final
hashes.  The composition replay's restored suite checks the final guard and
the complete composition contract.  The inherited replay also uses
the final sources.  `prefix-collision.mech` is the self-contained CLI
reproduction; its checksum is in the final-check report.

`receipt.json` hashes each evidence artifact.  The composition
mutation tool replaces the copy path in captured output by `<copy>`
(dev/composition-mutations.py:27-28).  The inherited congruence replay
writes its captured output with no replacement
(dev/congruence-mutations.py:28-29).  Outputs and source
hashes otherwise retain their recorded values.  No source file or
gate predicate is replaced by a receipt.

## Reproduction

From the checkout root:

```sh
env -u OPAM_SWITCH_PREFIX -u CAML_LD_LIBRARY_PATH \
  zsh dev/dunecho.sh build
_build/default/test/template_composition.exe .
python3 -I test/composition_runtime.py
python3 -I dev/composition-mutations.py /tmp/mechanism-compose-replay
python3 -I dev/congruence-mutations.py /tmp/mechanism-congr-replay
```

Each replay directory must be new and outside the checkout.
To reproduce the review correction, run:

```sh
_build/default/bin/mech.exe check \
  dev/validation/stage-c-composition/prefix-collision.mech
```

It must exit 1, print no standard output and print this diagnostic
on standard error:

```text
mismatch: the name X_One is already declared
```
