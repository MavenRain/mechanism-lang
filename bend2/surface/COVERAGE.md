# Historical frontend assertion coverage

This table distinguishes executed Bend assertions, frozen source observations,
and unfinished original-suite parity. A source accepted or rejected by the
semantic corpus does not replace a raw API assertion, a trusted-global audit,
a computed-value assertion, or a mutation negative control. Existing unit
execution evidence is listed in [VALIDATION.md](VALIDATION.md).

The historical inline corpus has 216 exact observed matches. Its original
source and evaluated-prefix hashes are in
`dev/bend2/surface-extra-inputs.json`. The broader frozen replay has 926 unique canonicalized inputs. On the pinned
pre-final driver, 925 matched exactly after serial 900-second retries; the
remaining NatMul case timed out at 900 seconds. The later corrected native replay
matched 924 cases and timed out on both arithmetic agreement sources. The isolated
indexed Global prototype matches all 926 frozen cases exactly; its source review
and guarded landing are complete. The final indexed Bend 2.0.27 JavaScript replay
also matches all 926 cases exactly, with zero failures. Generated `group:*` inputs concatenate quoted paths;
they do not reproduce the corresponding original suite setup.

| Original test | Assertions already represented by Bend units | Remaining original-suite or mutation-driver work |
| --- | --- | --- |
| `equality.ml` | `surface_equality.bend` preserves all 15 original named assertions, including Prop index universes, singleton elimination, field quantities, scope and exact callback poll boundary. All 15 executed successfully. | No original assertion gap; rerun in the final aggregate after later kernel changes. |
| `family_poly.ml` | `surface_family_poly.bend` preserves 33 executable original named cases in order, exact quantities and scopes, recursive fields/motive renaming, universality, all original collision placements, ambient rechecking, rollback and shared substitution-budget measurements. Runtime passed. | The original negative signed arity is enforced by a positive/negative compiler pair in `surface_protocol_boundaries.py`; this is a separate assertion, not a fabricated runtime case. |
| `prelude_poly.ml` | `surface_prelude_poly.bend` preserves the two catalog arities, five concrete instances, audits and two nominal/indexed refusals. Runtime passed. | No original assertion gap. |
| `prelude_transport.ml` | `surface_prelude_transport.bend` preserves exact catalog/member inventory, seven instances, chosen exports over a reused family, computation witnesses, audits and five scope-specific refusals. Runtime passed. | All ten transport/member mutation controls killed, with baseline and restored baseline passing. |
| `family_members.ml` | `surface_family_members.bend` ports all 35 original named cases in exact order, including full export collision/name planning, hidden metadata, rollback and original extra-poll equalities 16 and 13. Runtime passed. | All ten associated transport mutation controls killed. |
| `prelude_equality_ops.ml` | `surface_prelude_equality_ops.bend` preserves all ten instance contracts, eleven exact normalizations, audit and eight good/bad controls. Runtime passed. | All ten mutation controls killed, with baseline and restored baseline passing. |
| `prelude_dependent.ml` | `surface_prelude_dependent.bend` preserves six arities, four instance groups plus five additional Pi instances, 29-entry count, contracts, twelve exact normalizations, audit, six controls and callback cutoff. Runtime passed. | All ten mutation controls killed, with baseline and restored baseline passing. |
| `prelude_congruence.ml` | `surface_prelude_congruence.bend` preserves catalog inventory, callback cutoff, eight contracts, symbolic-name isolation, four exact normalizations, audit and five good/bad controls. Runtime passed. | All twelve congruence/group mutation controls killed, with baseline and restored baseline passing. |
| `family_groups.ml` | `surface_family_groups.bend` preserves all 31 original names/order, callback visibility and parity, exact scope/body/first-error refusals, companion specialization, collisions/rollback and entry cutoffs. Runtime passed. | Associated congruence and prenex-group mutation replays passed. Callback cancellation uses the equivalent measured callback allowance and a failing sentinel for forbidden callbacks. |
| `prenex.ml` | `surface_prenex.bend` preserves all original row order, isolation/audit, four exact computations, 25 refusals and the 18-digit universe roundtrip/check. Runtime passed after moving universe and Type literals to Bignum. | All seven controls killed; final aggregate regeneration remains separate. |
| `prenex_families.ml` | `surface_prenex_families.bend` preserves 13 families, 18 entries, five computations, 30 negatives including raw AST and exact specialization budget. Runtime passed. | All eleven controls killed, including the added specialization-entry budget assertion. |
| `prenex_groups.ml` | `surface_prenex_groups.bend` preserves 13 families, 23 ordered rows, seven computations, 39 refusals, AST boundary and grouped-budget comparison. Runtime passed. | All thirteen controls killed; collision controls remove the intended reservation at both relevant layers. |
| `prenex_exports.ml` | `surface_prenex_exports.bend` preserves 12 ordered rows, eight positives, 21 negatives, ten parser cases, three budget cases, chosen-name isolation and literal computations. Runtime passed. | All seven controls killed. |
| `prenex_dependency_exports.ml` | `surface_prenex_dependency_exports.bend` preserves seven positive chosen-name inventories, 23 exact rendered refusals, six parser refusals, two measured budget cases and original runtime fixture computations. Runtime passed. | All six controls killed. |
| `veil_templates.ml` | `surface_shape_metadata` ports all four original hidden SZk/SFhc/SMpc payload cases: exact level/name substitution and exact free-level refusal. Native execution passed. | Backend owns the three associated shape mutations and their kill evidence. |
| `template_composition.ml` | `surface_template_composition.bend` ports all original source/parser cases, nine executable raw cases, exact inventories/universes/computations, ambient rechecking, callback cutoff and category prefix through assoc. Compiler check passed. | Full runtime passed in 26.5 seconds. Negative signed arity is independently enforced by paired compiler fixtures; all seven controls killed. |
| `template_reuse.ml` | `surface_template_reuse.bend` preserves all six raw checks, exact identities/inventories, three normalized-level reuse variants, 17 exact source refusals, six parser cases, computations and category client. Full runtime passed in 280.6 seconds. | All five controls killed, including the count-reduction assertion-removal control. |
| `template_symbolic_reuse.ml` | `surface_template_symbolic_reuse.bend` preserves all five raw checks including certificate cancellation frontier and callback sentinel, full source/parser refusals, exact family/member inventories, computations and category client. Compiler check passed. | Full runtime passed in 491.5 seconds; all four controls killed. |

