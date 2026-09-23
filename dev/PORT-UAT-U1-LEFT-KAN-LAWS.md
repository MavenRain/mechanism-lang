# U1: pointwise left Kan mediator laws

Date: 2026-09-22. Base: 186efb9 (dependency export clauses).
Extended 2026-09-23 from 6a275ba with cocone postcomposition and its mediator law.

## Contract

`prelude/cat/left-kan-laws.mech` adds `MechLeftKanLaws`, parameterized by
the six independent object and hom universe levels of the source, middle
and target categories. It imports `MechSharedLeftKan` as `Base`, retaining
its three nominal equality families.

- `CoconeEq` compares cocone components at every source object.
- `descId` compares the mediator of a solution from `eta` to itself with
  the identity transformation at every middle object. Its proof uses
  the target category's right identity law, mediator uniqueness and symmetry.
- `descCongr` compares the mediators of two solution records for
  pointwise-equal cocones with the same unit. It composes the first
  factorization proof with the supplied cocone equality, then applies
  the second solution's uniqueness proof.
- `postCocone` composes `alpha : F => G K` with the precomposition of
  `tau : G => I` by `K`. At a source object `x`, its component applies
  `alpha x` first, then `tau (K x)`.
- `descPostcomp` compares `s.desc y` followed by `tau y` with `t.desc y`,
  where `s` solves `alpha` and `t` solves `postCocone alpha tau` against
  the same unit. Associativity rewrites the candidate factorization,
  congruence applies `s`'s factorization, and `t`'s uniqueness proves the law.

The laws accept solution records directly, including values returned by
`lanSolve`. They do not require the records to be definitionally equal.
The existing `desc_unique` theorem already compares arbitrary mediators
with the same factorization.

Each mediator law concludes a Prop in the shared target hom equality family.
These are component equalities, with no claim of equality between whole
transformation records. Stage C and U1 remain open on that distinction
and source-type parity. The kernel, vendor pins and axiom policy are unchanged.

## Validation

`PRELUDE-LEFT-KAN-LAWS` checks symbolic elaboration, parse/print stability,
closed specializations at `(0,1,0,0,1,0)` and `(2,0,1,3,0,2)`, checked
family reuse, unchanged builtins, absence of axioms, ten computations and
ten type-mismatch refusals. The refusals cover unequal cocones, wrong
solution records, a missing cocone-equality argument, a wrong identity
conclusion, a distinct nominal equality family and a wrong congruence conclusion.
The four postcomposition refusals reject the wrong original or postcomposed
solution, a reversed conclusion and an equality from a separate nominal family.
Each `test/neg/left-kan-laws/<name>.err` pins the full refusal text. The suite
compares by prefix: the refusal text must start with the pinned text. The suite also
requires the ten pins to be pairwise distinct and requires that no pin is a prefix
of another pin, so a refusal for an unrelated reason fails the suite.
Parse errors, unbound names and exhausted budgets cannot satisfy a refusal.
The fixture `test/fixtures/prelude/left-kan-laws.mech` pins all three law
statements at `(2,0,1,3,0,2)` with bound solution records
(`WideIdContract := Wide_descId`, `WideCongrContract := Wide_descCongr`,
`WidePostcompContract := Wide_descPostcomp`). The postcomposition contract
binds three arbitrary middle-to-target functors and both solution records.
A template whose conclusion is trivial fails to convert against the pins.

The runtime fixture reuses the existing indexed left Kan example. It
constructs independent solution records using external transformations,
then passes each law to an erased proof argument while applying both
fields of its mediator. A weighted combination distinguishes the two
fields. Ten exports, including independent references, run at payloads
37 and 41 on the kernel, Node and Wasmtime, for 60 comparisons. Arithmetic
oracles distinguish the identity, two nonconstant cocones and both orders
of their postcomposition. The orders have different results, and each
reference uses a newly solved cocone. Both mediator fields contribute.

The mutation runner checks four invalid proofs, a swapped runtime field
calculation, reversed runtime composition and three trivialized law statements
in a temporary source copy. It retains full attempt logs and
source hashes. Each killed attempt must print its expected diagnostic head
exactly once; the control and restored attempts must print nothing. The
output directory must be new and outside the repository; its products are
copied into the validation record afterwards. The two gate predicates require
the expanded counts. Their CATEGORY watchdog and runtime budgets are unchanged.

```sh
zsh dev/dunecho.sh build
_build/default/test/prelude_left_kan_laws.exe .
python3 -I test/left_kan_laws_runtime.py
python3 -I dev/left-kan-laws-mutations.py NEW_OUTPUT_DIRECTORY
```

The postcomposition validation record is under
`dev/validation/port-uat-u1-left-kan-postcomposition/`. The earlier identity
and congruence record remains in `dev/validation/port-uat-u1-left-kan-laws/`.
The full battery is not rerun
for this additive prelude slice. The inherited TRUSTED-LINES failure
remains `kernel=5475/3000 encoder=246/900`.
