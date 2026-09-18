# Natural transformation law validation

Captured on 2026-09-18 against base
59f48f11593c8524efbd9e9051f3fbc6767a392b.

This is scoped validation of the additive pointwise law template,
its clients and its two new gate entries. The full battery was not
rerun. TRUSTED-LINES was rechecked and fails at kernel=5475/3000 and
encoder=246/900, the committed Veil baseline. No limit or watchdog
tier changed. These results do not claim a passing full battery or
source-type parity.

- `checks.json` records commands, captured outputs, exit statuses,
  executable hashes, and the two gate-oracle comparisons.
- `mutations.json` is the complete seven-control replay report.
- `mutation-output.json` retains its baseline, control and restored
  stdout/stderr strings; their hashes match the report's rows.
- `initial-mutation-attempt.json` records the preliminary run that
  stopped after two controls on an incorrectly escaped newline anchor.
- `sources.sha256` pins the final relevant source and documentation
  files. Run `shasum -c dev/validation/port-uat-u1-nattrans-laws/sources.sha256`
  from the repository root.

To reproduce after `zsh dev/dunecho.sh build`:

```sh
_build/default/test/prelude_nattrans_laws.exe .
python3 -I test/nattrans_laws_runtime.py
python3 -I dev/nattrans-laws-mutations.py /tmp/new-nattrans-law-mutations
_build/default/test/prelude_heterogeneous_nattrans.exe .
_build/default/test/prelude_shared_nattrans.exe .
zsh dev/trusted-lines.sh
```

The mutation work directory must be fresh and outside the repository.
The final command retains its expected nonzero exit from the inherited
trusted-source limit failure. All other listed checks passed in this
capture. The source controls use fixture copies; canonical sources
and their checked hashes remain unchanged.
