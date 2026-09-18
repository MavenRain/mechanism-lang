# Veil kernel migration

The 2026-09-17 migration replaces the Kanon dependency at
936a43a92dd59a04698648f24fa5ae94cdb532df with Veil at
a7534cedeac82d396de8e23058ee6bc990560f65. This was the clean local
Veil HEAD when the user requested its current commit. Mechanism's
base commit was ba02c070f5b18c0310008d87a123d8de4a0c267e.

`vendor/veil` is the sole direct submodule. Its public upstream URL
is recorded in `.gitmodules`, and its gitlink, checkout and `PIN`
must agree. Initialize it with:

```sh
git submodule update --init vendor/veil
```

The build uses Veil's source files without its nested Tot development
checkout. CI initializes direct submodules only because Veil's nested
submodule URL is a local developer path.

Mechanism keeps its prenex universes, family templates, closure
handling, importer and prelude. The existing overlays were merged
against the previous pin, then compared against Veil in `PIN-DELTA.md`.
Veil's checker, erasure, shape rules, circuit reader, surface syntax
and emitter are active in Mechanism's own libraries. Namespace shims
still use `Kanon_kernel`, the namespace Veil itself retains.

The driver also inherits Veil's `circuit` and multi-file `build`
commands. `axioms` includes Veil's shape disclosures. No private shape
is hidden by the Mechanism CLI. `SPEC.md` records the new eight-shape
baseline; SPar remains unadmitted.

The shape audit includes SZk, SFhc and SMpc and permits the circuit
reader to inspect them. The trusted-source count now includes that
reader and its interface. The existing limits remain 3,000 kernel
lines and 900 encoder lines. The resulting count is 5,475 and 246,
so TRUSTED-LINES remains a failing gate. Frozen performance denominators
and previous validation records keep their historical Kanon pin.

## Regression coverage

- The inherited Veil kernel, surface and Wasm suites run against
  Mechanism's compiled libraries.
- VEIL-TEMPLATES checks universe substitution, family renaming and
  rejection of hidden free universes in all four new shape payload
  positions, including both SMpc arguments.
- VEIL-CIRCUIT runs Veil's 18 circuit-reader boundary cases against
  Mechanism's kernel. Its oracle holds the count `CIRCUIT-BOUNDS
  18/18`, the value that Veil's own battery holds, so a deleted case
  makes the leg red.
- VEIL-KERNEL compares checked forms, circuit reports and axiom
  disclosures with Veil's goldens for ZK, FHC and MPC. It builds the
  three host fixtures with the plaintext reactor and checks their
  outputs on Node and Wasmtime. It reads the circuit report from
  stdout and ignores the exit code of the circuit command, because
  that command exits 1 on a refused row by design.
- VEIL-KERNEL also diffs Veil's two circuit spine fixtures,
  mu-dependent-layout and one-fields, and test/circuit-spine.kan
  against their circuit goldens. The rule for a field of an
  introduction at SMu decides whether those rows read a depth or a
  refusal, so they follow the rules of lib/rules.ml.
- Two legs of Veil's battery are not inherited: HOST and HOST-NAT.
  They drive test/host/host-nat.kan, test/host/nat-bytes.kan and
  test/host/zk-instance.kan through Veil's own Node drivers. The Nat
  to bytes and blob handle imports of the reactor in wasm/emit.ml are
  therefore covered only by the three pack fixtures and their run
  goldens. This cut is deliberate and recorded here.

The standalone FHC shape example's `check --erased` command fails
with the same missing-expected-type diagnostic in both Mechanism and
Veil's existing binary. Veil's executable host fixture passes. The
shape example is therefore used for its checked, circuit and axiom
goldens, matching Veil's own gate coverage.

## Validation

The build and all three new gates pass. The CPU auction checks also
pass, including kernel certificates and second-price execution.

The full inherited battery completed with 57 passing legs and eight
failures: seven timeouts and TRUSTED-LINES. It ran before the three
Veil entries were added to the battery; those ran separately against
the final build. No production source changed during that run.

The timed-out legs were PRELUDE-HETEROGENEOUS-LEFT-KAN, its runtime
test, PRELUDE-CATEGORY-RUNTIME, PRELUDE-CATEGORY-ACCESSORS,
PRELUDE-FUNCTOR-RUNTIME, PRELUDE-NATTRANS-RUNTIME and
PRELUDE-LEFT-KAN-RUNTIME. The first uses a 300-second watchdog, the
heterogeneous runtime uses 110 seconds, and the other runtime drivers
use 210 seconds internally. No timeout was raised or bypassed.

Five timeout failures also occurred in focused runs on a clean build
of the original Kanon-based commit. The natural-transformation and
left Kan runtime tests passed on the clean baseline and then passed
on Veil rechecks under the same limits. The original timeout records
remain alongside those passing rechecks.

Full outputs, focused comparisons against a clean build of the
original Kanon-based commit, and final source hashes are recorded in
`validation/veil-kernel/`. These results do not constitute a passing
full battery.
