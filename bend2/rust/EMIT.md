# Rust emitter (mech-rust M0, unit B)

This directory also holds the Rust emitter of the mech-rust transpiler, the `mech rust-out` verb. The emitter is written in Bend 2. It checks `.mech` files with the kernel, erases the checked terms to a small Rust IR, and writes a Cargo crate with no dependency. A declaration with no Rust form goes verbatim to `mech-carrier.mech`. The M0 inputs are `prelude/init.mech` and `prelude/mechanism/second-price.mech`. The round-trip laws tested with the importer (IMPORT.md) on the seed corpus, their scope and their limits are in ROUNDTRIP.md.

## Files

| File | Content |
| --- | --- |
| `rir.bend` | The Rust IR: items (`use`, enum, unit struct with its constructor function, function), types, expressions and patterns. Each `&`, `.clone()` and `Box::new` is an explicit node. |
| `erase_typed.bend` | The class of each declaration, and the typed erasure of families, signatures and bodies to the IR. |
| `emit.bend` | IR to text. It lowers a module to the syntax tree of the frontend (`ast.bend`) and prints it with `print.bend`. |
| `../cli/rust_out.bend` | The `rust-out` verb: module names, `use` lines, the carrier, the text of `src/nat.rs` and the crate write. |
| `../tests/rust_emit.bend` | The driver. |
| `../tests/rust_emit_oracle.bend` | The oracle of the gate: kernel values, probe generation and the text of `src/bin/diff_exec.rs`. |

## Command

```sh
mech rust-out FILE.mech... OUT_DIR
```

The last argument is the output directory. The arguments before it are the input files in order. The emitter checks file `i` in the globals of the files before it. It starts from checked kernel terms and `Global` entries, not from the untyped erased terms.

| Result | Exit code | Output |
| --- | --- | --- |
| No input file | 64 | The usage line. |
| The output directory exists | 1 | `mech: output already exists: <out>`. The emitter does this test before it reads a file. |
| Refusal | 65 | One or more `refused` lines on stderr. No crate is written. A file after a refused file is not read. |
| Success | 0 | The crate. |

The crate:

- `Cargo.toml`: package `mech_out`, edition 2021, license `MIT OR Apache-2.0`, no dependency.
- `src/lib.rs`: one `pub mod` line for each module.
- `src/<module>.rs`: one module for each input file. The first line is ``//! Emitted by `mech rust-out` from `<file>`.``
- `src/nat.rs`: the kernel `Nat` as a bignum with no dependency (`Nat`, `nat_small`, `nat_add`, `nat_decimal`). The emitter writes it only if a module uses it.
- `mech-carrier.mech`: the carrier (see below).

The emitter writes the crate into a temporary directory next to the target and then renames it one time.

## Classes

Each declaration has one class. The driver mode `classify` prints one line for each declaration. A constructor gets no line.

| Line | Class | Destination |
| --- | --- | --- |
| `fn <name> = pub fn <signature>` | Runtime function | Module |
| `data <name> = [copy ]enum ...` or `data <name> = struct <name>; fn <ctor>` | Runtime family | Module |
| `bool <name>` | `MechBool` | Rust `bool` |
| `prop <name>` | A family in `Prop`, a proof, or a definition with a value in `Prop` | Carrier |
| `type <name>` | A definition with a value in `Type n` (`MechPi`, `MechSigma`) | Carrier |
| `absurd <name>` | A function with no Rust body after erasure (`mechFalseElim`) | Carrier |
| `refused <name>: <why>` | Outside the M0 fragment | No crate |

The sort test is syntactic: the level of a family, the universe at the end of a telescope, and the kind of a bound variable. The emitter refuses a shape outside these rules.

For the two M0 inputs the table has 22 `fn` lines, 7 `data` lines, 1 `bool` line, 23 `prop` lines, 2 `type` lines and 1 `absurd` line.

## Erasure rules

Families:

- A family becomes a `pub enum`. A field with quantity 0 or with a type in `Prop` is dropped. A recursive field is `Box<T>`. Indices, universe levels and `Prop` parameters are dropped.
- Each item derives `Clone, Debug, PartialEq, Eq`. An enum with no field (a copy enum) and a unit struct also derive `Copy`.
- A family with one constructor and no kept field becomes a unit struct and a constructor function (`MechUnit`, `mech_unit()`).
- `MechBool` is `bool` by name. Its constructors are `true` and `false`, and a case on it is `if`/`else`.
- The kernel `Nat` is `Nat` of `src/nat.rs`. An integer literal is `nat_small(<n>)`. `natAdd` is `nat_add`. There is no other primitive.

