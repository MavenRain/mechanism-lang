# Left Kan postcomposition validation

Base: 6a275ba. The contract and reproduction commands are in
`dev/PORT-UAT-U1-LEFT-KAN-LAWS.md`.

| Check | Result |
| --- | --- |
| Build | Zero errors, zero warnings |
| Kernel | 963 entries, nine families, ten computations, ten refusals |
| Runtime | Ten exports, three hosts, two payloads, 60 comparisons |
| Mutation controls | Nine killed; control and restored sources pass |
| PIN and PIN-DELTA | Pass |
| TRUSTED-LINES | Inherited failure: kernel=5475/3000, encoder=246/900 |

The kernel suite also checks symbolic elaboration, parse/print stability,
builtin preservation, absence of axioms, checked family reuse and all
three law statements at independent universe levels. Runtime oracles
distinguish both composition orders and both mediator fields.

`checks.json` records the exact commands, exits, working copy and log
hashes. `kernel.stderr.json` preserves the exact diagnostic text, including
spaces at the ends of truncated trace lines. Its decoded text hash is
recorded separately from the JSON file hash. Other streams are plain text.
`sources.sha256` pins the final source inputs. `mutations/`
contains every final attempt and its aggregate result. The full battery
was not rerun for this additive prelude slice.

`attempts/` retains the first suite's duplicate-refusal-pin failure and
the interrupted first mutation run. The conflicting negative example
was changed to use IdentityChosen, then its diagnostic was regenerated.
The final kernel and mutation runs use that corrected fixture.
