# port-uat-u1-nattrans-units record

This directory holds the captured runs of the nattrans-units slice. Replay every command from the repository root at the commit named in `record.json` (`base`).

## Files

- `record.json`: the command battery. Each entry holds `name`, `argv`, `cwd`, `exit_code`, `signal`, `timed_out`, `spawn_error`, the stdout and stderr streams, and `capture_sha256`.
- `attempts.json`: earlier runs of the same battery with the same entry shape. Their streams are in `attempts/`.
- `<name>.stdout`, `<name>.stderr`: the captured streams of the command `<name>`.
- `sources.sha256`: sha256 rows for the sources that the two legs read, the six slice documents, and the three build products.
- `right-reflexivity.py`: the replay of the two reflexivity refusal controls.

## capture_sha256

`capture_sha256` is the hex sha256 of the bytes of `<name>.stdout` followed by the bytes of `<name>.stderr`, with no separator. Check one entry with:

    cat kernel.stdout kernel.stderr | shasum -a 256

## kernel and kernel-trace

The `kernel` entry runs the gate command of `dev/gates.sh` row 298: the suite executable with the repository root and no other argument. The `kernel-trace` entry runs the same executable with `--trace`. The `--trace` flag adds TRACE rows on stderr only (one row per negative with its refusal digest); stdout is the same row. The gate never passes `--trace`.

## scoped-build exit 124

`attempts.json` records `scoped-build` with `exit_code` 124 and `timed_out` false. That exit code is the cmdliner usage error of `dev/dunecho.sh` (`dunecho: too many arguments` in `attempts/scoped-build.stderr`). It is not a timeout. The `build` entry in `record.json` is the run that passed.

## Replay of the reflexivity controls

    python3 -I dev/validation/port-uat-u1-nattrans-units/right-reflexivity.py
    python3 -I dev/validation/port-uat-u1-nattrans-units/right-reflexivity.py left

Each command prints JSON with a `digest` field. The value must equal the row in `test/neg/nattrans-units/<side>-reflexivity.digest`.

## sources.sha256

    shasum -a 256 -c dev/validation/port-uat-u1-nattrans-units/sources.sha256

Every row reads OK after `zsh dev/dunecho.sh build`. The three `_build/` rows depend on the compiler and the tree; the source rows do not.
