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
from those fields.  The kernel and its declaration rules are unchanged.

| file | expected |
| --- | --- |
| lib/check.ml | 66 |
| lib/conv.ml | 51 |
| lib/rules.ml | 47 |
| lib/level.ml | 49 |
| lib/level.mli | 17 |
| bin/kanon.ml | 14 |
| surface/elab.ml | 116 |
