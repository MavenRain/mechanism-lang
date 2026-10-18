# Historical frontend assertion coverage

This table records the Bend 2 port of commit 8c90b64: it distinguishes executed assertions of its Bend units, frozen source observations,
and unfinished original-suite parity. A source accepted or rejected by the
semantic corpus does not replace a raw API assertion, a trusted-global audit,
a computed-value assertion, or a mutation negative control. Existing unit
execution evidence is listed in [VALIDATION.md](VALIDATION.md).

The historical inline corpus has 216 exact observed matches. Its original
source and evaluated-prefix hashes are in
`dev/cases/surface-extra-inputs.json`. The broader frozen replay has 926 unique canonicalized inputs. On the pinned
pre-final driver, 925 matched exactly after serial 900-second retries; the
remaining NatMul case timed out at 900 seconds. The later corrected native replay
matched 924 cases and timed out on both arithmetic agreement sources. The isolated
indexed Global prototype matches all 926 frozen cases exactly; its source review
and guarded landing are complete. The final indexed Bend 2.0.27 JavaScript replay (commit 8c90b64)
also matches all 926 cases exactly, with zero failures. Generated `group:*` inputs concatenate quoted paths;
they do not reproduce the corresponding original suite setup.

| Original test | Assertions already represented by the Bend units of commit 8c90b64 (now the C drivers) | Remaining original-suite or mutation-driver work |
| --- | --- | --- |
| `equality.ml` | `c/tests/surface_equality.c` preserves all 15 original named assertions, including Prop index universes, singleton elimination, field quantities, scope and exact callback poll boundary. All 15 executed successfully. | No original assertion gap; rerun in the final aggregate after later kernel changes. |
| `family_poly.ml` | `c/tests/surface_family_poly.c` preserves 33 executable original named cases in order, exact quantities and scopes, recursive fields/motive renaming, universality, all original collision placements, ambient rechecking, rollback and shared substitution-budget measurements. Runtime passed. | The original negative signed arity is enforced by a positive/negative compiler pair in `surface_protocol_boundaries.py`; this is a separate assertion, not a fabricated runtime case. |
| `prelude_poly.ml` | `c/tests/surface_prelude_poly.c` preserves the two catalog arities, five concrete instances, audits and two nominal/indexed refusals. Runtime passed. | No original assertion gap. |
| `prelude_transport.ml` | `c/tests/surface_prelude_transport.c` preserves exact catalog/member inventory, seven instances, chosen exports over a reused family, computation witnesses, audits and five scope-specific refusals. Runtime passed. | All ten transport/member mutation controls killed, with baseline and restored baseline passing. |
| `family_members.ml` | `c/tests/surface_family_members.c` ports all 35 original named cases in exact order, including full export collision/name planning, hidden metadata, rollback and original extra-poll equalities 16 and 13. Runtime passed. | All ten associated transport mutation controls killed. |
| `prelude_equality_ops.ml` | `c/tests/surface_prelude_equality_ops.c` preserves all ten instance contracts, eleven exact normalizations, audit and eight good/bad controls. Runtime passed. | All ten mutation controls killed, with baseline and restored baseline passing. |
| `prelude_dependent.ml` | `c/tests/surface_prelude_dependent.c` preserves six arities, four instance groups plus five additional Pi instances, 29-entry count, contracts, twelve exact normalizations, audit, six controls and callback cutoff. Runtime passed. | All ten mutation controls killed, with baseline and restored baseline passing. |
| `prelude_congruence.ml` | `c/tests/surface_prelude_congruence.c` preserves catalog inventory, callback cutoff, eight contracts, symbolic-name isolation, four exact normalizations, audit and five good/bad controls. Runtime passed. | All twelve congruence/group mutation controls killed, with baseline and restored baseline passing. |
| `family_groups.ml` | `c/tests/surface_family_groups.c` preserves all 31 original names/order, callback visibility and parity, exact scope/body/first-error refusals, companion specialization, collisions/rollback and entry cutoffs. Runtime passed. | Associated congruence and prenex-group mutation replays passed. Callback cancellation uses the equivalent measured callback allowance and a failing sentinel for forbidden callbacks. |
| `prenex.ml` | `c/tests/surface_prenex.c` preserves all original row order, isolation/audit, four exact computations, 25 refusals and the 18-digit universe roundtrip/check. Runtime passed after moving universe and Type literals to Bignum. | All seven controls killed; final aggregate regeneration remains separate. |
| `prenex_families.ml` | `c/tests/surface_prenex_families.c` preserves 13 families, 18 entries, five computations, 30 negatives including raw AST and exact specialization budget. Runtime passed. | All eleven controls killed, including the added specialization-entry budget assertion. |
| `prenex_groups.ml` | `c/tests/surface_prenex_groups.c` preserves 13 families, 23 ordered rows, seven computations, 39 refusals, AST boundary and grouped-budget comparison. Runtime passed. | All thirteen controls killed; collision controls remove the intended reservation at both relevant layers. |
| `prenex_exports.ml` | `c/tests/surface_prenex_exports.c` preserves 12 ordered rows, eight positives, 21 negatives, ten parser cases, three budget cases, chosen-name isolation and literal computations. Runtime passed. | All seven controls killed. |
| `prenex_dependency_exports.ml` | `c/tests/surface_prenex_dependency_exports.c` preserves seven positive chosen-name inventories, 23 exact rendered refusals, six parser refusals, two measured budget cases and original runtime fixture computations. Runtime passed. | All six controls killed. |
| `veil_templates.ml` | `surface_shape_metadata` ports all four original hidden SZk/SFhc/SMpc payload cases: exact level/name substitution and exact free-level refusal. Native execution passed. | Backend owns the three associated shape mutations and their kill evidence. |
| `template_composition.ml` | `c/tests/surface_template_composition.c` ports all original source/parser cases, nine executable raw cases, exact inventories/universes/computations, ambient rechecking, callback cutoff and category prefix through assoc. Compiler check passed. | Full runtime passed in 26.5 seconds. Negative signed arity is independently enforced by paired compiler fixtures; all seven controls killed. |
| `template_reuse.ml` | `c/tests/surface_template_reuse.c` preserves all six raw checks, exact identities/inventories, three normalized-level reuse variants, 17 exact source refusals, six parser cases, computations and category client. Full runtime passed in 280.6 seconds. | All five controls killed, including the count-reduction assertion-removal control. |
| `template_symbolic_reuse.ml` | `c/tests/surface_template_symbolic_reuse.c` preserves all five raw checks including certificate cancellation frontier and callback sentinel, full source/parser refusals, exact family/member inventories, computations and category client. Compiler check passed. | Full runtime passed in 491.5 seconds; all four controls killed. |

