# Heterogeneous whiskering validation

Base: c7e95c3f38f787bf97f75407748b2e541974aa12. Recorded on 2026-09-15.

## Results

- BUILD: PASS.
- PRELUDE-HETEROGENEOUS-WHISKERING: PASS.
  Contract: 274 entries, four specializations, four computations and 15 refusals.
- PRELUDE-HETEROGENEOUS-WHISKERING-RUNTIME: PASS.
  Four exports, two payloads, kernel plus Node and Wasmtime.
- CLI arity refusal: PASS, five supplied arguments where six are required.
- Mutation controls: 9 of 9 caught; restored suite
  PASS; replay passed=True.
- Full battery: 54 of 55 PASS, exit 1.

Failed battery rows:

- FAIL TRUSTED-LINES

TRUSTED-LINES has the inherited kernel bound of 3000 against 4208 lines;
the encoder is 246 against 900. This increment changes no trusted source
or bound. Timeouts, if present, remain failures in these results. Earlier
development probes encountered a load spike above 360; the final logs
and per-command durations, rather than that earlier load, define this record.

## Mutation controls

| Control | Result | Elapsed ms |
| --- | --- | ---: |
| right-naturality | caught | 33707 |
| left-naturality | caught | 18545 |
| left-composition-law | caught | 15399 |
| object-map | caught | 92157 |
| arrow-map | caught | 88238 |
| negative-corpus | caught | 72430 |
| canonical-mirror | caught | 18 |
| second-mirror | caught | 18 |
| composite-mirror | caught | 16 |

The replay operates on temporary source copies, restores each edit, checks
the restored source hashes and checks the original suite again. Timeouts
are recorded with a null exit code and timed_out=true. Distinct diagnostics
and all controls are required for replay passed=true.

## Evidence

- `gates.log`: full battery output and measurement rows. `gates.stderr`:
  empty.
- `arity.stderr`: exact CLI refusal. `arity.log`: empty, because the
  refusal goes to stderr.
- `mutations.json`: controls, expected diagnostics, restoration and source hashes.
- `checks.json`: commands, exit statuses, durations, executable and evidence hashes.
- `sources.sha256`: final prelude, test, fixture and gate source hashes.

The rows of `sources.sha256` match the sources as captured. Review
round 1 changed four of the hashed paths after the capture, so rows
5, 8, 9 and 10 differ from the final blobs. The rows stay as
recorded; the closer adds the `review_delta` key with the captured
and the final hashes. The keys
`final_sources` and `source_normalizations` of `mutations.json` are a
post-capture annotation: the replay script writes neither.
They record that two diagnostic-prefix files had trailing spaces removed
after execution. Their effective messages are identical because the suite
uses `String.trim`; `source_normalizations` records both byte hashes and
the unchanged effective-prefix hash. Original captured hashes are retained
in `checks.json.validated_sources_sha256` and `mutations.json.sources`.
The kernel suite
checks symbolic templates and their specializations in one elaboration.
It retains the existing 120-second SLOW watchdog. The runtime driver keeps
its existing 110-second total deadline and checks both payloads in one input.

## Reproduce

Run from the repository root:

```sh
zsh dev/dunecho.sh build
_build/default/test/prelude_heterogeneous_whiskering.exe .
python3 -I test/heterogeneous_whiskering_runtime.py
python3 -I dev/heterogeneous-whiskering-mutations.py /tmp/whiskering-replay.json
zsh dev/gates.sh
```

The replay output path must not exist. For the arity probe, concatenate
category-core.mech, composable-functors.mech and heterogeneous-whiskering.mech,
append `specialize MechHeterogeneousWhiskering (0, 1, 2, 3, 4) as Bad`, and
run `_build/default/bin/mech.exe check FILE` on the temporary file.
The expected stderr is in `arity.stderr`.

Review round 1 added a universe-arity control to the suite after this bundle
was captured, so the suite now reports 16 refusals where the rows above
record 15. The captured rows stay as recorded; the closer adds the
`review_delta` key with the captured and final hashes of the changed paths.