Signatures:

- A type binder with quantity 0 becomes a generic with a `Clone` bound. A generic stays only if it occurs in the erased signature. A binder `0 P : ... -> Type n` is a generic, and `P a b` is `P`.
- Each other binder with quantity 0 and each binder with a type in `Prop` is dropped.
- A binder with a function type is `&impl Fn(..) -> T`.
- A parameter name comes from the lambda binder of the body. A parameter with no use after erasure is `_name`.
- `def rec` becomes a recursive function.

Ownership:

- The Copy set is `bool`, the copy enums and the unit structs.
- A binder with quantity 1 takes its value. A binder with quantity many is `&T`, or takes its value if `T` is in the Copy set.
- A `&T` variable in an owned position gets `.clone()`. An owned variable or a compound expression in a `&T` position gets `&`. A value in the Copy set never gets `.clone()`.
- A match on a family outside the Copy set borrows the scrutinee, and its pattern variables are references.
- A closure parameter takes the borrow mode of the declared parameter type of the callee: a declared `&B` with `B = bool` gives `|b: &bool|`.

Terms:

- A constructor becomes an enum variant, a call of the constructor function of a unit struct, or a `bool` literal. A boxed field gets `Box::new`.
- A case becomes a `match` with one arm for each constructor and no `_` arm. A field with no use is `_`.
- A case with one branch on an erased `Prop` scrutinee becomes the body of the branch (`mechJ`).
- A match where each arm builds its own pattern again becomes the scrutinee. `if c { true } else { false }` becomes `c`.
- The erased type arguments of a call go in a turbofish (`mech_nat_rec::<P>(..)`). A dropped generic takes no turbofish argument.
- A lambda in an argument position becomes a closure.

## Names (rule D7)

- A definition `camelCase` becomes a function `snake_case`. A constructor becomes an `UpperCamel` variant. A Rust keyword gets `_` at the end.
- The module name is the file stem with `-` changed to `_`. The stem must be a lower-case letter and then `[a-z0-9_-]*`. The module names `bin`, `lib`, `main` and `nat` are reserved.
- The emitter checks the name map for each crate. The item names of all modules, together with `Nat`, `nat_small`, `nat_add` and `nat_decimal`, must be different: `refused: name collision (D7): <name>`. The module names must be different: `refused: module name collision (D7): <name>`.
- The emitter checks that each item name comes back. The importer reads a Rust name with the inverse of this rule. If the result is not the mech name, the declaration is refused: ``refused <name>: a name that does not come back from the Rust name `<rust>`: the importer gives `<back>` (D7)``. The check applies to a function, to each constructor of an enum and to the constructor of a unit struct. For a constructor, `<name>` is the family. Examples: `bad__name` (the importer gives `bad_Name`), `type_` (`type`), `foo_bar` (`fooBar`), the constructor `Red` (`red`). A family name is kept, so it has no check.
- Limit: the names of parameters and of local binders have no check. Two parameters `fooBar` and `foo_bar` get the same Rust name, and the Rust text has the name two times. No gate shows this.
- A module gets `use crate::<module>::*;` only for a module before it that has an item that it mentions, and for `nat`. The lines are sorted.

## Carrier

`mech-carrier.mech` has one header line. Then, for each input file that has a carried declaration, it has a `-- from <file>` line, an empty line and the carried chunks in source order.

- The emitter splits the text of an input file into chunks. A chunk starts at a line with a lower-case letter in column 1. The `--` lines directly before that line go with the chunk.
- Chunk `i` is declaration `i` of the parser. The emitter refuses a file if the counts are different, if a chunk does not contain the first name of its declaration, or if a declaration has runtime names and names that are not runtime names.
- The carried classes are `prop`, `type` and `absurd`.
- The second carrier line is `-- files <file> ...`, listing every source file in source order, including files with no carried declarations. Proof-only dependencies disappear from Rust, so this order is needed to check the merged program.
- Each carried chunk has a key line first: `-- at <file> start` or `-- at <file> after <name>`. `<name>` is the first name of the nearest runtime declaration before the chunk in the same file. `start` shows that no runtime declaration is before the chunk. `rust-in` reads the key and puts the chunk back at that place (see `IMPORT.md`). The emitter is the only writer of key lines.

