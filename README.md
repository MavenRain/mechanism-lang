# mechanism-lang

mechanism-lang is a dependently typed language for mechanism design.  It
is a sibling of kanon, not a kanon milestone.  The kernel of kanon is
vendored, not forked: vendor/kanon is a git submodule pinned at
936a43a92dd59a04698648f24fa5ae94cdb532df, the sha in the PIN file.  The
submodule is never forked and never rebased.  mechanism-lang adds a
level overlay in lib/, an importer for the lean4export format in
import/, and a prelude in prelude/ that states its mapping targets in
map/prelude.map.tsv.

The driver is `mech` and the source extension is `.mech`.

## The M0 gate

PRELUDE-CHECKED is the M0 gate.  It prints two lines:

```
PRELUDE-CHECKED external=2477 NAME_AND_TYPE=n1 NAME_ONLY=n2 UNMAPPED=n3 NEVER=n4
PRELUDE-CHECKED covered=(n1+n4)/2477 = P percent  NEVER=n4 of 2477
```

The gate fails when n2 or n3 is above zero.  Every percentage prints the
NEVER count beside it, so a covered figure is always read with the count
of the names that mechanism-lang refuses to map.

## The gate battery

`zsh dev/gates.sh` runs the battery.  Each leg prints one PASS or FAIL
line under a watchdog tier, then the script prints the MEASURE block and
GATES-OK or GATES-FAIL.  A tier is a hang ceiling and never a
performance budget.  Stage A adds universe-level tests and runs the
inherited suites against the recompiled overlay.  R0 counts, active
trusted sources and the pin delta are checked by the same battery.
The import foundation adds grammar, corpus, count and CLI gates.
The prelude mapping gate remains due in a later M0 stage.

The implementation passes its behavioral suites.  Acceptance remains
pending the trusted-kernel limit ruling: the active kernel exceeds the
unchanged 3,000-line bound, so TRUSTED-LINES fails.
See `dev/M0-BUILD-LOG.md` for the validation record.

The driver inherits check, axioms, emit, run and spec-count.
Prenex templates are available through the OCaml Poly API.  The importer
retains their universe arguments for checked resolver integration.
A template is checked universally and every explicit closed specialization
is rechecked.

## Importing Lean types

Build the driver, then run:

```sh
_build/default/bin/mech.exe import path/to/uat.export --out imported-types
_build/default/bin/mech.exe diff-parity --export path/to/uat.export
```

The reader accepts lean4export format 3.1.0 and checks every record and
table reference.  It retains all declaration types, including constructors
and recursors.  The output directory must be new.  It contains a manifest,
a shared type table in types.ndjson and a summary.  Shared nodes retain
universe arguments and binder scope without expanding repeated subterms.

The report distinguishes a scoped source type from a kernel-checked type.
KERNEL_TYPE means the type lowered and checked with the available globals.
DEFERRED means a checked prelude mapping is still needed.  UNSUPPORTED and
KERNEL_ERROR retain their declaration and reason.  No status claims
NAME_AND_TYPE parity, and no NEVER ledger is applied yet.

This delivers the Stage B import foundation.  The full planned Stage B
kernel-type table remains incomplete: imported constants need checked
prelude mappings, and projections need their checked representation.
The library exposes Translate.lower_type with an explicit resolver for
that integration.  Values remain scheduled for M1.

## Checked prelude foundation

`prelude/init.mech` declares the data foundation and its recursors.
The PRELUDE and AXIOMS gates check it from an empty kernel environment,
so it cannot silently use the driver's initial Nat axiom.  Parameterized
constructors use their expected family type to determine parameters.

The external-name inventory is reproducible:

```sh
_build/default/bin/mech.exe map-inventory --export path/to/uat.export
_build/default/bin/mech.exe map-inventory --export path/to/uat.export --never
```

These commands print `map/prelude.map.tsv` and `map/NEVER.tsv` respectively
when given the frozen UAT export.  NAME_ONLY records a candidate target;
Stage D must still compare its type with the source type.  Unavailable
features remain UNMAPPED unless an exact ratified NEVER exception applies.

`MechEq` now supports equality over a carrier at `Type 0`, dependent J,
transport, symmetry, transitivity and congruence without postulates.
The checker permits erased data indices in Prop families while preserving
its constructor-field and large-elimination restrictions.  The runtime
gate compares transport on the kernel, Node and Wasmtime hosts.

Stage C remains open.  Universe-polymorphic families, general type cast,
category targets and checked source-type parity remain outstanding.
The equality mapping candidates are NAME_ONLY at their stated universes.
See `dev/M0-STAGE-C-EQUALITY.md` for the equality change and its limits.
See `dev/M0-STAGE-C.md` for the remaining work and validation contract.

`zsh dev/dunecho.sh build` is the only way a dune verb runs in this
repository.  The runner puts the zxcaml-p1 opam switch first on PATH and
it takes the root from its own path.

## Licence

MIT OR Apache-2.0.  See LICENSE-MIT and LICENSE-APACHE.
