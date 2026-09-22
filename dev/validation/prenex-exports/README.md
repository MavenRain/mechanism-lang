# Textual member export validation

Base commit: `0b8661b066bc75d607ce280eea59e1a6ceaf371e`.
The validation checkout is `/Users/oobi/Documents/gpt6/mechanism-lang-20260922`.
Its staged patch is applied to `/Users/oobi/Documents/mechanism-lang`.

`sources.sha256` pins the implementation, test and gate files exercised by
these checks. Verify the manifest from the repository root with:

```sh
shasum -a 256 -c dev/validation/prenex-exports/sources.sha256
```

Hash equality establishes freshness, not correctness.

| Artifact | Check |
| --- | --- |
| `build.stdout` | Restored mutation checkout build, zero errors and warnings |
| `suite.stdout` | Surface suite after all mutants were restored |
| `runtime.stdout` | Two exports, three hosts, original and changed payloads |
| `mutations.json` | Seven compiled mutants, designated diagnostics and source hashes |
| `gates.stdout`, `gates.stderr` | Complete regression battery output |

Reproduce the focused checks from the repository root:

```sh
zsh dev/dunecho.sh build
_build/default/test/prenex_exports.exe .
python3 -P test/prenex_runtime.py --exports
python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY --exports
```

The mutation runner builds an isolated copy, checks its baseline, requires
each compiling mutant to fail with the designated diagnostic, restores the
source, then rebuilds and reruns the suite. It saves complete per-attempt
stdout and stderr under the supplied new work directory.

The full battery runs with `zsh dev/gates.sh`. Its final status and any
remaining gate failure are recorded in the dated textual member exports
entry in `dev/M0-BUILD-LOG.md`.
