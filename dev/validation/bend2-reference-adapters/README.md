# Bend 2 reference adapters from Git history

This receipt records the live reference check after the seven OCaml
observation adapters left the source tree. The runner reads each adapter from
Git commit `ed923e2130b8501ccbe500b538cfa3d39aa2bcda` with
`git --no-replace-objects cat-file blob`. It checks each adapter against its
recorded SHA-256 hash before the OCaml build starts.

The reference checkout was at `7a2f9f233b1b3864ca9cc0a6716aac7dd81bb77a`.
`run.sh` contains the commands. Each suite group ran in a fresh evidence
directory.

| Suite group | Cases | Differences | Result |
| --- | ---: | ---: | --- |
| JSON, export, translation | 166, 470, 613 | 0 | passed |
| Surface checker | 926 | 0 | passed |
| Erasure | 80 | 0 | passed |
| Import pipeline | 1,118 | 0 | passed |
| Parser | not replayed | not applicable | error |

Each passing report records `adapter_revision` and, under `inputs`, the hash
of each retrieved adapter. No input drift occurred.

The parser group stopped before replay with `stale corpus input:
prelude/cat/left-kan-laws.mech`. Commits `e6d3e38`, `f230f34` and `e6597c2`
changed this file after the reference commit. The parser corpus was stale
before this change, and the adapters are not the cause.
`parser-report.json` and `parser.stderr` contain the error.

`dev/bend2-reference-test.py` ran two times. The two new controls,
`adapter-history` and `adapter-digest`, passed in both runs. The first run
then failed at the existing `core-timeout` control: the child process did not
print its marker within the 0.2-second limit, with a load average above 50.
The second run passed all twelve controls. `reference-test-1.*` and
`reference-test-2.stdout` contain both outputs.

The CLI and runtime suites and the Bend 2 replay were not run again. They do
not use the adapters. To check the source pins, run this command from the
repository root:

```sh
shasum -a 256 -c dev/validation/bend2-reference-adapters/sources.sha256
```
