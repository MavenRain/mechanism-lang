# Frontend validation inventory

Commands use `/Users/oobi/.bend/bin/bend`, version 2.0.25. A direct invocation
checks and executes a test. The official JavaScript target is also used for
the differential source driver. Historical OCaml recorders and harvesters are
archived outside the worktree; the active replay requires only frozen data.

Run the frozen semantic gate with
`python3 -P dev/bend2-surface-check.py --driver _bend2/test/surface_check_driver.exe`.
The default timeout is 900 seconds per case, configurable with `--timeout`;
the aggregate gate allows 5400 seconds. The report distinguishes actual
mismatches, new execution timeouts, and 18 unresolved historical attempts.
`--require-complete-baseline` additionally refuses that unresolved historical
inventory instead of counting it as observed behavior.

| Original test family | Native coverage | Observed result |
| --- | --- | --- |
| Scanner, parser, prenex syntax | `surface_lexer`, `surface_parser`, `surface_poly`; parser oracle corpus | Native suites pass; 261 exact parser cases pass |
| Raw Poly traversal | `surface_transform` | Name and universe substitution under annotations, motives, branch bodies; late cancellation and scope rejection pass |
| Family catalog/member checking | `surface_catalog`, `surface_family` | Renaming, export planning, forged member rejection, callback visibility and first error, budget rejection pass |
| Family declaration scope and universality | `surface_boundary` | Header, parameter, index, constructor field, result parameter/index annotations, ignored shape metadata; universal field bound and mixed Prop/Type refusal pass |
| Family group and indexed reuse trust | `surface_trust` | Parameter permutation refusal, index/arity/recursion/positivity metadata mutation, ambient rechecking, mutual reuse refusal and exact diagnostic precedence pass |
| Family cancellation and rollback | `surface_boundary`, `surface_program` | Callback entry/member cancellation, nested composition/specialization exact budget cutoffs, preserved caller inventories and successful retry pass |
| Family universe instances | `surface_universes` | Eq at Prop/data/high carrier, singleton criterion, both Sum universe directions and original erased/relevant binder quantities pass |
| Prelude helper catalogs | `surface_prelude` | Dependent Pi/Sigma, equality operations, congruence and basic families execute; computed results and zero-budget rejection pass |
| Crypto positive source fixtures | `surface_crypto` | Seven original standalone zk/fhc/mpc fixtures pass |
| Hidden crypto shape metadata | `surface_shape_metadata` | All four original zk/fhc/mpc metadata scope and specialization cases pass |
| Template composition/reuse/group/export source refusals | Historical inline corpus | 216 exact semantic comparisons pass after correcting 20 parser diagnostics; separate larger corpus owned by parent |
| Runtime compatibility protocol and slicing | `prelude_runtime`, `runtime_slice` | 15 protocol/output/Wasm assertions pass; 28 original composition mutation assertions pass on Node and Wasmtime |

Recent execution artifacts:

- `surface_catalog`: `.kanon-exec/run-IoLWpN`
- `surface_program`: `.kanon-exec/run-5BLA2H`
- `surface_crypto`: `.kanon-exec/run-fFfwBp`
- `surface_prelude`: `.kanon-exec/run-9Lq7ag`
- `surface_trust`: `.kanon-exec/run-cWZzp1`
- `surface_boundary`: `.kanon-exec/run-xRv1en`
- `surface_universes`: `.kanon-exec/run-ChuTXg`
- `surface_shape_metadata`: `.kanon-exec/run-JXBLf1`
- Runtime adapter protocol: `.kanon-exec/run-1K1Vjk`
- Original composition runtime mutations: `.kanon-exec/run-ErTeFq`
- Historical inline semantic comparison, final driver: `.kanon-exec/run-NRyCX1`
- Parser corpus replay, final driver: `.kanon-exec/run-R0XfZ6`
- Focused malformed member/template diagnostics: `.kanon-exec/run-8TNeCp`

The historical inline input file is `dev/bend2/surface-extra-inputs.json`.
Its SHA256 is `bf3ab7bba59d7732bb8919be35c779c9855af6b8d9503cfb406ff0f48003106c`.
The retired harvester evaluated the original test definitions to preserve
concatenation and recorded original source and evaluated-prefix hashes.
Its exact source is archived outside the worktree, with its retirement receipt
in `build/bend2-surface-check/recorder-retirement.json`.
These source cases supplement the native raw API tests; they do not
stand in for raw trust-boundary assertions.
The independent comparison report is
`build/bend2-surface-check/extra-independent-report.json`; the initial 20
diagnostic differences are retained in `extra-independent-before-parser-fix.json`.

Runtime adapter reports are `build/bend2-prelude-runtime/report.json` and
`build/bend2-prelude-runtime/composition-report.json`, including driver and
fixture hashes. The adapter maps `test/prelude_runtime.ml`; its reachability
helper maps `test/runtime_slice.ml`.

