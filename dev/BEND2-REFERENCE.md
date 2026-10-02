# Bend 2 live reference checks

The live reference checks compare the frozen cases with an external OCaml
checkout. The sixth port slice adds concrete and symbolic family reuse across
the kernel, multi-file builds, Wasm emission and runtime hosts. Run the full
comparison with:

```sh
export REFERENCE=/path/to/ocaml-checkout
make bend2-diff REFERENCE="$REFERENCE"
make bend2-reference-test REFERENCE="$REFERENCE"
```

`bend2-diff` builds fresh OCaml libraries and seven observation adapters,
compares their results with the checked-in expectations, verifies the Bend 2
build, and replays the same twelve case sets through Bend 2. The Bend builder
checks source, compiler, and artifact fingerprints before reusing binaries.
Select the intended OCaml switch in `PATH`. The reference build needs `git`,
`dune`, `ocamlfind`, `ocamlc`, `ocamlopt`, `ocamlrun`, and the `zarith` and
`unix` packages. The reference directory must be the root of a git work tree,
because the report records its `HEAD` commit.
The Bend toolchain is configured as for `make bend2-build`.
The core CLI, equality, composition and reuse runtime cases also require
`node`, `wasmtime`, `zsh`, and `rg` in `PATH`.
Their executable hashes and both runtime scripts are recorded in the report.

The seven observation adapters are retrieved byte for byte from Git commit
`ed923e2130b8501ccbe500b538cfa3d39aa2bcda` and written into the reference
check's evidence directory. This keeps the Bend source tree free of OCaml
sources while preserving the live comparison checks. The checkout running
these checks must be a Git repository whose history contains that commit;
shallow clones must fetch the required history first. The runner retrieves
the adapters before the OCaml build and stops if that history is missing.
It ignores Git replacement objects and checks each adapter against its
recorded SHA-256 hash. The report records the adapter revision and, under
`inputs`, the hash of each retrieved adapter in the evidence directory.
The adapter retrieval receipt is in
`dev/validation/bend2-reference-adapters/`.

| Case set | Cases |
| --- | ---: |
| JSON | 166 |
| Export | 470 |
| Translation | 613 |
| Import pipeline | 1,118 |
| Parser | 261 |
| Surface checker | 926 |
| Erasure | 80 |
| CLI and publication | 37 |
| Core CLI, Wasm bytes and runtime hosts | 28 |
| Equality transport, Wasm bytes and runtime hosts | 34 |
| Template composition, multi-file builds and runtime hosts | 40 |
| Concrete and symbolic family reuse and runtime hosts | 104 |
| Total | 3,877 |

The core CLI cases compare exact Wasm bytes for emission and multi-file builds,
plus results from the kernel, Node, Wasmtime and combined hosts. They also cover
traps, missing exports, invalid output paths, and filenames containing shell
characters. Each case has a fresh temporary directory and a 30-second limit;
timeouts remain failures, with captured output retained and child processes
cleaned up by the shared process helper. The OCaml build receives copies of its
runtime scripts at the location resolved relative to its executable. Both the
original scripts and the copies are pinned by hash.

Run this case set alone with:

```sh
python3 -P dev/bend2-reference-check.py --reference "$REFERENCE" --suite core-cli
python3 -P bend2/tests/cli_core_check.py
```

The frozen core CLI expectations remain in
`bend2/tests/cli_core_expected.json`. The reference runner and Bend replay use
the same case definitions and observation helper.

The equality runtime cases reuse `prelude/init.mech` and
`test/fixtures/prelude/equality-runtime.mech`. They transport a literal, a data
value and a closure, with fixed answers of 37, 2 and 42. Changing the payload
to 41 must change those answers to 41, 2 and 46. Both variants check without
axioms and compare exact emitted Wasm bytes and answers from the kernel, Node,
Wasmtime and combined hosts. Each observation has the existing equality gate's
20-second limit, a fresh temporary directory, and the shared process cleanup.
The fixture and prelude are pinned before the live reference build and checked
again after replay. The Bend replay also checks the hashes of both files again
after the last case.

Run this family alone with:

```sh
python3 -P dev/bend2-reference-check.py --reference "$REFERENCE" --suite equality-runtime
python3 -P dev/bend2-equality-runtime-check.py
python3 -P dev/bend2-equality-runtime-test.py --reference "$REFERENCE"
```

The frozen equality observations are in `dev/bend2/equality-runtime-cases.json`.
The loader checks source hashes, the complete case inventory, observation types,
Wasm headers and fixed answers before replay. Fourteen refusal controls cover missing and extra
cases, malformed observations, missing or unexpected Wasm, a changed answer,
a stale payload mutation, wrong runtime answers, changed Wasm bytes, a timeout
with captured output, missing or changed source hashes, and fixture drift.
`bend2-reference-test` runs these controls
alongside the existing controls.

