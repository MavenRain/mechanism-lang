# Rust frontend (mech-rust M0, unit A)

This directory holds the Rust frontend of the mech-rust transpiler. The frontend is written in Bend 2. It reads Rust source text in the house-convention subset, refuses each construct outside that subset, and prints the syntax tree again as rustfmt-stable text. It does not use `syn` or `rustc`.

## Files

| File | Content |
| --- | --- |
| `token.bend` | Tokens with one-based `line:col` positions and a blank-line flag (`gap`). |
| `lexer.bend` | Source text to tokens. Keywords stay identifiers. A non-doc comment becomes a `Comment` token. |
| `fragment.bend` | The fragment pass: a walk over the tokens that gives all refusals in source order. |
| `ast.bend` | The syntax tree. Items and expressions share one recursive `Node` type. |
| `parser.bend` | Tokens to `A.SrcFile`. Entries: `Parser.file(source)` and `Parser.from_tokens(tokens)`. |
| `print.bend` | `A.SrcFile` to text with the rustfmt layout (default configuration, edition 2021). |
| `../tests/rust_frontend.bend` | The driver. |

## Pipeline

`check` is the full frontend: lex, fragment pass, parse, print.

1. The lexer gives the token list, or `LEX-FAIL <line>:<col> <message>`.
2. `Fragment.refusals(tokens)` gives the refusals. If the list is not empty, the driver prints one `REFUSED <line>:<col> <construct>` line for each refusal and stops.
3. `Parser.from_tokens(tokens)` gives the tree, or `PARSE-FAIL <line>:<col> expected ..., found ...`.
4. `Pr.file(tree)` gives the text.

The driver modes are `lex`, `parse`, `print`, `check` and `pos`. Only `check` runs the fragment pass.

```sh
bend bend2/tests/rust_frontend.bend check "$(cat file.rs)"
```

Each `Meta` of the tree has a position `pos` (`A.Pos`, one-based `line:col`, the same text as a token position). It is the position of the first token of the construct after its outer docs and attributes. The constructs with a `Meta` are items, fields, variants, statements and match arms. Expressions, types and patterns have no position of their own. The printer does not read `pos`, so the round-trip law does not change. A tree from `emit.bend` has the position `0:0` in each `Meta`. The mode `pos` prints one `<line>:<col> <kind> <name>` line for each top-level item.

## Round-trip law

For each accepted fixture `f` in `test/rust/parse`, `f` is rustfmt-clean and `print(parse(f)) == f` byte for byte.

## Refused constructs

The position is the first character of the token in the third column.

| Construct name | Token rule | Position |
| --- | --- | --- |
| `loop` | keyword `loop` | keyword |
| `while` | keyword `while` | keyword |
| `for` | keyword `for`, but not between `impl` and the next `{` or `;` | keyword |
| `&mut` | `&` (or the second `&` of `&&`) then `mut`; a lifetime between them is permitted and gives its own refusal | `&` |
| `mut binding` | each other `mut` | keyword |
| `unsafe` | keyword `unsafe` | keyword |
| `.unwrap()` | `.` then `unwrap` then `(` or `::` | `.` |
| `.expect(` | `.` then `expect` then `(` or `::` | `.` |
| `.scan(` | `.` then `scan` then `(` or `::` | `.` |
| `panic!` | `panic` then `!` | name |
| `assert!` | `assert` then `!` | name |
| `assert_eq!` | `assert_eq` then `!` | name |
| `macro <name>!` | each other identifier then `!`, but not `vec!` | name |
| `as` | keyword `as`, but not between `use` and the next `;` | keyword |
| `indexing` | `[` after an identifier, a literal, `)`, `]` or `?` | `[` |
| `dyn Trait` | keyword `dyn` | keyword |
| `lifetime` | each lifetime token | `'` |
| `async` | keyword `async` | keyword |
| `.await` | `.` then `await` | `.` |
| `comment` | each line comment or block comment that is not a doc comment | comment start |

A doc comment (`///`, `//!`) is kept. A raw identifier (`r#loop`) is an identifier and not a keyword.

A construct that is outside the grammar and has no rule in this table gives a parse failure and not a refusal.

## Fixtures and gate

- `test/rust/parse/*.rs`: accepted fixtures.
- `test/rust/refuse/*.rs`: refused fixtures, one for each construct name. Each fixture has exactly one refusal.
- `test/rust/refuse/EXPECTED.tsv`: one row for each refused fixture, with no header. The columns are the file name, the `line:col` position and the construct name. The separator is a tab.

```sh
dev/rust-parse-gate.sh
```

The gate builds the driver one time into a temporary directory as JavaScript (`bend <driver> -o <file>.js`, about one minute) and runs `check` on each fixture with `node` (about one second for each run). The gate does not use a native build: with Bend 2.0.25 a native build of the driver needs 8 to 20 minutes. It prints one `PASS` or `FAIL` line for each fixture, and then `RUST-PARSE-OK` (exit status 0) or `RUST-PARSE-FAIL` (exit status 1). The gate also fails if there are fewer than 14 accepted fixtures or fewer than 20 refused fixtures, or if the rows of `EXPECTED.tsv` and the refused fixtures do not agree one to one. `rustfmt` and `node` must be on the `PATH`. Set `BEND` to use a Bend binary other than `~/.bend/bin/bend`, and `NODE` to use a different Node.js binary.

## Known limits

Fragment pass:

- The rules use tokens only. `Option::unwrap(x)` and a method passed by path are not refused.
- A `[` directly after `}` is not an index refusal. The parser fails there.
- `*mut T` has the name `mut binding`. An `as` in a qualified path (`<T as Trait>::f`) has the name `as`. A loop label has the name `lifetime`.

Printer:

- A raw identifier prints without `r#`.
- A binary expression breaks only after the last operator. There is no rustfmt layout for operator chains.
- A match arm body that is too wide is not put in a block.
- A long `if` condition, long function generics and a long use tree do not break.
- Tuple-struct fields and generic lists are always on one line.
