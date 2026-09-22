# M0 Stage C: textual member exports

Date: 2026-09-22. Base: 0b8661b.

The chosen member names API is now available in `.mech` programs.
This increment changes the surface AST, parser and elaborator. The kernel,
family catalog API, erasure and Wasm encoder retain their existing rules.

## Stage C textual member exports

A top-level family specialization accepts an optional export mapping after
its optional family reuse clause:

```text
specialize TEMPLATE (LEVELS) as PREFIX
  with (LOCAL := EXISTING)
  export (MEMBER := GLOBAL, MEMBER2 := GLOBAL2)
```

Omitting `export` preserves prefix-based member names. An explicit mapping
must name every member exactly once, with distinct fresh targets. Family
names and constructor labels retain their existing names. Targets cannot
collide with the instance prefix, installed or reused families, constructor
labels, existing globals, or either template catalog. Member types and
bodies are renamed together, including references to earlier members.
Output rows retain declaration order, independently of mapping order.

`export ()` and `export ( )` are explicit empty mappings. They are accepted
only for a family template with no members. They never request default
member names. The AST retains this distinction and the printer preserves it.
`export` is contextual after the specialization options and remains an
ordinary identifier elsewhere.

Export validation and closed checking share the caller's budget. The
elaborator validates through `Family_poly.instantiate` before planning
output names, preserving the API's specific mapping errors. It returns
the immutable environment only after checking both catalogs and all
generated names. A failed check publishes no partial specialization.

Definition templates reject export clauses. Unknown templates keep their
unbound-name error. Export clauses on dependencies inside a `poly group`
remain outside this increment. Top-level specialization of such a group
can export its members with the same syntax. The member set of a
composed group includes the definitions imported by its dependencies
under their LOCAL_member names, next to the group's own members. An
explicit mapping names every one of them; a mapping that names only the
group's own members is refused.

## Validation

`PRENEX-EXPORTS` checks parse/print round-trips, member declaration order,
fresh and reused families, computations, absent default names, name swaps,
explicit empty mappings, precise refusals and check-budget propagation.
`PRENEX-EXPORTS-RUNTIME` compares two computations on the kernel, Node and
Wasmtime, at payloads 37 and 41, and requires empty axiom reports.

The runtime fixture passes a value produced by the original family
instance to a newly exported operation over reused families. Its other
computation uses an independently installed family group.

Run the isolated source mutations with:

```sh
python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY --exports
```

The seven controls cover empty-mapping preservation, printing, export
forwarding, definition-catalog reservations, family reuse in name planning,
specialization budgets and definition-template refusals. Baseline and
restored builds must pass with zero warnings. Each mutant must compile
and produce its designated failure.

Measured results and capture paths are recorded in `dev/M0-BUILD-LOG.md`
and `dev/MUTATION-LOG.md`.