The composition runtime cases reuse
`test/fixtures/prelude/template-composition.mech` and
`test/fixtures/prelude/composition-runtime.mech`. Forward composition adds two
before doubling, reverse composition doubles before adding two, and nested
composition doubles through dependent template instances. For input 37 their
answers are 78, 76 and 74; input 41 must produce 86, 84 and 82. Each variant
checks without axioms, compares exact Wasm bytes from both single-file emission
and a build with separate template and runtime files, and runs on the kernel,
Node, Wasmtime and combined hosts. Each observation has a fresh temporary
directory, a 20-second limit, and the shared process cleanup. Both source files
are pinned before replay and checked for drift afterward.

Run this family alone with:

```sh
python3 -P dev/bend2-reference-check.py --reference "$REFERENCE" --suite composition-runtime
python3 -P dev/bend2-composition-runtime-check.py
python3 -P dev/bend2-composition-runtime-test.py --reference "$REFERENCE"
```

The frozen composition observations are in
`dev/bend2/composition-runtime-cases.json`. Eighteen refusal controls cover
incomplete case inventories, malformed observations, missing or unexpected
Wasm, changed inputs and composition order, stale mutation anchors, unknown
variants, missing or changed source hashes, runtime and build mismatches,
timeouts with captured output, and drift in either source file.
`bend2-reference-test` runs these controls with the existing reference and
equality controls.

The reuse runtime cases use `reuse-runtime-concrete.mech` and
`reuse-runtime-symbolic.mech`, with the existing `Box` and `Pair` templates
from `template-composition.mech`. `reuse-runtime-shared.mech` checks reuse
inside a group with three independent universe variables. Both modes share
source, intermediate and target box families across repeated transfers,
including two dependencies bound to one family. Input 37 produces 83, 80,
37 and 44 for `repeated`, `offset`, `identity` and `identityOffset`;
input 41 produces 91,
88, 41 and 48. Each mode and input checks without axioms, compares exact Wasm
bytes from emission and builds with separate prelude and fixture files, and
runs on the kernel, Node, Wasmtime and combined hosts. Observations have fresh
temporary directories, a 20-second limit and shared process cleanup. All four
source files are pinned before replay and checked for drift afterward.

Run this family alone with:

```sh
python3 -P dev/bend2-reference-check.py --reference "$REFERENCE" --suite reuse-runtime
python3 -P dev/bend2-reuse-runtime-check.py
python3 -P dev/bend2-reuse-runtime-test.py --reference "$REFERENCE"
```

The frozen observations are in `dev/bend2/reuse-runtime-cases.json`.
Twenty-six refusal controls exercise missing concrete or symbolic cases,
malformed observations, absent or extra Wasm, incorrect exported
answers, missing or changed source pins, missing or duplicated payload anchors,
unknown modes and variants, altered live runtime answers and Wasm bytes,
timeouts with captured output, and source drift. `bend2-reference-test` runs
these controls along with the previous reference and runtime controls.
The controls validate all 104 cases before selecting six live observations
to damage. For machines with spare CPU capacity, both the reference runner
and reuse replay accept `--jobs N` with one through eight workers (default
one). The reference runner parallelizes runtime suites only. Each observation
keeps its own temporary directory and timeout; report order remains the case
inventory order, and source drift checks still run after all observations.
With more than one worker, an interrupt or a termination signal stops new
observations, and the runner waits for the running observations to end. Each
running observation can use its full timeout.

The surface corpus also references 18 unresolved historical attempts in
`dev/bend2/surface-baseline-unresolved.json`. Those attempts retain their
unresolved status and are outside the passing case inventory.

Run only the OCaml comparison with
`make bend2-reference-check REFERENCE="$REFERENCE"`. Each run
creates a fresh directory under `_bend2/reference/recording-*` containing the
isolated build, adapters, per-case observations, and `report.json`. The report
pins the reference revision, source files, dune build files, case files, helper
scripts, tools, and compiled artifacts by SHA-256. A case mismatch, a case
timeout, or input drift found at the end of the run gives exit status 1. A
failed build, a missing tool, a malformed case file, or an input change found
during the run gives exit status 2 and writes `error.json`. The expected case
files are never rewritten by this command.

The comparison preserves exit status, stdout, and stderr, plus the CLI's
published file tree. The only normalizations are the temporary CLI root and
the process-specific suffix in the missing-parent mkdir diagnostic.
`bend2-reference-test` checks changed expectations, timeouts, and input drift
using a real OCaml build and temporary case files. Core CLI controls additionally
reject missing or extra cases, malformed byte observations, invalid exit statuses,
changed Wasm bytes and changed runtime answers. The timeout control also checks
that captured output survives cancellation.

