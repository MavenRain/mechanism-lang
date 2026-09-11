# M0 Stage C: textual ordered groups and members

Date: 2026-09-10.  Base: d4dc0c4.  This increment connects the checked
ordered family and member API to source syntax.  It continues Stage C;
category targets, checked source-type parity and the D-A-1 bound ruling
remain open.

## Source contract

```text
poly (u) mu First : Sort (succ u) with | first : First
and Second : Sort (succ u) with | second : First -> Second
where
def witness : Second := second first
def alias : Second := witness
end
specialize First (0) as Data
```

The first family names the template.  The example installs `Data`,
`Data_Second`, `Data_witness` and `Data_alias`.  The `where` block is
optional, but requires at least one definition when present.  It shares
the family's universe binders.  `where` is a reserved word.  Single
families retain their existing syntax and can also have member blocks.

Families check in order and can use earlier families.  Members check
after all families and can use earlier members.  Forward references,
mutual recursion, recursive members, postulates and nested templates are
refused.  A recursive member reports that a nonrecursive member
definition is expected.  An explicit `poly (...) mutual ... end`
remains unsupported.  Ordinary declarations after a template remain
outside its scope.

The source elaborator checks symbolic families and members, then submits
the group to `Family_poly.declare_group`.  No symbolic global escapes.
Specialization substitutes closed levels, renames the entire group and
rechecks it.  Member definitions join the checked output rows in source
order.  All phases share the caller's budget and return no partial
environment on failure.

Generated names are checked against both source catalogs and existing
global, family and constructor names.  Labels from all installed families
are checked against both catalogs, every generated name and the
definition and family names of the caller globals.  Labels can repeat
across families and across instances of one template, and resolve by the
expected family.  A symbolic member cannot reuse a constructor label.
The companion-name accessor exposes catalog metadata for these checks;
it changes no checking rule.

## Validation scope

PRENEX-GROUPS checks an empty-environment fixture with 13 installed
families, 23 definitions and seven exact normal forms.  It exercises
independent level arguments, data, types and proofs, member calls,
single-family members and groups without members.  Exact inventories
check row order and symbolic isolation.  All entries are definitions and
the axiom list is empty.  Additional clients cover repeated labels,
Prop-valued groups and an ordinary definition after a group.

The 39 refusals check forward references, recursion, malformed member
blocks, scope, arity, invalid types and result universes, catalog lifetime,
name collisions, the declaration budget, the specialization budget and
the single-declaration API boundary.  Each semantic refusal compares the
complete error value.  Parser refusals compare the error constructor and
complete message, without depending on source coordinates.

PRENEX-GROUPS-RUNTIME compares member calls on the kernel, Node and
Wasmtime.  The payload changes from 37 to 41, while another member call
stays 12.  Each variant checks and has empty axiom output before emission.

`python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY --groups` builds
thirteen isolated controls.  They reverse family and member order,
reverse output rows, remove generated-name and companion-label checks,
remove the label check against the caller globals and the group-name
check against existing constructor labels, remove source catalog
reservation, bypass both budgets and change a payload.
Each mutant must build without warnings and fail its designated suite
check.  The seven definition controls and eleven family controls remain
available.  The older ordered-group refusal is updated to the explicit
mutual syntax, because ordered groups are now supported.

The pin, kernel, encoder, import denominators, watchdog tiers and trusted
bounds are unchanged.  `dev/PIN-DELTA.md` records the measured surface
overlay counts.  Validation evidence is in
`dev/validation/stage-c-prenex-groups/`.

Final result: the build has no errors or warnings.  All 31 mutation
controls pass (13 group, 11 family, seven definition).  The full battery
has 33 passing legs and one inherited failure:
`TRUSTED-LINES kernel=4182/3000 encoder=246/900`.  The clean base has the
same failure, with 31 passing legs.  No bound or watchdog tier changed.
