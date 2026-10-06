# Round trip (mech-rust M1, unit D)

E is `mech rust-out` (EMIT.md) and I is `mech rust-in` (IMPORT.md). This page gives the round-trip laws tested on the nine seed programs below and the gate that checks them. The gate establishes these results for that corpus; it does not establish them for every program in the M0 fragment.

## Files

| File | Content |
| --- | --- |
| `roundtrip.bend` | The compare `alpha`: two kernel terms are equal up to bound names. |
| `../tests/rust_import.bend` | The driver. The modes `seed-check` and `rt-mech` serve the gate. |
| `../../dev/rt-mech-gate.sh` | The ROUND-TRIP gate. |
| `../../test/rust/seed/` | The seed corpus: one directory for each program. |
| `../../test/rust/seed-control/` | The control programs of the gate. |

## Canonical program

The canonical seeds are designed with these restrictions. They are not a complete syntactic test for whether another program satisfies the laws:

- Each runtime declaration has a Rust form: a family, a unit struct, a function with `match`, `if`, closures or structural recursion, the kernel `Nat` and a Boolean. The fragment is the M0 fragment of EMIT.md. There is no growth of the fragment in this unit.
- A runtime declaration has no quantity-0 binder that is not a type, no erased `Prop` field, no index and no postulate.
- A declaration with no Rust form has the class `prop`, `type` or `absurd`. The emitter carries it with a key line (EMIT.md, `## Carrier`).
- A program that uses a Boolean starts with its own family `MechBool`, in the form that IMPORT.md gives under `bool`.
- Item and file names satisfy the emitter's D7 rules, including the inverse name check. Names such as `bad__name`, `type_` and the constructor `Red` are refused even in this fragment. Parameter and local names must also avoid collisions after conversion to Rust names; the emitter does not check those collisions.
- The checked terms have the ascriptions the importer writes. For example, removing `(lightGreen : Light)`'s ascription in seed 01 still passes the kernel check and both commands, but gives `RT-MECH-FAIL lightNext`.

Acceptance by the emitter or importer alone is insufficient. A new candidate needs all four checks below; erasure and term simplification can change its checked form.

The two M0 inputs (`prelude/init.mech`, `prelude/mechanism/second-price.mech`) are not canonical. Erasure drops quantity-0 binders that are not types, `Prop` fields and indices. So the golden crate of M0 is outside these laws, and the RUST-IN gate refuses it with its carrier (IMPORT.md, check CARRIER).

## Laws

For each seed program S:

- SEED-CHECK: S passes the kernel check with no axiom.
- RT-RUST: E(I(E(S))) is equal to E(S), file for file.
- FIXPOINT: I(E(I(E(S)))) is equal to I(E(S)), file for file.
- RT-MECH: S and I(E(S)) have the same checked rows up to bound names.

The text of a program is its files in `MANIFEST` order, with a newline after each file. RT-RUST and FIXPOINT compare the output directories with `diff -r`.

RT-MECH is the mode `rt-mech` of the driver. It takes the two program texts, checks each one, and compares the rows with `alpha`. A kernel term has de Bruijn indices, and each binder keeps its source name for the printer only. So `alpha` is `Term.equal` with these names ignored: the name of `SPi` and `SZk`, the names of a leg, the `self` name and the index names of a motive, and the name of `Let`. The count of the index names of a motive is compared. Each other field is compared: quantities, family names, constructor names, global names, literals, levels, and an ascription `Ann` with its type. The compare is strict on ascriptions: an `Ann` must be on the two sides. The mode prints `RT-MECH-OK <n>` (n rows agree) or `RT-MECH-FAIL <name>` (the first row that differs).

The checked rows do not contain a separate row for a `mu` family. RT-MECH sees a family only through terms that use it. Two programs containing only a family with different constructor orders can therefore give `RT-MECH-OK 0`. RT-RUST supplies the separate check of emitted family structure; RT-MECH alone is not a comparison of all declarations. A kernel failure in either input gives `MECH-CHECK-FAIL left|right <error>`.

The mode `seed-check` prints `MECH-CHECK-OK <n> rows axioms=<k>` or `MECH-CHECK-FAIL <error>`.

## Seed corpus

Each program is a directory `test/rust/seed/<program>/`. It holds the `.mech` files and `MANIFEST`, one file name on each line, in dependency order. No expected Rust text is committed: the laws compare the outputs of the two commands.

