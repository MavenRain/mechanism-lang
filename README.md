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
The importer and prelude mapping gates arrive in later M0 stages.

Stage A's implementation passes its behavioral suites.  Acceptance is
pending the trusted-kernel limit ruling: the active kernel has 4,140
lines against the unchanged 3,000-line bound, so TRUSTED-LINES fails.
See `dev/M0-BUILD-LOG.md` for the validation record.

The Stage A driver inherits check, axioms, emit, run and spec-count.
Prenex templates are available through the OCaml Poly API; import syntax
for them arrives with the lean4export reader.  A template is checked
universally and every explicit closed specialization is rechecked.

`zsh dev/dunecho.sh build` is the only way a dune verb runs in this
repository.  The runner puts the zxcaml-p1 opam switch first on PATH and
it takes the root from its own path.

## Licence

MIT OR Apache-2.0.  See LICENSE-MIT and LICENSE-APACHE.