Pending integration checks: final-source checked-form replay and the aggregate
compatibility gate. The installed frozen corpus now represents 949 completed
historical observations with 926 unique inputs, including the exact canonical
empty-global category call. Its initial 925-input replay had seven failures:
two synthetic timeouts and five large agreement stack crashes. Those receipts
remain unchanged. Tail-recursive UTF-8 traversal removed the scanner stack
problem, and the launcher raises the process stack before Node.

The later pinned lookup driver matched 918 cases within 120 seconds. Serial
900-second retries matched seven of the remaining eight exactly: category
220.25s, NatAdd787.06s, NatEq99.05s, NatLt98.76s, NatSub244.4s, synthetic
heterogeneous_nattrans254.6s and synthetic nattrans_laws415.2s. NatMul timed out
at900s and is still incomplete. These observations are in
`build/bend2-surface-check/heavy-900-report.json`; they precede the final shared
Big-width migration. The replay harness's configurable default is900s per case;
it changes no checker Budget assertion or existing performance threshold.

The original native accept-only oracle
parsed `prelude/cat/category.mech` to one declaration, then timed out after
120 seconds in checking with initial globals and no checked-form printing. This is recorded in
`build/bend2-surface-check/accept-only-category-report.json` with source,
object and binary hashes. The exact original `test/prelude_category.ml:16`
call uses empty globals. That reference call succeeded in 82.18 seconds,
recorded in `build/bend2-surface-check/canonical-category-baseline.json`.
Different initial-global contexts must not be treated as equivalent tests.
The full capture retains 18 unresolved reference attempts: two category source
contexts and 16 generated concatenations. Generated concatenations are
supplemental source probes, not reproductions of the named original unit suites.
An early default native source-driver build exited137 after checking. Explicit
C emission followed by Clang-O1 later succeeded, and the native driver matched
canonical category in133.13s. Official JavaScript emission also succeeded.
No frontend behavior is intentionally omitted or delegated to OCaml.
Negative raw arities are unrepresentable in the native `Nat` API. Paired
positive/negative compiler fixtures enforce both original family and composition
arity cases (`surface_protocol_boundaries.py`, assertions=2).

Earlier semantic driver SHA256 (historical receipt, not final-source evidence):
`df73e6e3d9ecdb29266a74bff17711601dc7cbbd2b4635e1c07b4781ea544712`.
The runtime adapter's tightened slice self-test passed separately with SHA256
`4700740b7a00428424ac088ff0de7c0893abf75a7efed84b71393eb3178b6fb4`,
recorded in `build/bend2-prelude-runtime/final-self-test.json`.

The original foundational protocols are mapped assertion by assertion in
`COVERAGE.md`. All19 individually passed across pinned builds, including the
repaired prenex18-digit universe check. The combined18-pass/one-regression
report is `build/bend2-foundation-protocols/complete-report.json`; the repaired
prenex passed separately in `run-eSivXo`. The original category clients inside
reuse, symbolic reuse and composition passed in280.6s,491.5s and26.5s.

There are 102 migrated frontend mutation controls. Anchors are unique; one
C-DEP-M1 kill was observed before the coordinated width migration suspended the
first run. The final corrected execution is recorded below. The earlier failed baseline
serializer synchronization attempt and interrupted second attempt remain in
`run-yqjM6E` and `run-vPAwSI`.

The full Big width/index integration passed the 19-protocol dispatcher typecheck
in `run-VZdEuS`. A fresh numeric driver compiled in `run-mH591A` and passed in
`run-cz0GvT`: all 16 original oracle results matched exactly, and the huge
width/index syntax round-tripped. Its marker is
`SURFACE-NUMBERS-OK oracle=16 huge-width-roundtrip=1`. The pinned inputs and
reference outputs are in `dev/bend2/surface-numeric-cases.json`.

The final 20-mode dispatcher compiled in `run-XIr5Qb` and all 20 protocols passed
in `run-rTKqWA`. The exact report is
`build/bend2-foundation-protocols/final-report.json`, including source hashes and
artifact SHA256 `9c6d64c46862218ea5fa913662dafcf608b16c94774345db3cc1c234ece67ef6`.
This build includes the full arbitrary-precision width/index migration. Its
reuse, symbolic reuse and composition protocols completed in 123.77, 193.77 and
23.00 seconds respectively under shared runtime load.

Independent verification then exposed a raw expected-value elaboration defect:
the collection helper evaluated every actual leg instead of exactly the
requested count. The correction preserves first-error order, ignores extra
legs, refuses the first missing leg, and normalizes negative raw widths to an
explicit error before expected-type inspection. `surface_expectations.bend`
adds 14 helper/public-elaboration cases, each with a zero-poll budget. The
corrected 21-protocol dispatcher passed all cases in
`build/bend2-foundation-protocols/corrected-report.json`.