For the two M0 inputs the carrier has 26 declarations (10 `prop`, 2 `type` and 1 `absurd` from `init.mech`, 13 `prop` from `second-price.mech`).

## Refusals

A refusal line is `refused <name>: <why>`. The marks in a reason are: `(M0)` for a construct that is not in this milestone, `(D3)` for a type-valued definition in a runtime type, `(D4)` for an empty elimination of an erased `Prop` scrutinee, and `(D7)` for the name rules.

| Group | Reasons |
| --- | --- |
| Declarations | `a postulate in the runtime`, `a primitive`, `a function with no definition` |
| Families | `a type parameter with no use in a field`, `a type-valued constructor field`, `a family with too few arguments`, `an unknown family: <name>` |
| Names | ``a name that does not come back from the Rust name `<rust>`: the importer gives `<back>` (D7)`` |
| Types | `a type-valued definition in a runtime type (D3): <name>`, `a value in a type`, `a polymorphic function type`, `a function type with erased arguments only`, `a Prop as a type argument`, `a proposition as a type argument`, `a type argument with a runtime quantity`, `an unknown name in a type: <name>`, `a type shape outside the M0 fragment` |
| Calls | `a call of an absurd function (D4)`, `a partial application (M0)`, `more arguments than the telescope of the callee (M0)`, `a call of a name that is not a function`, `a call of a name with no entry`, `a call of a variable that is not a function`, `an application head outside the M0 fragment` |
| Terms | `a case with no branch (D4)`, `a case on a family outside the M0 fragment`, `a constructor of an item that is not data`, `a constructor outside the M0 fragment: <name>`, `a let of a function type (E1)`, `a let of a non-Copy type (E1)`, `a let of a type (E1)`, `the let binder `<name>` collides with a name in scope after the name map (E1)`, `a literal above u32 (M0)`, `a literal that is not an integer (M0)`, `a boxed field by value (M0)`, `an erased variable in a runtime position`, `a term shape outside the M0 fragment` |

## Driver

```sh
bend bend2/tests/rust_emit.bend -o re.js
node --stack-size=16384 re.js <mode> <arguments>
```

| Mode | Output |
| --- | --- |
| `sample <init\|second>` | The text of a module from an IR that is written by hand. |
| `classify <file.mech> [<file.mech>]` | The class table, then `names: no collision` or the collision line. |
| `emit <file.mech> [<file.mech>]` | The text of each module. A refusal gives exit code 65 and no module text. |
| `crate <file.mech>... <out dir>` | The crate. This mode calls the same `run` as `mech rust-out`. |
| `probes <file.mech>...` | The generated probes. |
| `values <file.mech>...` | One line `<name> = <value>` for each declared name of the last file, from the kernel evaluator. An evaluation error gives `<name> ! <why>`. |
| `bin <file.mech>...` | The text of `src/bin/diff_exec.rs`: one `println!("{:?}", <name>());` line for each declared name of the last file. |

## Fixtures and gate

- `test/rust/emit/crate/`: the golden crate of the two M0 inputs (`Cargo.toml`, `mech-carrier.mech`, `src/lib.rs`, `src/init.rs`, `src/nat.rs`, `src/second_price.rs`).
- `test/rust/emit/generated-probes.mech`: the golden of the generated probes (218 probes of 13 functions).
- `test/rust/emit/probes.mech`: 20 probes that are written by hand, for the generic functions, the higher-order functions, and the functions with an erased parameter or a dependent result.
- `test/rust/emit/values.txt`: the golden of the kernel values (238 lines).
- `test/rust/emit/neg_classify.mech`: a negative fixture with one postulate in the runtime and one name that does not come back. The name collision check stays in the code. Two fn definitions cannot reach it any more, because one of the two names does not come back and is refused first.
- `test/rust/parse/15_empty_match.rs`: a frontend fixture for `match e {}`, which the printer now prints.

A probe is a closed runtime definition. The DIFF-EXEC law: for each probe, the value from the kernel evaluator, printed in the Rust `{:?}` form, is equal to the line that the emitted Rust prints. A constructor value is the variant name and its kept fields (`MechSucc(MechSucc(MechZero))`), a `Nat` is decimal, a `MechBool` is `true` or `false`, and a unit struct is its name.

The generated probes are for each runtime function that has no generic, a closed result type (`MechBool`, `Nat`, or a family with no parameter and no index) and an input set for each parameter: the two values of `bool`, the values 0, 1 and 2 of `MechNat`, and each constructor of a copy enum with no parameter. There is one probe for each combination. The name is `probe<Function><Labels>`.

