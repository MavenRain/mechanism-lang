# Left Kan cocone equality validation

Base: `543ff831da6c8bfa14243838c188db9ab13c08a2`.

Both focused native drivers built with the pinned Bend 2.0.27 toolchain.
The three contracts passed the native kernel check. The integrated gate
passed three laws, three contracts, six computations and five refusals:

```
python3 -I test/left_kan_cocone_equality.py
PRELUDE-LEFT-KAN-COCONE-EQUALITY-OK laws=3 contracts=3 computations=6 negatives=5
```

The new runtime mode passed six exports on the kernel, Node and Wasmtime
at inputs 37 and 41, for 36 comparisons. The existing cocone-congruence
mode passed four exports at those inputs, for 24 comparisons:

```
python3 -I test/left_kan_laws_runtime.py --cocone-equality
python3 -I test/left_kan_laws_runtime.py --cocone-congruence
```

`checks.json` records the commands, capture hashes and source hashes.
The stdout and stderr files preserve each completed check. The focused
source and extraction manifest preserve the retained definitions and
their provenance. The five checked-in refusal oracles are distinct across
all 23 left Kan refusal cases.

The full battery, full left Kan suite and default runtime mode were not
rerun. The preceding slice reported an overrun of the unchanged 1,800-second
native suite allowance and the unchanged 480-second default emission limit.
