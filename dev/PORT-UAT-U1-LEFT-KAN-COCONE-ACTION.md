# U1: left Kan cocone postcomposition action

`MechLeftKanLaws` exposes the postcomposition laws directly as `CoconeEq`
proofs. Clients can apply reflexivity, symmetry, transitivity or mediator
congruence to these proofs without restating the component equation.

- `postCoconeEqId` relates postcomposition by the identity transformation
  to the original cocone.
- `postCoconeEqVcomp` relates successive postcompositions to
  postcomposition by vertical composition.
- `postCoconeEqCongr` compares the two actual postcomposed cocones,
  given cocone equality and pointwise transformation equality.

The proofs use the target category's identity and associativity laws, with
checked equality congruence and transitivity. The group retains six independent
universe levels and introduces no axiom. These conclusions use the cocone
relation, whose definition compares components. Equality of whole
transformation records and source-type parity remain open in Stage C and U1.

Three contracts at `(2, 0, 1, 3, 0, 2)` bind their categories, functors,
cocones and transformations independently. Their statements use the
actual `postCocone`, identity and vertical-composition operations.
The runtime fixture specializes the API at `(0, 1, 0, 0, 1, 0)`.
It consumes each proof through `lanCertified`, covers both orders of
noncommuting transformations, and changes the cocone operand in congruence.

`PRELUDE-LEFT-KAN-COCONE-ACTION` checks the three contracts, eight
computations and four refusals: an incorrect identity endpoint, reversed
composition, missing equality premises and an equality for another cocone.
`PRELUDE-LEFT-KAN-COCONE-ACTION-RUNTIME` compares eight exports on the
kernel, Node and Wasmtime at inputs 37 and 41, for 48 comparisons.
The independent arithmetic oracles distinguish the composition orders.

The dedicated native driver is selected with:

```sh
python3 -P dev/bend2-build.py --target tests --test-mode prelude-left-kan-cocone-action --backend native
python3 -I test/left_kan_cocone_action.py
python3 -I test/left_kan_laws_runtime.py --cocone-action
```

The runtime command also requires the native `prelude-runtime` driver.
The standard Bend test runner reads the driver's `prepare` entry and generates
the focused contracts before invoking its check, including with `--no-build`.
The extractor keeps the declared template inventory, includes the action
fixture only for this selection, and retains definition bodies verbatim.
Kernel and runtime timeout limits remain unchanged.

The full left Kan laws suite retains its existing 26 computations and
23 refusals. Its inventory gains six specialized law declarations,
for 1,033 entries and nine families. The dedicated action contracts and
refusals run in their separate gate.
