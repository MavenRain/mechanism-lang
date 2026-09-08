# Stage C mapping inventory

`prelude.map.tsv` inventories every distinct referenced external constant in
the frozen UAT export.  The columns are `lean_name`, `target`, and `verdict`.
The `-` sentinel means that no target is supplied.  Rows are unique and
sorted by the export reader's exact display name.  The in-house namespaces
are UnifiedAggregation, ArrowCat, and CompCatTheory, with exact namespace
boundaries.  Their private declarations remain external under D-M0-4.

A target is a candidate with the verdict NAME_ONLY.  UNMAPPED records a
missing target.  Stage C does not claim NAME_AND_TYPE.  Stage D must load
the checked prelude, resolve the exact universe instantiation, translate
the source type, and recompute parity.  Editing a TSV verdict supplies no
evidence that a target has the corresponding type.  The foundation
candidates name declarations in `prelude/init.mech`; polymorphic source
declarations still need universally checked targets before parity can pass.
The prelude case `map-name-only-targets` of test/prelude.ml requires every
NAME_ONLY target of `prelude.map.tsv` to name a checked prelude family, a
checked definition or a constructor of a checked family.

`NEVER.tsv` has the columns `lean_name`, `ratification`, and `milestone`.
Its current rows are exactly the referenced constants in the Lean.Omega
namespace, covered by R-V4's omega reflection exception.  They also remain
in `prelude.map.tsv`, with verdict NEVER, and in the denominator.  Private
omega helpers, other Lean namespaces, Array operations, and String names
receive no inferred exception.  Further ratified categories need exact
per-constant classification before rows are added.

The checked-in files use the UAT export at
`corpus/lean-parity/uat/uat.export` in the kanon-m2-corpus checkout, SHA-256
`f4439dce6a0b488e9bc328592e53c47867c4d19fb123b31358aeb35ed5d14354`.
It contains 2,477 distinct referenced external constants, 2,543 declared
external constants, and 3,017 total distinct referenced constants.
The inventory currently has 12 NAME_ONLY candidates, 2,280 UNMAPPED rows,
and 185 NEVER rows.  It does not satisfy the PRELUDE-CHECKED exit gate.
Declared but unreferenced constants do not become map rows.  Referenced
constants in values do become rows even though value translation is M1.

`mech map-inventory --export FILE` prints a fresh inventory from the
validated export reader.  The Mapping API accepts explicit candidate
targets and rejects duplicate source names, names outside the inventory,
invalid TSV cells, and targets that conflict with NEVER.  It neither
installs globals nor interprets the checked-in verdicts as parity results.
