# Bend 2 live reference checks

The live reference checks compare the frozen cases with the OCaml implementation
in this repository. The third port slice adds core CLI, Wasm emission and runtime
host comparisons. Run the full comparison with:

```sh
make bend2-diff
make bend2-reference-test
```

`bend2-diff` builds fresh OCaml libraries and seven observation adapters,
compares their results with the checked-in expectations, verifies the Bend 2
build, and replays the same nine case sets through Bend 2. The Bend builder
checks source, compiler, and artifact fingerprints before reusing binaries.
Select the intended OCaml switch in `PATH`. The reference build needs `git`,
`dune`, `ocamlfind`, `ocamlc`, `ocamlopt`, `ocamlrun`, and the `zarith` and
`unix` packages. The reference directory must be the root of a git work tree,
because the report records its `HEAD` commit.
The Bend toolchain is configured as for `make bend2-build`.
The core CLI cases also require `node`, `wasmtime`, `zsh`, and `rg` in `PATH`.
Their executable hashes and both runtime scripts are recorded in the report.

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
| Total | 3,699 |

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
python3 -P dev/bend2-reference-check.py --suite core-cli
python3 -P bend2/tests/cli_core_check.py
```

The frozen core CLI expectations remain in
`bend2/tests/cli_core_expected.json`. The reference runner and Bend replay use
the same case definitions and observation helper.

The surface corpus also references 18 unresolved historical attempts in
`dev/bend2/surface-baseline-unresolved.json`. Those attempts retain their
unresolved status and are outside the passing case inventory.

Run only the OCaml comparison with `make bend2-reference-check`. Each run
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
