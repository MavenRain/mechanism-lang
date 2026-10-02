# Bend 2 migration

Status: completion batch under validation. Slices 1 through 6 imported the
Bend implementation and added live comparisons against OCaml. This batch
consolidates the remaining validation and build cutover on slice 6 (`7a2f9f2`).
It removes 115 retired OCaml and Dune paths and replaces the Veil submodule
with its pinned fixture snapshot. The external OCaml checkout remains the
validation oracle until the completion checks pass.

The compiler is pinned to Bend 2.0.27. The `.mech` language and existing
acceptance requirements are preserved. Build outputs live in `_bend2/`.
`make build` prepares the native production CLI and JavaScript test shards;
`make test` also prepares the native acceptance drivers. Live OCaml checks
require an explicit external checkout, for example
`make reference-check REFERENCE=/path/to/ocaml-checkout`.
Prepare native artifacts with `make acceptance-build` before running
`zsh dev/gates.sh`; its build leg retains the original 120-second watchdog.

Validation on 2026-09-29: all 12 live OCaml comparison suites passed, covering
3,877 cases with zero differences or drift. All 76 extra Bend regression
checks, the four reference refusal suites, 25 cache and recorder controls,
and the CPU auction and certificate checks passed. The full 87-leg acceptance
run completed with 67 passing legs and 20 failures, all on execution deadlines.
Native routing subsequently fixed the importer grammar timeout within its
original 30-second limit. All 24 native acceptance drivers and the isolated
CLI now compile, and prepared builds pass the original 120-second watchdog.
Bundle setup requires the complete mode inventory and retains each launcher's
installation checks; 56 adversarial bundle controls passed across seven groups.
The other deadline failures remain unresolved, including the separate
120-second heterogeneous left-Kan mutation baseline. No experimental kernel
cache, compiler upgrade or optimization flag change has been adopted. The
canonical checkout keeps the OCaml oracle until the remaining checks pass.

Completion review restored the shared mutation recorder's original execution
limits: 300 seconds by default, 180 for composition, 900 for reuse, and 120 for
the dependency-export fixture. Build limits also remain unchanged: 300 seconds
by default, 180 for composition, 1800 for reuse, 120 for dependency exports,
300 for closure, and 900 for Veil mutations. All 25 recorder controls passed
again, observing both build and execution limits on both backends. The updated
receipt is `dev/validation/bend2-completion/recorder-report.json`.

Current receipts are under `dev/validation/bend2-completion/`. The earlier
validation results below record the migration's previous development stages.

| Component | Bend implementation |
| --- | --- |
| Kernel, levels, erasure, circuits, termination | `bend2/kernel/` |
| Lexer, parser, elaboration, polymorphism, prelude catalogs | `bend2/surface/` |
| Lean export ingestion, translation, lowering, reports | `bend2/import/` |
| Wasm encoding, layout, closures, host effects | `bend2/wasm/` |
| `mech` and `mech-cert` | `bend2/cli/` |

Small C and JavaScript effects provide filesystem and process operations;
Python orchestrates builds and validation. The replacement does not invoke an
OCaml compiler or executable. The Bend checks replay frozen reference fixtures.
An optional live-reference verifier retrieves seven small OCaml test adapters
from Git commit `ed923e2130b8501ccbe500b538cfa3d39aa2bcda` into its evidence
directory. It rebuilds an external OCaml checkout in an isolated
output directory and compares observations without replacing expected outputs.
See `dev/BEND2-REFERENCE.md` for its scope, controls and validation record.

`dev/bend2-build.py` generates separate test entry points and records toolchain,
source and artifact hashes. JavaScript launchers raise the process stack within
the host limit. Native builds emit C and explicitly compile at `-O1`. This avoids
relying on the pinned compiler's default native optimization settings.
The consolidated source includes
the public precedence printer, three structural-zero predicate repairs, an
outer-scope closure short-circuit, and a default build with JavaScript test shards
and the full native production CLI. Successful production-only builds now publish
the source inventory required by R0-AUDIT, including through that mixed build.
An earlier `make acceptance-build` passed on the consolidated Bend sources in
2143 seconds within its original 3600-second limit. It rebuilt the JavaScript
test shards, native production CLI,
all 20 native protocol aliases, isolated native test CLI and template drivers.
Publication freshness, PIN-DELTA and R0-AUDIT passed for that build. Subsequent
review found that macOS compiler caches identified Apple's launcher shim without
its selected compiler. The builders now bind both identities, including the
resolver and selection environment. A later fresh acceptance build passed in
1,916 seconds with that correction. The current native setup covers 21 protocols
and the CLI.

