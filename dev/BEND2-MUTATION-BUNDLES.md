# Explicit native relational bundles

Fresh compilation remains the default. After a core source freeze, build a bounded shard with `python3 dev/bend2-mutation-build.py --build-bundle INDEX` (indices 0 through 4). The command pins the complete production `source_graph`, including Base and foreign C/JS imports, before and after generation and native compilation. It also pins Bend and Clang binaries before and after the build.

For a fixture-only gate, set `BEND_MUTATION_BACKEND=native` and `BEND_MUTATION_BUNDLE` to that shard's emitted `.bundle.json` manifest. Only the manifest's whitelisted relational modes can use it. Installation copies and revalidates the executable. Every invocation verifies the complete source graph, compiler identities, manifest, and executable before execution and verifies them again afterward.

A changed source graph forces fresh compilation during gate setup. A stale artifact, compiler identity, unsupported mode, incomplete provenance, or mutation after installation refuses execution. Compiler failures never count as mutation kills. CLI and shape source mutants retain fresh compilation.

The earlier full18 and small native diagnostic binaries are not authorized bundles: their complete source or compiler pre-build pins were missing. Post-build observations remain useful diagnostic evidence and are labeled as such; they are not promoted into pre-build proof.

`python3 bend2/tests/relational_bundle_guards.py` exercises twelve controlled guard cases, including changed foreign code, artifact corruption, mode refusal, post-only metadata, and source modification during execution. These are guard tests using a synthetic executable, not native protocol or mutation-kill evidence.

For left-Kan proof controls only, `BEND_MUTATION_PACKAGED_CLI` may explicitly select `_build/default/bin/mechanism.js.build.json`. The harness records JavaScript for these shared-language checks and native for relational assertions. It verifies the packaged source graph, Bend and Node identities, JavaScript artifact, and exact production launcher before and after each invocation. A CLI source change forces fresh compilation. `build/cli-packaged-guard-controls.json` records stale Node, artifact, and launcher refusals; the quiet checking smoke receipt is `.kanon-exec/run-pRzCVF`.

The optional isolated native CLI uses `--build-native-cli` to build
`_build/default/mutation/cli-isolated-native` with the same complete source,
Bend, Clang, artifact, and before/after guards. Set `BEND_MUTATION_NATIVE_CLI`
to its exact `.bundle.json` path and `BEND_MUTATION_BACKEND=native` to select it
for fixture-only CLI controls. It installs `cli-native.exe` in the mutation
output directory and leaves the shared `bin/mech.exe` launcher untouched.
The manifest permits only the fixed CLI entry and artifact paths, and only
CLI mode. A source change forces a fresh isolated build. Execution validates
source, toolchain, artifact, and manifest before and after the command.

The relational Python harnesses and `compile_protocol` use
`dev/bend2-process.py` to terminate the whole wrapper process group on timeout
or cancellation. The compiler wrapper retains its 3600-second limit. Original timeout
values and refusal predicates are unchanged. Guard-only and process-lifecycle
controls are infrastructure checks; they do not count as semantic mutant kills.

The managed execution sandbox can deny `killpg` with `EPERM` during external
cancellation. This error is not suppressed or treated as successful cleanup.
The recorded controlled replay succeeds when launched with authorized
escalation. In a restricted environment, retain the failed cleanup receipt,
inspect only the owned process IDs, and obtain the required execution permission
before relying on process-group cancellation. See
`build/relational-process-followup.json` for the separate observations.