| Seed | Files | Content |
| --- | --- | --- |
| `01_enum_match` | `light.mech` | `MechBool`, the family `Light`, `match` and `if` in `lightNext`, `lightGo`, `lightPick`. |
| `02_generic` | `choice.mech` | The generic family `Choice` and the generic functions `choiceFold`, `choicePick`. |
| `03_recursion` | `count.mech` | The unary natural `Count`, `def rec countFold` and `countDouble`. |
| `04_two_files` | `tally.mech`, `native.mech` | Two files with a dependency edge: `tallyNative`, `tallyBump`, `Tally`, `def rec tallyFold`. |
| `05_proofs` | `light.mech` | Seed 01 with the carried declarations `LightSafe`, `MechEq`, `lightGoGreen`. |
| `06_two_files_proofs` | `tally.mech`, `native.mech` | Seed 04 with the carried declarations `tallyNativeNone`, `TallyEq`, `tallyNoneSelf`. |
| `07_carrier_order` | `z_value.mech`, `y_equality.mech`, `a_proof.mech` | Proof-only edges across three files. |
| `08_carrier_bool_order` | `z_base.mech`, `a_proof.mech` | `MechBool`, `Safe` and a proof file that sorts before its base file in byte order. |
| `09_names` | `names.mech` | The names `bad_Name` and `good_`, whose Rust names are `bad__name` and `good_`. |

The control programs in `test/rust/seed-control/` are single files:

- `01_enum_match_renamed.mech`: seed 01 with each binder renamed. It passes RT-MECH against seed 01 (GREEN).
- `names_double_underscore.mech` (`def bad__name`), `names_keyword_tail.mech` (`def type_`), `names_upper_ctor.mech` (`mu Light` with the constructor `Red`): each one passes the kernel check, and the emitter refuses it (EMIT.md, rule D7).

## Carrier merge

E writes each carried chunk with a key line first: `-- at <file> start` or `-- at <file> after <name>`. The second line of the carrier is `-- files <file> ...`, the source order of all files. I puts each chunk back at its key, restores the source order, and runs the kernel check on the merged text. So the check of I(E(S)) checks each carried proof against the Rust text. The full rules are in EMIT.md `## Carrier` and IMPORT.md `## Command`.

## Gate

Run `zsh dev/rt-mech-gate.sh`. The gate builds the two drivers (`bend2/tests/rust_import.bend`, `bend2/tests/rust_emit.bend`) one time. All outputs stay in `$RT_WORK` (default `$TMPDIR/rt-mech-gate`). `BEND` gives the compiler (default `$HOME/.bend/bin/bend`).

For each seed program the gate prints one PASS or FAIL line for each check: SEED-CHECK, EMIT, IMPORT, EMIT2, RT-RUST, IMPORT2, FIXPOINT, RT-MECH. An import with no nonempty `MANIFEST` fails the check MANIFEST, and the gate goes to the next seed.

Controls, on seed `01_enum_match`:

- GREEN renamed binders: the control passes RT-MECH against the seed.
- RED branch bodies swapped: two branch bodies swapped in the imported text give `RT-MECH-FAIL lightGo`.
- RED constructors swapped: two constructors swapped in the imported text give a different crate (RT-RUST fails).

Controls of the carrier merge, on seed `05_proofs`:

- KEYS: the carrier of E(S) has one key line for each carried chunk (three).
- COPY: the import has a byte-equal copy of the carrier.
- REFUSE unknown, duplicate or missing file in the `-- files` line: exit code 65, `REFUSED carrier: ...`, no output.
- RED carried proof dropped: the last chunk dropped from the carrier gives `RT-MECH-FAIL`.
- RED fn body changed: each Boolean of the Rust text flipped makes a carried proof false, and the import fails with `MECH-CHECK-FAIL`, exit code 65, no output.
- REFUSE key with no place in the import: exit code 65, `REFUSED carrier: the import has no place for the key line: ...`, no output.
- REFUSE chunk with no key line: exit code 65, `REFUSED carrier: the first line of a chunk is not a key line: ...`, no output.

Controls of the name map, on the three `names_*` programs: SEED-CHECK passes, and `rust-out` gives exit code 65, the refusal line of rule D7, and no output.

The last two lines are `work=<dir> pass=<n> fail=<n>` and `ROUND-TRIP-OK` (exit code 0) or `ROUND-TRIP-FAIL` (exit code 1). The present count is `pass=99 fail=0`.

## Known limits

- The compare is strict on ascriptions. A seed program has the ascriptions that the importer writes, or RT-MECH fails on it.
- The names of parameters and local binders have no come-back check. Two parameters `fooBar` and `foo_bar` get the same Rust name (EMIT.md, rule D7).
- The seed corpus covers examples from the M0 fragment and the E1 `let`, not every program in those fragments. A struct with fields, `RsOption`, `RsResult`, `RsVec` and integers are not in the seed corpus. Seed `10_let` has lets in a tail, a match scrutinee and an `if` condition; a `let` in an argument position and a `let` with no type have no seed.
- The gate runs the seed corpus and the control programs only. The two M0 inputs are not canonical, and the DIFF-EXEC gate (`dev/rust-out-diff-exec.sh`) runs the golden crate.
- The copy of the carrier in the import is a record only. The emitter writes a new carrier from the imported files, and RT-RUST compares it.
- A refusal of a use before its declaration (mutual recursion or a forward reference) has the position of the item, not of the use. No seed has a use of a later type or constructor.
- The GREEN control compares against seed 01 only. No other seed has a renamed copy.
