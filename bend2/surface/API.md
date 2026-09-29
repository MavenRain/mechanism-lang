# Native surface API

The frontend is implemented natively in Bend 2.0.25. End-to-end semantic
validation is in progress.

- `token.bend`: `Loc{line: Nat, col: Nat}`, `KindT`, and `Tok{kind: KindT, loc: Loc}`.
  `NatTok` stores canonical arbitrary-length decimal text. The parser converts
  that text with `Bignum.of_decimal`; syntax naturals are `Bignum.T`.
- `lexer.bend`: `lex(source: String) -> Result<&2, &2, Error.T, List<&2, Token.T>>`.
  Token positions are one based and count UTF-8 bytes, matching the OCaml scanner.
- `syntax.bend`: location-free `T` and `Decl`, with generic `Binder<A>`,
  `Motive<A>`, `Field<A>` and `Branch<A>` to avoid mutual datatype definitions.
  Universe literals, `SType`, `SProj`, `SInj` and `BrLeg` numeric payloads use
  `Bignum.T`; their source parser retains the original 18-digit limit.
  Binder positions, source locations and actual list lengths use `Nat`.
- `parser.bend`: `term(source: String)` returns `Syntax.T`; `program(source:
  String)` returns `List<&2, Syntax.Decl>`, each wrapped in
  `Result<&2, &2, Error.T, ...>`. Both require complete input consumption.
- Expected scanner/parser failures use `Error.Parse` with source location.
- `syntax.bend`: `show(T)` and `print(List<&2, Decl>)` print parseable syntax.
- `universe.bend`: `lower(T)` returns `Result<&2, &2, Error.T, Level.T>`.
- `poly.bend`: `map_levels(LevelAction, Term.T)` traverses every term field.
  `map_term(Action, Term.T)` returns `Budget.Comp(Term.T)` and also supports
  explicit name mappings. `LevelAction` supports preservation, scope validation,
  and simultaneous substitution. Traversal polling shares one threaded budget.
- `family_poly.bend`: raw traversals, metadata, name planning, trusted
  declaration/group declaration, composition, reuse and specialization.
  Every trusted operation has a `Budget.Comp` core and a public Result wrapper.
- `elab.bend`: `elab(ctx: Check.Ctx, expected: Maybe<Value.T>, syntax: Syntax.T)`
  returns `Result<Error.T, Term.T>` and covers every term constructor.
  `elab_decl(decl, ctx)`, `elab_fam_decl(family, ctx)` and
  `elab_ctor_decl(ctor, ctx, family_name)` return raw kernel declarations.
  `elab_term.bend` contains the shared-budget term core. `elab.bend` integrates
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
  mismatches. Report: `build/bend2-parser/report.json`.
- Historical inline semantic comparison: 216 cases, zero acceptance,
  checked-form or diagnostic mismatches after fixing 20 parser diagnostics.
  Report: `build/bend2-surface-check/extra-independent-report.json`.
- `surface_diagnostics.bend` executes malformed member and template location
  regressions, including declarations beginning on a later source line.
- `surface_elab.bend` executed successfully: dependent linear identity,
  large naturals, lets, dependent pair projections, unknown-name rejection and
  rejection of a discarded linear argument.
- `surface_catalog.bend` executed successfully: trusted member renaming,
  exports, reuse, forged-member rejection, hidden annotation scope, duplicate
  constructors, sequential callback visibility, first-error preservation and
  exhausted declaration/specialization budgets.
- `surface_program.bend` executed successfully: ordinary, recursive,
  family and polymorphic declarations; exports, composition, reuse and exact
  shared-budget boundaries. The boundary test found and verified the repair of
  a kernel probe that swallowed budget exhaustion.
- `surface_trust.bend` executed successfully: constructor parameter order,
  indexed reuse metadata, ambient global rechecking, mutual-member reuse
  refusal and exact group-versus-single-family diagnostic precedence.
- `surface_boundary.bend` executed successfully: all declaration-field scope
  placements, universality versus closed sampling, callback cancellation,
  nested composition and specialization budgets, retry and caller inventories.
- `surface_universes.bend` executed successfully: equality at Prop, data and
  level 4611686018427387903, singleton elimination, and Sum constructors at
  unequal universes in both directions, retaining binder quantities.
- `surface_crypto.bend` executed seven original zk/fhc/mpc positive fixtures.
- `surface_shape_metadata.bend` executed the original four hidden crypto
  metadata cases, checking scope rejection and complete name/level rewriting.
- `surface_prelude.bend` executed all four native prelude helper catalogs,
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
- `tests/prelude_runtime.bend` exposes `arguments(args: List<String>) ->
  IO(Unit)` and a standalone `main`. The adapter preserves normal,
  `--reachable` and `--slice-self-test` modes, per-export TSV output and binary
  `.wasm` writes. `runtime_slice.bend` computes the full erased dependency
  closure and preserves type groups, postulates and row order.

Source mapping: each `.bend` module corresponds to `surface/<same-name>.ml`.
`poly.bend` also supplies the raw recursive term traversal used by
`family_poly.ml`; its transformation callbacks are represented by reusable data.
`elab_term.bend` contains the recursive term portion of `surface/elab.ml`.
The four modules under `bend2/prelude/` map to their original `.ml` names.
See `VALIDATION.md` for the original test families and remaining integration
validation, which is separate from raw API coverage.

Ownership: this directory and `bend2/tests/surface*.bend` belong to the surface
builder. CLI/import expansion belongs to the parent builder.
