# Stage C: dependency member export clauses

Date: 2026-09-22. Base: a86e84b (top-level prenex exports).

## Contract

Dependencies inside a composed group accept the same optional `export`
clause as top-level family specializations. It follows any `with` clause
and chooses names local to the composed schema:

```text
poly (u) mu Box : Sort (succ u) with | box : Box
where
def witness : Box := box
def alias : Box := witness
end
poly (v) group Pair where
specialize Box (v) as Local export (alias := a, witness := w)
def pick : Local := a
end
specialize Pair (0) as Run
```

The closed instance contains `Run_Local`, `Run_w`, `Run_a` and `Run_pick`.
Its member rows retain the original order: `Run_w`, `Run_a`, `Run_pick`.
The names `Local_witness`, `Local_alias`, `w` and `a` do not escape the
symbolic group. `Run_Local_witness` and `Run_Local_alias` are not installed.
An outer export clause names `w`, `a` and `pick`, including the imported
members. Nested dependencies can rename these members again.

Omission preserves prefixed names. An explicit clause names every member
exactly once, with distinct fresh targets. Both `export ()` and
`export ( )` are accepted only when there are no members. Family names,
constructor labels, template names, dependency aliases, existing globals
and other imported or local members remain reserved. A composed dependency
alias stays reserved even when it is only a prefix for its families.

Reuse continues to bind family identities and does not hide member
definitions. The runtime fixture imports a composed group, renames its
members again, reuses both of its families in a later dependency, and
calls members across those reused identities.

The AST and `Family_poly.compose` dependency tuples now include an optional
member mapping after the reuse bindings. Composition uses the existing
mapping validator and simultaneous renaming traversal for member types and
bodies. It rechecks symbolic declarations under the group's universe scope
and rechecks closed instances. Mapping validation uses the caller's budget
before name planning and again during composition, and name planning
polls the budget once more before it reports a collision. An exhausted
budget therefore takes precedence over an export-name collision.
Failure returns no partial catalog or environment.

## Validation

`PRENEX-DEPENDENCY-EXPORTS` checks round-trips, no symbolic leakage, chosen
row names and declaration order, empty mappings, contextual `export`,
name swaps, complete mappings, collision diagnostics, clause ordering,
budget cancellation, nested reuse and empty axiom reports.

`PRENEX-DEPENDENCY-EXPORTS-RUNTIME` compares `dependencyPayload` and
`nestedPayload` on the kernel, Node and Wasmtime. Both must produce 37,
then 41 after the shared input is changed. Every variant must report no
axioms. Existing export, template composition and symbolic reuse suites
also run without relaxing their predicates.

```sh
zsh dev/dunecho.sh build
_build/default/test/prenex_dependency_exports.exe .
python3 -P test/prenex_runtime.py --dependency-exports
python3 -P dev/validation/prenex-dependency-exports/mutations.py NEW_WORK_DIRECTORY
zsh dev/gates.sh
```

The mutation runner copies the source tree into the new work directory
named on the command line (it refuses an existing path or one inside the
repository), copies each per-attempt test log into
`dev/validation/prenex-dependency-exports/` before it exits, and checks
six compiling mutations: permit
incomplete mappings, drop chosen reference names, drop alias reservation,
omit exports from name planning, drop dependency clauses in the printer,
and ignore the name-planning validation budget.
Each must fail at its designated regression; the restored tree
must pass. Results and source hashes accompany the validation record.

This slice does not change the kernel, vendor pin, axiom policy, gate
thresholds or watchdogs. Stage C and U1 remain open on whole-record
transformation equality and source-type parity.
