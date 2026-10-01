# U1: left Kan mediator round trips

`MechLeftKanLaws` exposes both directions between cocones and their
universal mediators at six independent universe levels.

- `postCoconeDesc` reconstructs a cocone by postcomposing the unit with
  the chosen mediator. Its conclusion is `CoconeEq` with the original
  cocone, using the solution's factorization field.
- `descPostCocone` takes a transformation from the extension functor, a
  solution for an independently bound cocone, and `CoconeEq` relating
  the postcomposed unit to that cocone. Uniqueness proves that the chosen
  mediator equals the transformation at every middle-category object.

Both laws accept an independently bound solution record. They introduce
no axiom. Cocone equality and mediator equality compare components;
equality of whole transformation records and source-type parity remain
open in Stage C and U1.

Two contracts at `(2, 0, 1, 3, 0, 2)` pin the actual postcomposition
operation, solution projection and transformation components. The runtime
fixture specializes the API at `(0, 0, 0, 0, 0, 0)` and consumes each proof
through `lanCertified`. Both directions run on two distinct cocones and
their independently chosen mediators. Function-valued morphisms produce
the independent arithmetic oracles `103 * input + 11` and
`103 * input + 1126`.
The example solutions are constructed directly from checked identity and
equality laws. The mixed-universe contracts accept arbitrary solution
records; the shared left Kan gates retain generic solver coverage.

`PRELUDE-LEFT-KAN-ROUNDTRIP` checks the two laws, two contracts,
eight computations and four refusals: a missing solution, a solution for
another cocone, a factorization for another transformation and an unrelated
equality family. `PRELUDE-LEFT-KAN-ROUNDTRIP-RUNTIME` compares four
certified exports on the kernel, Node and Wasmtime at inputs 37 and 41, for 24
comparisons. The arithmetic oracles distinguish the two cocones.
`PRELUDE-LEFT-KAN-ROUNDTRIP-FOCUS` replaces one certified export with its
reference function and checks that the extraction guard refuses it.

Run the dedicated native driver and runtime check with:

```sh
python3 -P dev/bend2-build.py --target tests --test-mode prelude-left-kan-roundtrip --backend native
python3 -I test/left_kan_roundtrip.py
python3 -I dev/left-kan-roundtrip-focus-test.py
python3 -I test/left_kan_laws_runtime.py --roundtrip
```

The full left Kan inventory now expects 1,037 distinct entries and nine
families. Its existing computation and refusal inventories are unchanged;
the new contracts and runtime examples belong to the dedicated gate.
