# Bend 2 live reference checks

The second port slice checks the frozen comparison cases against the live
OCaml implementation in this repository. Run the full comparison with:

```sh
make bend2-diff
make bend2-reference-test
```

`bend2-diff` builds fresh OCaml libraries and seven observation adapters,
compares their results with the checked-in expectations, verifies the Bend 2
build, and replays the same eight case sets through Bend 2. The Bend builder
checks source, compiler, and artifact fingerprints before reusing binaries.
Select the intended OCaml switch in `PATH`. The reference build needs `git`,
`dune`, `ocamlfind`, `ocamlc`, `ocamlopt`, `ocamlrun`, and the `zarith` and
`unix` packages. The reference directory must be the root of a git work tree,
because the report records its `HEAD` commit.
The Bend toolchain is configured as for `make bend2-build`.

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
| Total | 3,671 |

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
using a real OCaml build and temporary case files.

For a scoped comparison or another live OCaml checkout:

```sh
python3 -P dev/bend2-reference-check.py --suite json \
  --reference /path/to/mechanism-lang --output /tmp/mechanism-json-reference
```

An explicit output directory must be new. Inside the reference checkout it
must be under this runner's `_bend2/reference` directory. Completed reports
and observation files remain available for inspection. The checked-in slice
validation receipt is in `dev/validation/bend2-reference-live/`. `report.json`
gives the result for each case set and the hashes of the slice files.
`sources.sha256` pins the source files and case files that the run read.
`tools.json` pins the OCaml tools. The `*.stdout` and `*.stderr` files contain
the command output. To check the source pins, run
`shasum -a 256 -c dev/validation/bend2-reference-live/sources.sha256` from the
repository root. `report.json` also pins the Bend 2 drivers, their build
fingerprints, the `bend2/` tree, and the Bend 2 check scripts that `bend2-diff`
used.
