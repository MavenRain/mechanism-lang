# Rust importer (mech-rust M1, unit C)

`mech rust-in <crate dir> <out dir>` reads a Rust crate and writes a mechanism-lang program.
The importer accepts only the Rust text that the emitter writes (EMIT.md). It refuses all other text.
The round-trip laws tested on the seed corpus, their scope and their limits are in ROUNDTRIP.md.

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
If the crate has a `mech-carrier.mech` file, the command merges it into the import. Each carried chunk starts with a key line: `-- at <file> start` or `-- at <file> after <name>` (see `EMIT.md`). The command puts the chunk in `<file>`: at the start, or after the declaration with the name `<name>`. Chunks with the same key keep the order of the carrier. The key line is not written. The kernel check runs on the merged text, so it checks each carried proof against the Rust text. The command refuses a chunk with no key line, and a key with no place in the import. The command also copies the carrier file with no change.
The parent of the output directory must exist.

The carrier's optional second line `-- files <file> ...` restores source-file order before the kernel check. It must name every imported file exactly once; unknown, repeated, or omitted files are refused. Without that line, the importer keeps the order derived from Rust dependencies. Current `rust-out` always writes it, because Rust dependencies alone omit proof-only edges. The kernel still checks the complete merged program in the selected order.

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
- CARRIER: the import of the golden crate with its carrier fails with `MECH-CHECK-FAIL` and writes no file. The golden crate is not a canonical program, and a carried declaration does not agree with the imported text.
- GOLDEN: the import of the golden crate with no carrier file is equal to `test/rust/import/expected/`, with no extra output files.
- MECH-CHECK: the imported program passes the kernel check with no axiom.
- RT-RUST: the emitter gives each file of the golden crate back from the imported program, with the same root and source file inventory. The carrier is not compared.
- REFUSE: each fixture gives its row of `EXPECTED.tsv` and leaves no output path, including a dangling symlink.
- RED: two mutation controls. A swap of two constructor names in an imported file must fail RT-RUST. A recursive call on the full arguments must fail the kernel check with the totality error.

`dev/rust-infer-gate.py`, `dev/rust-lower-gate.py` and `dev/rust-in-cli-gate.py` have more cases for inference, lowering and the command.

## Known limits

- An item sees the items of the used modules, the earlier items of its module, and itself. `resolve` refuses a use of a later item: `use of the function `<name>` before its declaration (mutual recursion or a forward reference)`. The position is the position of the item that has the use, not of the use (fixtures `06_mutual_recursion`, `11_forward_reference`). No fixture has a use of a later type or of a later constructor.
- A struct with fields or with generics is not in this fragment. The refusal is `struct `<name>` with fields` or `struct `<name>` with generics` (fixtures `02_struct_fields`, `10_tuple_struct`). A unit struct with no constructor fn keeps its text (fixture `09_unit_no_ctor`). No fixture has a struct with generics.
- `bad__name` is the D7 name of the mech name `bad_Name`, and `good_` is the D7 name of `good_`. The two Rust names import, and the seed `09_names` shows their round trip. The emitter refuses a mech name that does not come back (`EMIT.md`, rule D7). For example, the mech name `bad__name` gives the same Rust name as `bad_Name` and comes back as `bad_Name`. The ROUND-TRIP gate has three controls for this refusal: `names_double_underscore`, `names_keyword_tail` and `names_upper_ctor` in `test/rust/seed-control`.
- `lower` removes one leading `_` from a local binder. A used binder `_x` becomes `x`.
- No fixture has a match or an `if` with no expected type.
- The copy of the carrier in the output directory is a record only. The emitter writes a new carrier from the imported files.
- The merge is for a canonical program. If a carried declaration does not agree with the imported text, the kernel check fails and the command refuses the crate. The golden crate of M0 is such a crate: in the import, `AuctionOrder` takes no argument, and a carried declaration gives it two arguments.
- `dev/BEND2-BASELINE.json` has old hashes of `bend2/cli/mech.bend`.
- The ROUND-TRIP gate (`dev/rt-mech-gate.sh`, `ROUNDTRIP.md`) checks RT-MECH, FIXPOINT and RT-RUST on the nine programs in `test/rust/seed`. It does not establish these laws for every program in the M0 fragment. DIFF-EXEC (`dev/rust-out-diff-exec.sh`) runs the golden crate only.
