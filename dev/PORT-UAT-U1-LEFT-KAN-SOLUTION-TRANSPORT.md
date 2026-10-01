# U1: left Kan solution transport

`MechLeftKanLaws` constructs universal solution records across pointwise
equal inputs at six independent universe levels.

- `solutionCongr` takes a solution for `alpha` and
  `CoconeEq alpha beta`, then returns a solution for `beta` with the same
  mediator. Its factorization follows the original factorization by
  the supplied equality. Its uniqueness proof converts a candidate's
  factorization for `beta` back to one for `alpha`.
- `solutionUnitCongr` takes a solution with unit `eta` and
  `CoconeEq eta theta`, then returns a solution with unit `theta` and
  the same mediator. Postcomposition congruence supplies both directions
  needed to transfer factorization and uniqueness.

Both constructors accept arbitrary solution records. They introduce no
axiom and require no equality of whole transformation records.
The independent contracts at `(2, 0, 1, 3, 0, 2)` bind both endpoints and
the original solution separately, and pin the actual resulting solution type.

The dedicated native gate checks two constructors, two contracts, eight
computations and six refusals. Refusals cover missing cocone or unit equality,
solutions for the wrong target cocone, nominally different equality, and
cocone equality supplied where unit equality is required. Refusal oracles
pin the diagnostic prefix and complete MD5 digest; validation receipts also
record the diagnostic SHA-256 hashes.

Four certified runtime exports exercise the transported factorization and
uniqueness fields. The optional runtime mode is configured to compare each
export on the kernel, Node and Wasmtime at inputs 37 and 41, for 24
comparisons with independent arithmetic oracles.
The runtime fixture uses the distinct functions `n + 1` and `2 * n + 3`
to keep normalization within the existing emission allowance. The native
computation fixture retains its larger arithmetic cases.
Quantity-zero proof arguments are passed as checked functions, since the
kernel evaluator evaluates application arguments eagerly. Their proof bodies
remain in the dependency closure, and the computed values still project
the transported records.
The extraction guard requires each proof in the runtime dependency closure.
Four mutations replace individual certified exports with references, and
the guard must reject every substitution.

Runtime validation remains open. Three attempts, including compact arithmetic
and checked proof wrappers, reached the unchanged 480-second emission limit.
The complete host comparisons have not passed. This pending mode is opt-in;
the registered gates cover the passing native suite and extraction guard.

```sh
BEND=/path/to/bend-2.0.27 python3 -P dev/bend2-build.py --target tests --test-mode prelude-left-kan-solution-transport --backend native
python3 -I test/left_kan_solution_transport.py
python3 -I dev/left-kan-solution-transport-focus-test.py
python3 -I test/left_kan_laws_runtime.py --solution-transport
```

The runtime command requires the standard `prelude-runtime` test driver.
The full left Kan inventory expects 1,045 entries, nine families,
26 computations and 23 refusals. Existing suite and emission time allowances
are preserved. Stage C and U1 remain open on whole transformation equality
and source-type parity.
