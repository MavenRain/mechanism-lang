# PIN delta

Every mechanism-lang OCaml implementation or interface file of lib/,
wasm/, surface/ and bin/ that overlays a file of the vendored kanon
tree at PIN has one row here.  Those are the files dev/pin-delta.sh
reads (dev/pin-delta.sh:50-53).  The build files bin/dune, lib/dune,
surface/dune and wasm/dune also replace their counterparts at the pin,
each one wholesale, so none of them carries a delta row.
dev/pin-delta.sh diffs each file
against `git -C vendor/kanon show 936a43a92dd59a04698648f24fa5ae94cdb532df:PATH`
and compares the changed line count with the expected column.  The count
is the line count of the `diff` output.  The overlay scope is lib/,
wasm/, surface/ and bin/ (S0-D6).

Stage A overlays the level representation and interface, the universe
rule, the checker's prenex scope boundary, budgeted conversion, and
driver initialization.
Unchanged modules are copied by Dune into the mechanism libraries.
Namespace shims and new level helpers have no counterpart at the pin.

Stage C overlays the surface elaborator to use expected family parameters
when constructing parameterized families.  The constructor record itself
comes from the family the expected type names, as the kernel's
introduction rule reads it, so a constructor name that two families
declare resolves inside the expected family.  Constructor fields are
elaborated under their dependent types, and result indices are computed
from those fields.

The Stage C equality increment also overlays the index check: Prop
families permit erased data indices at any well-formed universe.  Data
families retain their index bound.  Constructor fields, large elimination,
proof irrelevance and erasure use their existing rules.

The Stage C family-template increment factors declaration and constructor
checking through contexts with a prenex universe arity.  The ordinary
entry points retain arity zero, and the universal family judgment discards
its temporary environment.  Empty families and constructors poll their
check budget.  Family formation and elimination rules are unchanged.

The textual-prenex increment overlays the lexer, tokens, syntax and parser
for Sort levels, definition binders and explicit specialization.  The
elaborator carries a Poly catalog for one program check.  It uses the
existing universal and closed judgments.  No kernel file changes here.

The textual-family increment adds a single-family template syntax node.
The source catalog retains Family_poly templates, elaborates constructors
under a temporary symbolic family and specializes through the existing
closed family checker. Cross-catalog names and specialized constructor
names are checked before the immutable program result is returned.

| file | expected |
| --- | --- |
| lib/check.ml | 140 |
| lib/conv.ml | 51 |
| lib/rules.ml | 47 |
| lib/level.ml | 49 |
| lib/level.mli | 17 |
| bin/kanon.ml | 14 |
| surface/elab.ml | 324 |
| surface/token.ml | 10 |
| surface/lexer.ml | 5 |
| surface/syntax.ml | 49 |
| surface/parser.ml | 151 |

The textual group increment adds ordered family elaboration, member
checking and complete instance reservation to the surface overlay.  The
parser reads `and` companions and a `where ... end` member block under
one universe scope.  The printer preserves both.  The review round of
2026-09-10 completes the label check of an instance against the caller
globals, names the refusal of a recursive member and drops one repeated
family check.  No kernel, encoder or vendor source changes in this
increment.
