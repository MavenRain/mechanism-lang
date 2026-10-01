# Left Kan cocone congruence evidence

The source base is `fb0eb36e2b3cf4b6f1c30047fda82c3b6cba1ef3`.
This slice adds `postCoconeCongr`, a mixed-universe contract, four certified
runtime exports and five refusal fixtures. `checks.json` records the completed
native build and runtime checks, artifact fingerprints, source hashes and
earlier attempts. The raw stdout and stderr files preserve their results.

The direct native kernel check passed with 1,002 entries, nine families,
20 computations and 18 refusals. Its measured wall time was 1,867.876
seconds, exceeding the manifest's unchanged 1,800-second allowance.
This records the direct driver's result separately from its timing limit.
The focused runtime marker covers the four new exports
at two payloads on the kernel, Node and Wasmtime, for 24 comparisons.
The default suite includes 20 exports but reached its unchanged
480-second emission limit. The preceding slice records the same timeout
in `../port-uat-u1-left-kan-cocone-laws/runtime.stdout`.

Set `BEND` to the project's Bend 2.0.27 compiler and reproduce with:

```sh
python3 -P dev/bend2-build.py --target tests --test-mode prelude-left-kan-laws --backend native
python3 -P dev/bend2-build.py --target tests --test-mode prelude-runtime --backend native
_bend2/test/prelude_left_kan_laws.exe "$PWD"
python3 -I test/left_kan_laws_runtime.py --cocone-congruence
```

The native runtime artifact was reused only after its binary fingerprint
and all 118 source fingerprints matched. The build tool then confirmed
that artifact was current. Both JavaScript and native emission of the
default suite reached the existing 480-second limit. The focused validation
uses the native frontend for source checking and emission, then executes
the emitted Wasm on Node and Wasmtime.

`dev/left-kan-cocone-congruence-focus.py` extracts the new exports and
their lexical dependencies, preserving definition bodies and group headers.
The generated source and extraction hashes are retained here. This focused
mode omits the abstract mixed-universe pin, which the full kernel gate checks.