The earlier staged replay, before this consolidated completion batch, recorded
64 passing and 23 failing checks:
22 timeouts and the existing trusted-line failure. Its extras suite passed 75
checks; the surface replay passed 926 cases with 949 known observations. These
records retain their original source and artifact pins.

The subsequent category mutation replay detected four controls, then timed out
on the fifth; five controls remained unrun. The functor harness detected all
four controls and passed its restored baseline, but a launcher-alias change
altered a pinned receipt and invalidated that phase. Native setup now selects
the same primary launcher as mutation setup. The fix passes exact receipt,
binary and alias comparisons across all 19 mutation-installer modes. The old
validation sequence is stopped; later phases require fresh bindings and runs.

Observed checks, each tied to its recorded source and artifact version:

- Exact frozen comparisons: JSON 166, parser 261, export reader 470, scoped
  translation 613, complete import pipeline 1,118 and kernel erasure 80.
- Core CLI comparisons: 28. Import and certificate CLI comparisons: 37,
  including outputs, diagnostics, exit status, published files and failure
  cleanup. One missing-parent diagnostic normalizes the original temporary
  directory name; the fixture preserves both raw observations.
- Filesystem and CLI effect tests: 15 each on JavaScript and native backends.
- Standalone mapping protocol: 14 assertions. Prelude protocol: 34 assertions
  plus its axiom audit. Wasm protocol: all 33 original cases, including exact
  WAT goldens, kernel checks and Node execution.
- Independent budget verification: 648 callback protocol cases, 81 boundary
  probes, lazy refusal, clock, importer, termination and real checker probes.
  The continuation implementation preserves results and poll counts while
  eliminating quadratic bind dispatch in the measured workload.
- Veil fixture provenance, all three shape mutation controls and all four
  closure mutation controls pass on Bend 2.0.27 with fresh isolated JavaScript
  builds. The fixture snapshot retains the pinned source hash and active overlays.
- The original kernel protocol passes all parser, checker, erasure, refusal,
  recursion and migrated-fixture groups. Four deliberate fixture corruptions
  are detected, and the restored baseline passes.
- The frontend assertion protocols and all 102 mutation controls pass on the
  final Bend 2.0.27 compiler and indexed Global implementation. The mutation
  run covers nine native and 93 JavaScript controls, with all 28 baseline and
  restored-baseline receipts passing and source hashes unchanged.
- Import grammar has 120 original assertions and lowering has 28. The complete
  11 MB Lean export corpus produces the original counts and byte-identical
  protocol output.
- The final numeric core passes 54 LEVELS assertions, projection regressions,
  exact conversion-budget boundaries and signed large-address regressions.
  Four LEVELS and two frontend compiler tests preserve negative-arity refusals
  that the Bend types exclude before execution.
- Final JavaScript checks cover all 37 unit modes, 261 parser goldens, 28 core
  CLI cases, runtime slicing and the original template-cost acceptance leg.
  Process and compiler boundary checks pass at their recorded revisions. The
  reviewed process helper now preserves timeout failures and captured output
  while propagating cancellation through cooperative nested helpers. Its
  bounded cleanup and reserved child environment metadata are documented in
  `dev/BEND2-GATES.md`. The original six-case regression and the separate
  three-case nested regression pass after integration. The ordinary all/extras
  suites now include the nested regression.

Source numeric payloads use signed arbitrary-precision integers. Native `Nat`
is reserved for finite list positions and binder indices. This preserves the
original parser's 18-digit input range and raw kernel metadata beyond Bend's
native integer range. Negative collection construction returns an explicit
error where the original helper raised a host exception. Oversized injection
metadata no longer allocates a width-sized list of absent expectations.

