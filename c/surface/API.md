# Native surface API

The frontend is implemented in C and built with tcc (`make c-build`). It
ports the Bend 2.0.25 frontend of commit 8c90b64. End-to-end semantic
validation is in progress.

- `token.c`: `Loc{line: Nat, col: Nat}`, `KindT`, and `Tok{kind: KindT, loc: Loc}`.
  `NatTok` stores canonical arbitrary-length decimal text. The parser converts
  that text with `Bignum.of_decimal`; syntax naturals are `Bignum.T`.
- `lexer.c`: `lex(source: String) -> Result<&2, &2, Error.T, List<&2, Token.T>>`.
  Token positions are one based and count UTF-8 bytes, matching the OCaml scanner.
- `syntax.c`: location-free `T` and `Decl`, with generic `Binder<A>`,
  `Motive<A>`, `Field<A>` and `Branch<A>` to avoid mutual datatype definitions.
  Universe literals, `SType`, `SProj`, `SInj` and `BrLeg` numeric payloads use
  `Bignum.T`; their source parser retains the original 18-digit limit.
  Binder positions, source locations and actual list lengths use `Nat`.
- `parser.c`: `term(source: String)` returns `Syntax.T`; `program(source:
  String)` returns `List<&2, Syntax.Decl>`, each wrapped in
  `Result<&2, &2, Error.T, ...>`. Both require complete input consumption.
- Expected scanner/parser failures use `Error.Parse` with source location.
- `syntax.c`: `show(T)` and `print(List<&2, Decl>)` print parseable syntax.
- `universe.c`: `lower(T)` returns `Result<&2, &2, Error.T, Level.T>`.
- `poly.c`: `map_levels(LevelAction, Term.T)` traverses every term field.
  `map_term(Action, Term.T)` returns `Budget.Comp(Term.T)` and also supports
  explicit name mappings. `LevelAction` supports preservation, scope validation,
  and simultaneous substitution. Traversal polling shares one threaded budget.
- `family_poly.c`: raw traversals, metadata, name planning, trusted
  declaration/group declaration, composition, reuse and specialization.
  Every trusted operation has a `Budget.Comp` core and a public Result wrapper.
- `elab.c`: `elab(ctx: Check.Ctx, expected: Maybe<Value.T>, syntax: Syntax.T)`
  returns `Result<Error.T, Term.T>` and covers every term constructor.
  `elab_decl(decl, ctx)`, `elab_fam_decl(family, ctx)` and
  `elab_ctor_decl(ctor, ctx, family_name)` return raw kernel declarations.
  `elab_term.c` contains the shared-budget term core. `elab.c` integrates
  ordinary, mutual, recursive and polymorphic declarations.
- `Elab.check_in(globals, budget, source)` and
  `Elab.elab_program_in(globals, budget, declarations)` return
  `Result<Error.T, Common.Pair<Global.T, Rows>>`, where
  `Rows = List<Common.Pair<String, Global.Entry>>`.
  `check_text` and `elab_program` return just the rows. `checked_form(rows)`
  prints checked kernel syntax; `disclosure_names(rows)` reports postulates
  and shape assumptions. One budget state spans each whole program.

Observed validation:

- Lexer, parser and polymorphic syntax behavior suites returned `True{}`.
- Template term traversal tests returned `True{}`, including late budget
  exhaustion and substitution beneath annotations, motives and branch bodies.
- Native parser differential replay: 261 cases, zero acceptance or diagnostic
  mismatches (Bend run, commit 8c90b64). Report: `build/bend2-parser/report.json`.
- Historical inline semantic comparison: 216 cases, zero acceptance,
  checked-form or diagnostic mismatches after fixing 20 parser diagnostics.
  Report of the Bend run (commit 8c90b64): `build/bend2-surface-check/extra-independent-report.json`.
- `c/tests/surface_diagnostics.c` executes malformed member and template location
  regressions, including declarations beginning on a later source line.
- `c/tests/surface_elab.c` executed successfully: dependent linear identity,
  large naturals, lets, dependent pair projections, unknown-name rejection and
  rejection of a discarded linear argument.
- `c/tests/surface_catalog.c` executed successfully: trusted member renaming,
  exports, reuse, forged-member rejection, hidden annotation scope, duplicate
  constructors, sequential callback visibility, first-error preservation and
  exhausted declaration/specialization budgets.
- `c/tests/surface_program.c` executed successfully: ordinary, recursive,
  family and polymorphic declarations; exports, composition, reuse and exact
  shared-budget boundaries. The boundary test found and verified the repair of
  a kernel probe that swallowed budget exhaustion.
- `c/tests/surface_trust.c` executed successfully: constructor parameter order,
  indexed reuse metadata, ambient global rechecking, mutual-member reuse
  refusal and exact group-versus-single-family diagnostic precedence.
- `c/tests/surface_boundary.c` executed successfully: all declaration-field scope
  placements, universality versus closed sampling, callback cancellation,
  nested composition and specialization budgets, retry and caller inventories.
- `c/tests/surface_universes.c` executed successfully: equality at Prop, data and
  level 4611686018427387903, singleton elimination, and Sum constructors at
  unequal universes in both directions, retaining binder quantities.
- `c/tests/surface_crypto.c` executed seven original zk/fhc/mpc positive fixtures.
- `c/tests/surface_shape_metadata.c` executed the original four hidden crypto
  metadata cases, checking scope rejection and complete name/level rewriting.
- `c/tests/surface_prelude.c` executed all four native prelude helper catalogs,
  including computed dependent projections, equality transport and casts,
  congruence and Sum elimination, and zero-budget refusal.
- Both official JavaScript emission and native C emission followed by Clang
  `-O1` have executed source drivers successfully. Earlier default native
  builds exited 137 and remain recorded as failed attempts.
- The frozen semantic corpus contains 926 unique observed inputs. A pinned
  pre-final driver matched 925 exactly; NatMul remained incomplete after a
  900-second timeout. Final-source replay is running after the shared arbitrary-
  precision collection-width migration. Earlier 120/300-second timeout
  observations remain in their original reports.
- Expected collection payload types are visited in requested-index order.
  Extra legs are ignored, a missing requested leg returns `Wrong_leg`, and a
  preceding closure error retains priority. Negative raw injection widths
  return `Mismatch` before inspecting the expected type. Fourteen raw helper
  and public-elaboration controls also require zero budget polls.
- All 19 original frontend protocols have individually passed across pinned
  builds. The prenex protocol exposed an 18-digit native-Nat overflow; after
  moving universe and Type payloads to Bignum, its exact original suite passed.
  `COVERAGE.md` distinguishes these observations from final-source validation
  and from the 102 mutation controls whose complete replay remains pending.
- `c/tests/prelude_runtime.c` exposes `arguments(args: List<String>) ->
  IO(Unit)` and a standalone `main`. The adapter preserves normal,
  `--reachable` and `--slice-self-test` modes, per-export TSV output and binary
  `.wasm` writes. `c/tests/runtime_slice.c` computes the full erased dependency
  closure and preserves type groups, postulates and row order.

Source mapping: each `.c` module corresponds to `surface/<same-name>.ml`.
`poly.c` also supplies the raw recursive term traversal used by
`family_poly.ml`; its transformation callbacks are represented by reusable data.
`elab_term.c` contains the recursive term portion of `surface/elab.ml`.
The four modules under `c/prelude/` map to their original `.ml` names.
See `VALIDATION.md` for the original test families and remaining integration
validation, which is separate from raw API coverage.

Ownership: this directory and `c/tests/surface*.c` belong to the surface
builder. CLI/import expansion belongs to the parent builder.
