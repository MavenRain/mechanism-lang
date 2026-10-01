# U1: left Kan mediator equality

`MechLeftKanLaws` adds two laws at six independent universe levels.

- `descReflectsEq` takes solutions `s` and `t` for independent cocones
  `alpha` and `beta`, with the same unit `eta`. Pointwise equality of
  their mediators implies `CoconeEq alpha beta`. The proof combines the
  two factorization fields with congruence, symmetry and transitivity
  of target hom equality.
- `descChoiceEq` proves pointwise equality of the mediators of any two
  universal solutions for the same cocone. It applies `descCongr` to
  reflexive cocone equality.

Together, `descCongr` and `descReflectsEq` characterize cocone equality
by mediator equality. `descChoiceEq` makes independence of the chosen
solution explicit. These laws introduce no axiom and do not assert
equality of whole transformation records.

The wide contracts bind categories, functors, the unit, cocones and
solutions independently at levels `(2, 0, 1, 3, 0, 2)`. The dedicated
native gate checks two laws, two contracts, eight computations and five
refusals. The refusals cover missing mediator equality, a solution for
the wrong cocone, nominally different equality, a missing solution and
choice independence applied to different cocones.
The native gate checks a 65-character prefix and the complete diagnostic
with the existing MD5 oracle policy. Full diagnostic SHA-256 hashes are
recorded separately in the validation receipts. Checksums are distinct
across all five refusals.

The runtime gate compares four certified exports on the kernel, Node
and Wasmtime at inputs 37 and 41, for 24 comparisons. Two cocones have
distinct arithmetic oracles. Each runtime export retains its proof in
the focused dependency closure. A mutation gate substitutes a reference
for a certified export and requires the extraction guard to refuse it.

```sh
python3 -P dev/bend2-build.py --target tests --test-mode prelude-left-kan-mediator-equality --backend native
python3 -I test/left_kan_mediator_equality.py
python3 -I dev/left-kan-mediator-equality-focus-test.py
python3 -I test/left_kan_laws_runtime.py --mediator-equality
```

The full left Kan inventory expects 1,041 entries, nine families,
26 computations and 23 refusals. The dedicated modes preserve the
existing suite time allowances. Stage C and U1 remain open on whole
transformation equality and source-type parity.