The semantic corpus contains 926 replay inputs, including the original category
recipe. The final Bend 2.0.27 JavaScript implementation matches all 926 frozen
outputs. All 21 frontend protocols pass their functional assertions. Timing
acceptance is recorded separately: two JavaScript protocols exceed their
original limits, while both have passed on native Bend under those limits.
Explicit case splits repair native Boolean-composition discrepancies in quantity
checking and the relational test oracle. Permanent regressions cover both fixes.
The final 216-case quantity matrix and 18-case oracle each pass with two runtime
arguments on both JavaScript and native backends.
Persistent string-trie indexes replace linked-list Global lookup while retaining
ordered enumeration, replacement and namespace semantics. Independent verification
passed 25,650 runtime-key assertions on each backend and 1,115,995 comparisons
against a separate reference model. The final permanent Global regression also
checks 270 assertions on runtime-derived keys.
Frozen source snippets supplement the original assertion suites; they do not
replace those suites. Frontend assertion coverage is tracked in
[`bend2/surface/COVERAGE.md`](bend2/surface/COVERAGE.md). All 18 relational assertion
protocols passed on earlier source revisions within their original limits.
Sixteen of 94 relational mutation controls passed at those recorded revisions;
fresh replays on the consolidated source remain pending. The
stricter 120-second heterogeneous-left-kan mutation baseline still times out.
A finer private diagnostic reaches that deadline during positive elaboration,
before any negative checks. It records 44.8 seconds in template composition and
25.6 and 25.1 seconds in two specializations. The three largest completed member
checks each take about 8 to 9 seconds. These intervals can include delayed payload
work, and observation changes runtime layout and demand. They do not establish a
baseline speedup or satisfy the acceptance gate.
A compilation failure never counts as a killed mutant.

The auction integration run passes combinatorial allocation, kernel certificates,
second-price settlement and schema validation. The category integration check
exceeds its original 240-second CLI checking limit. All 351 recorded inputs
remain unchanged; the category result is still a failure.
A subsequent full native CLI diagnostic checks the same program in 73.3 seconds
under the 240-second cap, with 256 stable input pins. The default build now targets
that complete native entry point. The original integration command still requires
a fresh run after rebuilding; its earlier JavaScript failure remains recorded.

The public Bend `Syntax.at` now reproduces the original precedence printer over
the port's AST domain. Independent review verified 1,008 core and 240 escaped-name
comparisons against the original OCaml oracle on JavaScript and native backends,
plus seven semantic controls. Existing `Syntax.show` output is preserved. The
units law test now calls the public printer with its original expected strings.
Its focused 17-control replay passes on JavaScript and native backends.
The pre-existing universe-variable representation still excludes negative indices.

Conversion now uses the original structural-zero predicate in the proposition
probe and family subsingleton checks. Scoped old/new JavaScript and native controls
pass, including 55 original-rule family decisions and 2,040 conversion, fuel and
recovery observations. Malformed raw-level differences are recorded separately.
The repaired plain protocol still times out at 120 seconds; no speedup is claimed.

Scope closure now stops when all remaining local levels lie outside the closing
scope. Independent review verified complete usages, errors and fuel across
23,040 cases, including a separate fixed-fuel replay, on JavaScript and native
backends. All three JavaScript semantic corruption controls were detected. The original
120-second protocol still times out; this change has no measured speedup claim.

Independent review also found a public elaborator difference when supplied a
raw expected collection type with inconsistent width metadata. The fix visits
exactly the requested number of legs, preserves closure-error ordering and
returns an explicit error for negative widths. Independent numeric and elaborator
boundary validation passed, including huge indices and zero-poll failures.

The acceptance script retains all 85 original named protocol and runtime
checks and their original watchdog tiers. Heterogeneous left-Kan retains its
110-second harness deadline and 120-second outer watchdog. Left-Kan laws retains
its 480-second emission limit, 540-second total limit, 30-second host checks and
900-second outer watchdog. Additional Bend units, compiler boundaries and frozen
replays run through `dev/bend2-test.py --suite extras`. The full `make test`
command also runs the original protocol ports. Diagnostic replay results are
separate from acceptance results and do not establish R3 performance.

The budget runtime now stores completed values and errors directly, while
deferring user functions until execution. Five focused protocol comparisons
pass on both JavaScript and native backends. A fresh isolated mixed-backend
suite passes all 77 checks, including both builds, with 926 surface cases and
949 known observations. The 18 unresolved baseline observations remain
unresolved. Review verified all 2,772 original and copied input hashes and
exactly three source differences before applying the runtime and two counting
adapters. These results do not replace the outstanding native canonical and
mutation runs. See
[`flat-suite-review/REVIEW.json`](build/final-kernel-runtime-publication-execution/flat-suite-review/REVIEW.json).

The trusted-code limits remain 3,000 kernel lines and 900 encoder lines. The
original kernel already exceeded its limit at 5,475 lines. The reviewed Bend
inventory contains 8,518 kernel lines and 289 encoder lines; the kernel limit
still fails. Inventory review does not approve larger limits. R3's required Bend
2 compilation-speed comparison remains **UNMEASURED**; historical OCaml timing
denominators are retained as historical evidence.

Completion requires the remaining protocol and mutation executions, fresh
integrated builds and replays, review of the source and validation changes,
and an accurate report of any acceptance gates that remain unmet.
