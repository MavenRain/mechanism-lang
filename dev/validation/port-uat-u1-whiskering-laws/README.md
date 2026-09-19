# Pointwise whiskering preservation validation

Base: `c7b99fe06ca80da954dad15f6f1c8f1a651022d9`. Date: 2026-09-18.

Both new gates passed under the existing 300-second SUITE tier. No existing
gate or tier was weakened. The broader battery completed 48 of 72 checks:
46 passed and two unchanged tests timed out. It was stopped during the
next check after the host load averages rose to 134.32, 80.59 and 51.45.
The captured command has exit 143; its managed supervisor has exit 137.
The two timeouts and the remaining 24 checks need a fresh run when the
host is less loaded. A full regression pass is not claimed.

The timed-out checks were PRELUDE-HETEROGENEOUS-LEFT-KAN (300 seconds)
and PRELUDE-COMPOSABLE-FUNCTORS (120 seconds). Their implementations and
input libraries are unchanged from the base. These timeouts are recorded
separately from the established TRUSTED-LINES failure, reproduced by a
standalone check: kernel=5475/3000, encoder=246/900. The standalone
denominator check passed.

- PRELUDE-WHISKERING-LAWS: 206.504 seconds, exit 0.
- PRELUDE-WHISKERING-LAWS-RUNTIME: 157.205 seconds, exit 0.

The kernel suite checked 1,177 definitions, twelve families, 24 computations
and six refusals. The runtime suite compared twelve exports at two payloads
on the kernel, Node and Wasmtime. The runtime limits are 290 seconds total,
240 seconds for check/evaluation/emission and 30 seconds per host command.

The mutation replay killed all seven controls. It checks the symbolic source
template, with baseline and restored runs and exact full-stderr SHA256
oracles. It does not replace the concrete computation or runtime tests.
The initial diagnostic capture calibrated these seven oracles; the recorded
final replay reran them after pinning. Refusal fixtures separately use
diagnostic prefixes, with the complete nominal-family diagnostic retained.
The capture in `mutations.json` covers the six author controls; the review
ladder reruns the replay with seven.

Files:

- `checks.json`: commands, exits, partial battery measurements and executable hashes.
- `gates.stdout`, `gates.stderr` and `measurements.txt`: the interrupted battery capture.
- `kernel.stdout` and `runtime.stdout`: direct checks before the battery.
  Their companions `kernel.stderr` and `runtime.stderr` hold no rows.
- `mutations.json`, `mutations.stdout` and `mutations.stderr`: final replay.
- `mutation-diagnostics.json`: seven distinct exact error hashes.
- `sources.sha256`: changed source/documents and runtime dependencies.
- `trusted-lines.stdout` and `denominators.stdout`: independent repository checks.
  Their companions `trusted-lines.stderr` and `denominators.stderr` hold no rows.

The staged `gates.stdout` omits the trailing blank separator line. The
unaltered raw capture remains at `/Users/oobi/Documents/mechanism-lang/.kanon-exec/run-Hotcq1`.

Reproduce from the repository root after `zsh dev/dunecho.sh build`:

```sh
_build/default/test/prelude_whiskering_laws.exe .
python3 -I test/whiskering_laws_runtime.py
python3 -I dev/whiskering-laws-mutations.py /tmp/fresh-whiskering-law-mutations
zsh dev/gates.sh
shasum -a 256 -c dev/validation/port-uat-u1-whiskering-laws/sources.sha256
git diff --cached --check
```

The replay directory must be new and outside the repository. The full
battery command still needs completion; it was interrupted in this record.
The other listed commands passed. TRUSTED-LINES separately retains its
inherited nonzero result.
The complete final mutation diagnostics remain in
`/Users/oobi/Documents/gpt12/whiskering-mutation-final`; replay recreates them in a fresh directory.