For a scoped comparison or another live OCaml checkout:

```sh
python3 -P dev/bend2-reference-check.py --suite json \
  --reference /path/to/mechanism-lang --output /tmp/mechanism-json-reference
```

An explicit output directory must be new. Inside the reference checkout it
must be under this runner's `_bend2/reference` directory. Completed reports
and observation files remain available for inspection. The second-slice
validation receipt is in `dev/validation/bend2-reference-live/`. `report.json`
gives the result for each case set and the hashes of the slice files.
`sources.sha256` pins the source files and case files that the run read.
`tools.json` pins the OCaml tools. The `*.stdout` and `*.stderr` files contain
the command output. To check the source pins, run
`shasum -a 256 -c dev/validation/bend2-reference-live/sources.sha256` from the
repository root. `report.json` also pins the Bend 2 drivers, their build
fingerprints, the `bend2/` tree, and the Bend 2 check scripts that `bend2-diff`
used.

The third-slice receipt is in `dev/validation/bend2-core-cli-live/`. It records
the expanded live OCaml replay, the 28-case native Bend core CLI replay, and
the refusal controls. It includes the unchanged frozen core CLI corpus hash,
the source hashes, runtime tools and scripts, and the native Bend executable
and launcher hashes used for these checks. `sources.sha256` pins the source
files and case files that the live run read, and the slice files. To check
these pins, run
`shasum -a 256 -c dev/validation/bend2-core-cli-live/sources.sha256` from the
repository root. The second-slice pins record the second-slice run. Files
that a later slice changed do not agree with these pins.

The seven `dev/bend2/reference/*.ml` rows in the second- and third-slice
`sources.sha256` files name adapters that are no longer in the tree. The two
`shasum` commands above report them as missing. Check each of them with
`git show ed923e2130b8501ccbe500b538cfa3d39aa2bcda:dev/bend2/reference/<name>.ml | shasum -a 256`.

The fourth-slice receipt is in `dev/validation/bend2-equality-runtime-live/`.
It records 99 live OCaml and 99 native Bend observations across the CLI,
core CLI and equality runtime suites, plus ten existing and fourteen new
refusal controls. The fresh OCaml report pins both equality source files,
the runtime scripts, tools and compiled artifacts. The receipt also records
the current Bend production build fingerprints. `sources.sha256` pins the
repository inputs read by the reference run and the slice files; verify it
from the repository root with:

```sh
shasum -a 256 -c dev/validation/bend2-equality-runtime-live/sources.sha256
```

The fifth-slice receipt is in `dev/validation/bend2-composition-runtime-live/`.
It records 139 live OCaml and 139 native Bend observations across the CLI,
core CLI, equality runtime and composition runtime suites, plus 42 refusal
controls. This is a scoped run of those four suites. The new composition corpus
was recorded from a fresh OCaml build and then checked by another fresh build.
The receipt includes the reference report, native build fingerprint, full
command output, and the first control run's timeout failure. The equality
timeout control previously expired before Python printed its marker under
parallel load. Both runtime control scripts now skip Python site initialization
and allow two seconds for the deliberately sleeping child. If the child prints
nothing before that limit, the script tries again with five seconds and then
with ten seconds. Normal observations retain their 20-second limit. The
complete control target passed on rerun.
Verify the source and evidence pins from the repository root with:

```sh
shasum -a 256 -c dev/validation/bend2-composition-runtime-live/sources.sha256
```

The sixth-slice receipt is in `dev/validation/bend2-reuse-runtime-live/`.
It records 243 live OCaml and 243 native Bend observations across the CLI,
core CLI, equality, composition and reuse runtime suites, plus 68 refusal
controls. The 104 new observations were recorded from a fresh OCaml build
and compared with a second fresh build and a freshly compiled native Bend
executable. Runtime comparisons and the new live refusal controls used four
workers. The receipt includes full output, per-case observations, source pins
and the production build fingerprint. The full 3,877-case corpus was not rerun.

Earlier attempts with the full category and functor reuse fixtures exceeded
20- and 60-second limits during OCaml recording and a 60-second limit in a
native probe under local load. Their logs are retained in the receipt. Those
broader fixtures remain outside this slice's passing coverage. The final
focused fixtures retain the 20-second observation limit.

```sh
shasum -a 256 -c dev/validation/bend2-reuse-runtime-live/sources.sha256
```

The completion batch reran all twelve suites against the live OCaml checkout
at slice 6 (`7a2f9f2`). All 3,877 cases passed with zero differences and zero
input drift. The receipt is
`dev/validation/bend2-completion/reference-report.json`. The comparator's four
refusal suites also passed. This comparison covers the case sets above; the
larger category and functor fixtures described above still require their
separate runtime validation.
