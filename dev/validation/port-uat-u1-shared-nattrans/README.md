# Shared natural transformation validation

Validation for `dev/PORT-UAT-U1-SHARED-NATTRANS.md`, on base a168a09.
The build has zero errors and warnings. The restored kernel suite
checks 591 definitions, 11 families, four computations and six
refusals. The runtime gate compares four exports at payloads 37 and
41 on the kernel, Node and Wasmtime. All six mutation controls are
detected, with passing baseline and restored runs.

The full battery passes 62 of 63 gates. The sole failure is the
inherited TRUSTED-LINES bound: kernel=4208/3000 and encoder=246/900.
Both new gates pass. Existing predicates and watchdog tiers are
unchanged. The OCaml static audit reports no findings, both Python
files parse, shell syntax checks pass, and the diff has no whitespace
errors.

`checks.json` records commands, verdicts, tool versions, executable
hashes and source hashes. `sources.sha256` repeats the source hashes
in a format accepted by `shasum -a 256 -c` from the repository root.
`build`, `runtime` and `gates` stdout/stderr files retain the command
output. Absolute repository paths in those logs are replaced by ROOT.
`mutations.json` records all eight runs and their full-stream and
retained-capture hashes. Each mutation capture is prefixed with
`mutation-` in this directory. Streams above 4,000 bytes retain their
prefix and an omission marker; the replay driver retains full output
in its work directory. COPY in the replay command denotes the
isolated fixture tree. Reproduce the controls with:

```sh
python3 -I dev/shared-nattrans-mutations.py /tmp/shared-nattrans-replay
```

The replay directory must be new and outside the repository. The
driver copies only required fixtures, uses the current built suite,
and never edits the canonical sources. Shared left Kan APIs and
source-type parity remain open. No mapping verdict or denominator
changes in this increment.

The review of 2026-09-17 edited 11 of the 27 hashed sources after the captures were recorded; the last key of checks.json, review_delta, lists each path with its captured and final hash.