All 102 frontend mutation controls now target Bend sources and native protocol
assertions: dependent 10, equality operations 10, congruence 12, transport 10,
prenex 38 (7/11/13/7), composition 7, reuse 9 (5/4), dependency exports 6.
All anchors are mechanically unique. All 102 corrected controls were killed on
the pinned pre-indexed Bend 2.0.25 snapshot, including successful baseline and
restored checks. Final indexed Bend 2.0.27 mutation replay remains pending.
A failed baseline compiler attempt is recorded separately and earns no kill
credit. Unexecuted final controls remain explicit validation gaps, regardless
of aggregate pass counts.

`prelude_runtime.bend` and `runtime_slice.bend` separately preserve the runtime
adapter protocol, output ordering, partial progress, reachability traversal,
no-axiom check and slicing self-test. Their protocol, exact computations and
executable Wasm checks passed as recorded in `VALIDATION.md`.

Nine complete foundational protocols executed successfully using the fresh CPS and
lazy-lookup artifact. Exact stdout, elapsed times, artifact hash and source hashes
are in `build/bend2-foundation-protocols/report.json` (runtime capture
`.kanon-exec/run-uiLTiX`). These observations precede the later kernel conversion
correction and must be rerun against that final source state.

The Veil `vendor/veil/test/sl_surface.ml` suite is tracked separately: all 36 named
original tree-equality, roundtrip, checked-form and exact-message cases are frozen
in `dev/bend2/sl-surface-cases.json` and ported in `surface_sl.bend`. All 36 executed
successfully. Its original source hash is recorded; corpus acceptance is not used
as a substitute for these assertions.

The expanded 19-protocol artifact and runtime checkpoint are in
`build/bend2-foundation-protocols/complete-report.json`. That artifact exposed
the original prenex 18-digit Nat overflow; `surface-prenex-big.js` subsequently
passed the exact original suite after the Bignum fix (`run-eSivXo`). The shared
helper's failure exit now preserves empty stderr. The signed-boundary compiler
pairs passed (`run-1P8G19`, `SURFACE-BOUNDARIES-OK assertions=2`). These receipts
are versioned evidence, not a claim that the final combined artifact is fresh.

The native immediate-Nat audit exposed a separate representation defect for
18-digit universe, width and leg literals. `surface_numbers.bend` now preserves
16 freshly recorded original verdicts and one huge-width parser roundtrip. Its
input/oracle hashes are in `dev/bend2/surface-numeric-cases.json`. Surface numeric
payloads use Bignum; list positions remain Nat. All 20 final protocols passed
after the shared Big-width migration (`run-rTKqWA`,
`build/bend2-foundation-protocols/final-report.json`). Earlier protocol passes
remain pinned to their source versions. The 102-control mutation replay is
underway. Its first dependent run counted nine of ten because one equivalent
universe diagnostic had a different spelling; that expectation is corrected
and awaits a full rerun. No complete mutation-suite pass is claimed.

The corrected frontend mutation run passed all 102 controls across 12 suites,
including every baseline and restored baseline (`run-h1cfAm`). Exact results and
per-suite source hashes are in `build/bend2-frontend-mutations/corrected-report.json`.
These controls used the pinned pre-indexed Global implementation. Earlier 93/102
results and the original redundant-mutant counterfactual audit remain preserved.

Final Bend 2.0.27/indexed Global execution preserves all 21 protocol assertion
results, but template reuse and symbolic reuse exceed their original 120-second
timing tiers under shared load. These remain timing failures, independently of
functional coverage and the recorder's 900-second diagnostic completion cap.
See `build/bend2-foundation-protocols/final-2.0.27-timing.json`.
The subsequent native comparison passes reuse in 77.06 seconds but times out
symbolic reuse at 120.05 seconds. The latter remains an original-tier timing
failure, recorded separately from the successful JavaScript functional run.
The same native artifact subsequently completed symbolic reuse during a bounded
profile in 55.23 seconds with unchanged pins. The earlier timeout remains in
the gate report; timing conditions and this successful observation are separate.
