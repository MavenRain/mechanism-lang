# PIN delta

Every mechanism-lang OCaml implementation or interface file of lib/,
wasm/, surface/ and bin/ that overlays a file of the vendored Veil
tree at PIN has one row here.  Those are the files dev/pin-delta.sh
reads (dev/pin-delta.sh:50-53).  The build files bin/dune, lib/dune,
surface/dune and wasm/dune also replace their counterparts at the pin,
each one wholesale, so none of them carries a delta row.
dev/pin-delta.sh diffs each file
against `git -C vendor/veil show a7534cedeac82d396de8e23058ee6bc990560f65:PATH`
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

The Veil migration rebases the overlays onto its checker, rules, parser,
elaborator, driver and emitter. Mechanism retains its universe and
family machinery, closure fixes and prelude behavior. Circuit checking
is copied from the pin. The family traversal also maps Veil's shape
payloads during universe substitution and family renaming.

| file | expected |
| --- | --- |
| lib/check.ml | 140 |
| lib/conv.ml | 51 |
| lib/eval.ml | 24 |
| lib/rules.ml | 84 |
| lib/level.ml | 49 |
| lib/level.mli | 17 |
| bin/kanon.ml | 14 |
| surface/elab.ml | 385 |
| surface/token.ml | 12 |
| surface/lexer.ml | 6 |
| surface/syntax.ml | 74 |
| surface/parser.ml | 243 |
| wasm/emit.ml | 27 |
| wasm/link.ml | 61 |

The textual group increment adds ordered family elaboration, member
checking and complete instance reservation to the surface overlay.  The
parser reads `and` companions and a `where ... end` member block under
one universe scope.  The printer preserves both.  The review round of
2026-09-10 completes the label check of an instance against the caller
globals, names the refusal of a recursive member and drops one repeated
family check.  No kernel, encoder or vendor source changes in this
increment.

The category increment reifies frozen elimination closures in an evaluator
overlay, preserves a typed first projection in dependent projection motives,
and compares constructor indices at their telescope types.  The corresponding
pair eta motive uses the same typed projection.  No new kernel form is added.

The dependent-closure increment overlays WASM linking and emission.
Indirect calls use the arity stored in the closure.  Abstract nullary
closures dispatch even when every source argument erases.  Direct global
calls keep their declared signatures.  The kernel, erasure, encoder and
vendored tree keep their existing sources.

The template checking increment lets the family catalog elaborate and
check each member in one ordered pass.  The surface supplies raw member
syntax through callbacks.  Scope validation and the kernel judgment run
before a member becomes visible to later callbacks.  Closed instances
still recheck every member.  The surface overlay loses four delta lines.

Template composition adds a named source group with explicit template
dependencies at symbolic universe arguments.  The surface checks the
imported names and preserves their nested prefixes at specialization.
The family catalog reuses its existing raw traversal and ordered
checker.  Kernel, erasure, encoder and vendor sources keep their bytes.

The checked-family reuse increment adds optional closed family bindings to
specialization syntax and routes them through Family_poly. Existing
families are kernel rechecked in a temporary table, compared exactly, then
preserved in the caller. Family_poly is a mechanism-only helper and has no
vendored counterpart. Kernel, encoder and vendor sources are unchanged.

Symbolic family reuse adds bindings to group dependencies, preserves them
in printed source and omits reused family aliases from name reservation.
The family catalog checks certificates under the group's universe scope
and retains only fresh families in the resulting schema. The syntax and
parser counts above include this increment; the elaborator count stays 378.
