# Rust importer (mech-rust M1, unit C)

`mech rust-in <crate dir> <out dir>` reads a Rust crate and writes a mechanism-lang program.
The importer accepts only the Rust text that the emitter writes (EMIT.md). It refuses all other text.

## Files

- `lift.bend`: the syntax tree of unit A becomes the IR of the emitter (`rir.bend`). It is the inverse of `emit.bend`.
- `resolve.bend`: module order, item table, names.
- `infer.bend`: signatures become telescopes. Each body gets the type arguments that the kernel needs.
- `lower.bend`: the IR becomes surface declarations. The surface printer writes the text.
- `../cli/rust_in.bend`: the pipeline and the command.
- `../tests/rust_import.bend`: the driver. Its modes are `lift`, `resolve`, `types`, `lower`, `check` and `run`.

## Command

`mech rust-in <crate dir> <out dir>` reads `src/lib.rs`, then `src/<m>.rs` for each `pub mod` line.
It writes one `.mech` file for each module and the file `MANIFEST`. `MANIFEST` has one file name on each line, in dependency order.
If the crate has a `mech-carrier.mech` file, the command copies it with no change.
The parent of the output directory must exist.

| Exit code | Cause |
| --- | --- |
| 0 | The import is complete. |
| 1 | The output directory exists, or a write failed. |
| 64 | The arguments are wrong, or a file cannot be read. |
| 65 | A refusal or a kernel failure. The command writes no file. |

## Pipeline

1. Unit A makes the tokens, runs the fragment pass and parses each module (FRONTEND.md).
2. `lift` makes the IR. The law is: the emitter text of the lift of a file is equal to the file.
3. `resolve` makes the module order and the item table, and gives each name its mechanism-lang name.
4. `infer` checks each body against its signature.
5. `lower` writes the declarations.
6. The kernel checks the files, joined in manifest order. An import with a kernel error writes no file.

## Rules

- Module order: the topological order of the `use crate::<m>::*;` lines. Byte order of the module name breaks a tie. A module cycle is refused.
- Output file name: the file name in the `//!` line of the emitter. If there is no such line, the name is `<module>.mech`.
- Names: the inverse of rule D7 (EMIT.md). A Rust name is canonical if D7 of its inverse gives the name back. A name that is not canonical is refused.
- `nat`: the importer does not parse `src/nat.rs`. The file must be byte-equal to the fixed text of the emitter. `Nat`, `nat_small(<n>)` and `nat_add` become the kernel `Nat`, the literal and `natAdd`.
- `bool`: if the crate uses `bool`, the first file in manifest order starts with the family `MechBool`. The emitter makes no Rust item for it.
- Quantities: a reference and an owned `Copy` type have the quantity many. An owned type that is not `Copy` has the quantity 1. A generic is a quantity-0 type parameter.
- Recursion: a `fn` that calls itself becomes `def rec`. The kernel totality check refuses recursion that is not structural.

## Refusals

A refusal is one line on stderr: `REFUSED <file>:<line>:<col>: <construct>`.
A refusal of `lift` has the position of the nearest statement, arm or item. A refusal of `resolve`, `infer` or `lower` has the position of its item.
A kernel failure is the line `MECH-CHECK-FAIL <error>`. It has no position.

## Fixtures and gate

- `test/rust/import/expected/`: the import of the golden crate `test/rust/emit/crate`.
- `test/rust/import/refuse/`: one small crate for each refused construct. `EXPECTED.tsv` gives the exit code and the first error line of each crate (name, code, line; tabs).

Run `zsh dev/rust-in-gate.sh`. The last line is `RUST-IN-OK` or `RUST-IN-FAIL`. The gate has these checks:

- LIFT: the lift of each module file of the golden crate prints back byte for byte.
- GOLDEN: the import of the golden crate is equal to `test/rust/import/expected/`, with a byte-equal carrier copy and no extra output files.
- MECH-CHECK: the imported program passes the kernel check with no axiom.
- RT-RUST: the emitter gives each file of the golden crate back from the imported program, with the same root and source file inventory. The carrier is not compared.
- REFUSE: each fixture gives its row of `EXPECTED.tsv` and leaves no output path, including a dangling symlink.
- RED: two mutation controls. A swap of two constructor names in an imported file must fail RT-RUST. A recursive call on the full arguments must fail the kernel check with the totality error.

`dev/rust-infer-gate.py`, `dev/rust-lower-gate.py` and `dev/rust-in-cli-gate.py` have more cases for inference, lowering and the command.

## Known limits

- Mutual recursion fails in the kernel check, not in `resolve`. The line is `MECH-CHECK-FAIL <name>` with no position (fixture `06_mutual_recursion`).
- A struct with fields is refused with the text of a unit struct that has no constructor fn (fixture `02_struct_fields`).
- A name with two underscores in sequence is canonical by the D7 rule. `bad__name` imports as `bad_Name`.
- `lower` removes one leading `_` from a local binder. A used binder `_x` becomes `x`.
- No fixture has a match or an `if` with no expected type.
- The command copies the carrier. It does not compare the carrier with the imported text.
- `dev/BEND2-BASELINE.json` has old hashes of `bend2/cli/mech.bend`.
- RT-MECH and DIFF-EXEC on the imported program are not gates of this unit. RT-RUST gives the golden crate back, and `dev/rust-out-diff-exec.sh` runs that crate.
