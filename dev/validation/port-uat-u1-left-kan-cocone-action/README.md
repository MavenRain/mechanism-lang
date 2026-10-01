# Left Kan cocone action validation

Base: dff8f8355759cc4b05982f8a9d9326afbb7018af.

- `kernel.stdout`: three laws, three independent-universe contracts,
  eight computations and four refusals pass in 435.541 seconds.
- `runtime-corrected.stdout`: eight exports, two inputs and three hosts
  agree in 48 comparisons, in 187.971 seconds.
- `oracles-corrected.stdout`: four distinct mismatch diagnostics.
  The exact diagnostics are also stored with the refusal fixtures.
  Its final blank line is removed; `oracles-corrected.raw.json` preserves
  the complete original stdout and its hash.
- `selectors.stdout`: all 113 build-selector regressions pass.
- `runtime-reuse.json`: the native runtime artifact and all 118 input
  hashes match the current inputs.
- `equality-regression.stdout`: the existing equality mode reaches its
  unchanged 480-second emission allowance.
- `equality-source-identity.json`: that equality program is byte-identical
  to extraction from committed dff8f83.
- `focused-source.mech` and `focused-extraction.json`: the checked contracts
  and their source hashes. Definition bodies are retained verbatim.
- `checks.json`: final source and capture hashes.

The initial runtime attempt reached its emission limit. The first direct
congruence proof exposed an object-versus-hom universe mismatch; the final
proof uses two checked congruence steps and transitivity. Earlier failed
captures are retained, but they are not evidence for the passing checks:
`runtime.*` reached the emission limit, and `runtime-direct.*` and
`oracles-final.*` record the universe mismatch of the direct proof.
The corrected kernel and runtime checks pass with the existing deadlines.

The full battery and full left Kan suite were not rerun.
