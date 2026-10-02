# Typed compatibility pilot evidence

The report records 19 existing candidates and four U1 record signatures.
It checks the frozen UAT type graph against the current checked prelude.
See `dev/M0-STAGE-C-COMPATIBILITY-PILOT.md` for the precise contract.

`report.json` records verdicts, blockers and input hashes.  The checked
fixtures and their stdout and stderr are retained beside it.  The gate
streams record the pilot run and all 13 adversarial controls.  The native
build receipt records preparation of the production CLI.  It keeps the
author's working paths and records no binary hash.  The report records
the hashes of `mech.exe` and `mechanism-native`.  `trusted-lines.stdout`
is the output of `zsh dev/trusted-lines.sh .` from the repository root.

The complete derived type graph stays in the gate's retained evidence
directory.  Its hash is in the report.  This directory keeps the import
manifest and summary, without duplicating the corpus-sized type graph.
Recreate the graph and fixtures with `make prelude-compatibility-test`.

These witnesses establish scoped type signatures.  Constructor and
recursor representations, whole-record equality and full M0 coverage
remain open.  The inventory and denominator files have no changes.
