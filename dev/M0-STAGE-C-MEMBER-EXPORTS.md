# M0 Stage C: chosen member export names

Date: 2026-09-22. Base: 510ff32.

`Family_poly.instantiate` accepts optional `~exports` mappings from template
member names to fresh global names. Omitting the option preserves the existing
instance-prefix convention. Supplying it requires every member exactly once,
with distinct targets. `as_name`, installed family names, reused family names
and the constructor labels of the template's families remain reserved.
Existing globals, families, constructor labels and catalog templates cannot be
replaced by exported members. The surface elaborator resolves a bare name
through the constructor tables before the globals, so a global under a
constructor label would be unreachable from surface text.

For example, the type-equality library can expose chosen operation names:

```ocaml
Family_poly.instantiate globals catalog
  ~name:"MechTypeEq" ~levels:[Level.zero] ~as_name:"NamedCast"
  ~exports:["refl", "libraryRefl"; "cast", "libraryCast";
            "symm", "librarySymm"; "trans", "libraryTrans"]
```

All references to those members are renamed in their types and bodies before
closed checking. Member declaration order is preserved regardless of mapping
order. `Family_poly.instance_names` accepts the same option and reports the
actual output names. It returns `None` for an invalid mapping, including a
target equal to a constructor label of the template's families; freshness
against the ambient environment, its constructor labels and the catalog is
checked during instantiation.

Export validation shares the caller's check budget. The immutable input
environment and catalog remain unchanged after any failure, including a late
budget failure while checking exported members. Family reuse and companion
families retain their existing checking rules. No kernel or gate rules change.

Validation adds 16 family-member cases, bringing that suite to 35 cases. They
cover dependent member renaming and computation, mapping order, missing and
duplicate sources, unknown members, target collisions with `as_name` (also
under reuse), families, companion families, constructor labels of the
instance and of an ambient family, ambient globals and templates, family
reuse, name planning, the export share of the check budget, and late budget
rollback. PRELUDE-TRANSPORT also
installs the four type-equality operations under custom names over a reused
family and requires a cast to compute through an indexed witness. That client
passes the reused family's own witness to an exported operation, and the gate
requires the reused run to install no family and no default member name.

Build: zero errors and zero warnings. FAMILY-MEMBERS passes all 35 cases;
PRELUDE-TRANSPORT passes its seven instances and five negative clients plus
the custom-name computation test. FAMILY-POLY, PRELUDE-POLY and
PRELUDE-EQUALITY-OPS, the other gates over this code path, also pass.
`dev/M0-BUILD-LOG.md` holds the printed lines under `## Stage C: chosen
member export names (2026-09-22)`, and `dev/MUTATION-LOG.md` records the
export mutants.

Full-battery status: incomplete. The run was intentionally stopped before
the remaining long category-law suites, after the gates covering this API
change completed. The build log section records this run as interrupted, not
green.