```sh
dev/rust-out-diff-exec.sh
```

The gate does these steps:

1. It builds the driver one time as JavaScript below `$TMPDIR/mech-diff-exec`.
2. It writes the crate of the two M0 inputs and compares the six files with the golden crate.
3. It compares the generated probes and the kernel values with their goldens.
4. It writes a second crate with a third module `probes` (the generated probes, then `probes.mech`) and `src/bin/diff_exec.rs`.
5. It builds that crate with `cargo build` and runs `diff_exec`. It prints one `PASS` or `FAIL` line for each value.
6. Mutation control: in a copy of the crate, it exchanges the patterns of the two arms of each match on `AuctionChoice` in `src/second_price.rs`. The gate fails if no value of the copy is different.
7. It prints `DIFF-EXEC-OK <n> values` (exit status 0) or `DIFF-EXEC-FAIL` (exit status 1).

Each crate is below `$TMPDIR`, outside the tracked tree. The argument `emit` stops the gate before the cargo steps and prints the run directory. `node`, `cargo` and `python3` must be on the `PATH`. Set `BEND` to use a Bend binary other than `~/.bend/bin/bend`, and `NODE` to use a different Node.js binary.

Stack rule: the gate sets `ulimit -s` to the hard limit and runs `node --stack-size=16384`, as `dev/BEND2-BASELINE.json` does. With the default stack, the JavaScript build of the driver stops with `memory fault` when it writes the probe crate.

## Let (E1)

- A `let x : T := v in b` with a runtime binder gives `let x: T = v;` and then the statements of `b`. A let in the tail position of a fn body is one statement of the fn block. A let in an argument position is a block `{ let x: T = v; b }`.
- The binder must have a Copy type (`bool` or a Copy family). A binder of a function type, of a non-Copy type or of a type is refused. A binder of a proof type is erased, as a proof parameter is.
- The Rust name of the binder must not be equal to the Rust name of a parameter, of an outer binder or of a fn of the module (rule ED2). The emitter refuses the let, it does not rename the binder.
- Fixture: `test/rust/emit/let_probe.mech` (two lets in the tail, one let in an argument). The refusals are lines 3 to 6 of `test/rust/emit/neg_classify.mech`: three name collisions and one binder of the non-Copy type `MechNat`.

## Known limits

Emitter:

- The only struct form is the unit struct. A struct with private fields and accessors is not in M0.
- There is no string literal, no integer literal above `u32`, and no primitive other than `natAdd`. A `let` binds a value of a Copy type only (E1).
- `if c { false } else { true }` does not become `!c`. The IR has no node for it.
- A field of a type in the Copy set below a borrow pattern has no dereference node.
- A closure parameter adapts only to a borrow (a declared `&T` and a binder type in the Copy set). If the body uses such a binder by value, the emitter gives `.clone()`. No probe has this case.
- The use count for `_name` and `_` counts a shadowed name as a use. The result is safe: the name stays.
- A local closure with the name of a function of an earlier module counts as a use of that module.
- `mechFalseElim` is `absurd`: a runtime definition that calls it is refused.
- The carrier is not a complete program. It refers to runtime names, and `mech` does not check it directly. `rust-in` merges the carrier into the import, and the kernel checks the merged text.
- A write failure can leave an `<out>.tmp-*` directory.

Tests:

- The gate checks the two M0 inputs only. The only refusals with a fixture are the postulate and the name that does not come back of `neg_classify.mech` (the name collision check is not reachable with two fn definitions), and the gate does not run that fixture. The other reasons in the refusal table have no fixture.
- The ROUND-TRIP gate (`dev/rt-mech-gate.sh`, `ROUNDTRIP.md`) runs the emitter on the seed corpus `test/rust/seed` and on the control programs of `test/rust/seed-control`. The three `names_*` controls are the fixtures of the name that does not come back.
- `auctionCompare` (dependent result) and `auctionChoice` (erased parameters) get no generated probe. Their probes are in `probes.mech`. `mechEmptyElim` has no closed input and no probe.
- A first-order function with an erased binder that the probe rule does not find gives probes that do not check. The gate then stops at `kernel values`.
- The gate runs the driver, which calls the same `run` as the verb. It does not build `bend2/main.bend`, so it does not run the dispatch case of `mech rust-out`.
- The crate write has a recursion whose depth grows with the size of a module. The stack rule hides it for the M0 inputs. The cause is not found.