The first complete native replay recorded 906 exact matches, 18 quantity
diagnostic mismatches and two 900-second timeouts. The same 18 quantity cases
matched on JavaScript. The kernel owner repaired the native-sensitive usage
predicate and validated 47 quantity cases. The corrected serial 926-input replay
completed with 924 exact matches and only the two arithmetic timeouts, recorded
in `build/bend2-surface-check/final-fixed-native-report.json` (`run-CQ09wb`).
The earlier reports remain unchanged, including interrupted JavaScript heavy
replay `run-obMaSx`.

The first complete mutation run recorded 93 of 102 controls killed. Four
failures were diagnostic naming/rendering differences. Four collision mutants
also survived in the final original OCaml sources because another reservation
still rejected the same input; the source-pinned archival counterfactual evidence
is referenced by `build/bend2-foundation-protocols/original-mutation-audit.json`
(`run-wDL7E7`). Their
corrected Bend mutants remove that reservation at both relevant layers. The
remaining budget mutant is covered by an additional specialization-entry
zero-poll assertion, retaining the original 60-poll assertion. The fresh corrected
run killed all 102 controls across 12 suites, with every baseline and restored
baseline passing (`run-h1cfAm`). Reports and per-suite source manifests are under
`build/bend2-frontend-mutations/`, indexed by `corrected-report.json`. These inputs
use the pre-indexed Global implementation; they are not evidence of mutation
execution against the later isolated lookup prototype.

The arithmetic timeouts were reproduced with 1,000 unrelated definitions before
a single proof. The original full addition and multiplication sources complete
in 2.816 and 17.226 seconds. An isolated prototype adds Base.Map indexes while
retaining the global entry and family lists, replacement order and persistent
snapshots. Its ten bounded workloads match the original outputs on both targets.
The full addition and multiplication cases also match exactly: JavaScript takes
18.604 and 86.627 seconds; native takes 4.456 and 25.630 seconds. These are shared
load observations, not isolated benchmark guarantees. Source hashes, build
receipts and exact output comparisons are under `build/indexed-global-prototype/`.
The full isolated native replay then passed all 926 frozen cases exactly
(`run-iZ3XU7`, `build/indexed-global-prototype/corpus-report.json`). Canonical
category templates passed in 165.937 seconds under shared load. This establishes
prototype behavior. Independent verification approved the change after dynamic
key assertions on both targets and reference-map comparisons. The guarded
16-file landing preserved the newer relational test helper. The permanent
`kernel_global_index.bend` fixture retains the fixed cases and adds 270 assertions
from three runtime arguments, including NUL derivatives and persistent snapshots.
Its Bend 2.0.27 execution and missing-argument rejection both pass; the final
source hashes are in `build/indexed-global-prototype/final-freeze.json`.

Fresh Bend 2.0.27 frontend artifacts are recorded under
`build/frontend-final-2.0.27/`, with identical source/tool hashes before and after
each build. Their full 21-protocol and 926-case reruns are tracked separately
from the earlier prototype and mutation evidence.
The final JavaScript replay matches all 926 frozen cases exactly, with zero
failures in 1354.64 seconds. Its report is
`build/frontend-final-2.0.27/corpus-report.json`. The 18 historical unresolved
original baseline observations remain explicitly unresolved, outside those
926 observed cases.
All 21 final protocols passed their functional assertions in `run-yvyjD7`, recorded by
`build/bend2-foundation-protocols/final-2.0.27-report.json`. Reuse, symbolic reuse
and composition completed in 286.33, 211.89 and 26.71 seconds under shared load.
The original gate limits are separate from the recorder's 900-second cap:
reuse and symbolic reuse both fail their original SLOW 120-second tier.
The other 17 original runtime components remain within their original FAST
10-second or MED 30-second tiers. Numbers and expectation regressions are new
protocols with no original tier. Exact classification and the original gate
source hash are in `final-2.0.27-timing.json` beside the functional report.

A fresh two-protocol native Bend 2.0.27 comparison used unchanged assertions and
the original 120-second caps. Reuse passed in 77.06 seconds with empty stderr;
symbolic reuse timed out at 120.05 seconds. This is an unresolved timing failure,
not a passing gate. Source, compiler, C-header and artifact pins are recorded in
`build/frontend-template-native-2.0.27/`, with runtime receipt `run-cdapLY`.
The comparison did not change backend routing or default time limits.
The subsequent bounded profiling run completed symbolic reuse naturally in
55.23 seconds with its exact marker and empty stderr, using the same artifact
and unchanged pins (`run-gfi1Yq`). Both observations are retained. Shared-load
timing varies materially; the successful profile does not erase the earlier
gate timeout. Its five-second active-thread sample shows closure dispatch
31.57%, reference dropping 23.78%, and visible Budget functions 4.14%.