All 102 frontend mutation controls targeted the Bend sources (commit 8c90b64) and native protocol
assertions: dependent 10, equality operations 10, congruence 12, transport 10,
prenex 38 (7/11/13/7), composition 7, reuse 9 (5/4), dependency exports 6.
All anchors are mechanically unique. All 102 corrected controls were killed on
the pinned pre-indexed Bend 2.0.25 snapshot (before commit 8c90b64), including successful baseline and
restored checks. Final indexed Bend 2.0.27 mutation replay was pending at commit 8c90b64.
A failed baseline compiler attempt is recorded separately and earns no kill
credit. Unexecuted final controls remain explicit validation gaps, regardless
of aggregate pass counts.

`c/tests/prelude_runtime.c` and `c/tests/runtime_slice.c` separately preserve the runtime
adapter protocol, output ordering, partial progress, reachability traversal,
no-axiom check and slicing self-test. Their protocol, exact computations and
executable Wasm checks passed as recorded in `VALIDATION.md`.

Nine complete foundational protocols executed successfully using the fresh CPS and
lazy-lookup artifact. Exact stdout, elapsed times, artifact hash and source hashes
are in `build/bend2-foundation-protocols/report.json` (Bend run, commit 8c90b64; runtime capture
`.kanon-exec/run-uiLTiX`). These observations precede the later kernel conversion
correction and must be rerun against that final source state.

The Veil `vendor/veil/test/sl_surface.ml` suite is tracked separately: all 36 named
original tree-equality, roundtrip, checked-form and exact-message cases are frozen
in `dev/cases/sl-surface-cases.json` and ported in `c/tests/surface_sl.c`. All 36 executed
successfully. Its original source hash is recorded; corpus acceptance is not used
as a substitute for these assertions.

The expanded 19-protocol artifact and runtime checkpoint are in
`build/bend2-foundation-protocols/complete-report.json` (Bend run, commit 8c90b64). That artifact exposed
the original prenex 18-digit Nat overflow; `surface-prenex-big.js` subsequently
passed the exact original suite after the Bignum fix (`run-eSivXo`). The shared
helper's failure exit now preserves empty stderr. The signed-boundary compiler
pairs passed (`run-1P8G19`, `SURFACE-BOUNDARIES-OK assertions=2`). These receipts
are versioned evidence, not a claim that the final combined artifact is fresh.

The native immediate-Nat audit exposed a separate representation defect for
18-digit universe, width and leg literals. `c/tests/surface_numbers.c` now preserves
16 freshly recorded original verdicts and one huge-width parser roundtrip. Its
input/oracle hashes are in `dev/cases/surface-numeric-cases.json`. Surface numeric
payloads use Bignum; list positions remain Nat. All 20 final protocols passed
after the shared Big-width migration (`run-rTKqWA`,
`build/bend2-foundation-protocols/final-report.json`, Bend run, commit 8c90b64). Earlier protocol passes
remain pinned to their source versions. The 102-control mutation replay is
underway. Its first dependent run counted nine of ten because one equivalent
universe diagnostic had a different spelling; that expectation is corrected
and awaits a full rerun. No complete mutation-suite pass is claimed.

The corrected frontend mutation run passed all 102 controls across 12 suites,
including every baseline and restored baseline (`run-h1cfAm`). Exact results and
per-suite source hashes are in `build/bend2-frontend-mutations/corrected-report.json` (Bend run, commit 8c90b64).
These controls used the pinned pre-indexed Global implementation. Earlier 93/102
results and the original redundant-mutant counterfactual audit remain preserved.

Final Bend 2.0.27/indexed Global execution (commit 8c90b64) preserved all 21 protocol assertion
results, but template reuse and symbolic reuse exceed their original 120-second
timing tiers under shared load. These remain timing failures, independently of
functional coverage and the recorder's 900-second diagnostic completion cap.
See `build/bend2-foundation-protocols/final-2.0.27-timing.json` (Bend run, commit 8c90b64).
The subsequent native comparison passes reuse in 77.06 seconds but times out
symbolic reuse at 120.05 seconds. The latter remains an original-tier timing
failure, recorded separately from the successful JavaScript functional run.
The same native artifact subsequently completed symbolic reuse during a bounded
profile in 55.23 seconds with unchanged pins. The earlier timeout remains in
the gate report; timing conditions and this successful observation are separate.
