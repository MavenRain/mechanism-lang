# M0 Stage C: polymorphic equality operations

Date: 2026-09-09.  Base: bd92df5, the committed transport and cast slice.
This increment extends the programmatic equality catalog with dependent
J, symmetry, transitivity and congruence, plus type-equality symmetry and
transitivity.  Stage C and PRELUDE-CHECKED remain open.

## Operations

For an instance `E` of MechEq at carrier Sort `u` and motive Sort `v`,
the new definitions have these contracts.  The sort notation below
describes the programmatic API; it is not new textual syntax.

```text
E_j : (0 A : Sort u) -> (0 x : A) ->
  (0 P : (0 y : A) -> (0 e : E A x y) -> Sort v) ->
  P x (E_refl A x) -> (0 y : A) -> (e : E A x y) -> P y e
E_symm : (0 A : Sort u) -> (0 x : A) -> (0 y : A) ->
  E A x y -> E A y x
E_trans : (0 A : Sort u) -> (0 x : A) -> (0 y : A) -> (0 z : A) ->
  E A x y -> E A y z -> E A x z
E_congr : (0 A : Sort u) -> (0 B : Sort u) -> (0 f : A -> B) ->
  (0 x : A) -> (0 y : A) -> E A x y -> E B (f x) (f y)
```

For a MechTypeEq instance `T` at Type level `u`, `T_symm A B e`
returns `T B A` from `e : T A B`, and `T_trans A B C e h` returns
`T A C` from `e : T A B` and `h : T B C`.  The type endpoints are
erased.  The resulting proofs can be consumed by the existing `T_cast`.
All previous member names, order and universe arities are retained;
new members follow the existing ones.

Each operation eliminates the existing singleton equality family.
J returns its supplied reflexivity case, symmetry returns reflexivity,
transitivity eliminates the second proof and returns the first, and
congruence returns reflexivity at the mapped left endpoint.  Carrier
and motive universes remain independent, including Prop.  Congruence
uses one shared carrier Sort level for its domain and codomain because
the template contains one equality family.  It does not provide Lean's
fully universe-independent congrArg signature.

The family and all members check symbolically from `Global.empty` and
check again on specialization.  This increment changes no kernel rule,
surface syntax, mapping verdict, trusted-line bound or vendor file.  The
existing equality soundness obligations, programmatic-only catalog plan
question and D-A-1 trusted-line ruling remain open.

## Validation

PRELUDE-EQUALITY-OPS specializes ten instances: seven MechEq combinations
and three MechTypeEq levels.  Generic source contracts exercise distinct
bound endpoints and the exact dependent J signature at every instance.
The source fixture supplies a motive indexed by both the right endpoint
and the proof, and indexed data witnesses for reflexivity computation.
Eleven independently specified normal forms are checked through the
kernel evaluator, including a proof-indexed data constructor.  The test
audits every installed entry and family, refusing axioms, primitives,
provisional families and builtins.

Eight misuse cases cover incorrect symmetry endpoints, nonmatching
transitivity endpoints, a wrong congruence function, an invalid J base,
a wrong J endpoint, the corresponding type-equality mistakes and mixing
instances.  Each case first checks its accepted counterpart in the same
environment.  Diagnostic oracles identify the operation and error prefix.
Counts in the gate's OK line are computed from the case lists.

Run `python3 -I dev/equality-ops-mutations.py NEW_WORK_DIRECTORY` to
reproduce ten isolated controls.  The script requires a clean build of
each mutant before its test failure counts, checks the expected
diagnostic, restores the original file and reruns the unmodified suite.
It records exact replacements, source hashes, mutant hashes and outputs.
The full battery also retains the existing transport and WASM tests.
See `dev/M0-BUILD-LOG.md` and `dev/MUTATION-LOG.md` for measured results.

The build copy is
`/Users/oobi/Documents/kanon-inference/mechanism-lang-equality-ops`.
Integration checks the canonical base and worktree before applying the
validated files to `/Users/oobi/Documents/mechanism-lang` and staging
all changes.  No commit is created.
