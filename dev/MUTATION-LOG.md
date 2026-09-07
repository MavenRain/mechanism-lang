# M0 mutation log

## Stage 0

Every mutation runs on a copy under the scratch directory
/private/tmp/claude-501/-Users-oobi-Documents-claude4/0c2b51e5-60d5-46cb-b020-ae52f441e18e/scratchpad/stage0,
and never on the repository files.  The judge ran each check again on
2026-09-07 with the runner script judge-mutants.sh, which copies .git,
.gitmodules, .gitignore, PIN, dune, dune-project, dev/, lib/ and vendor/
into the scratch directory before it changes one byte.  The runner
prints the repository file after each mutation, so the log shows that
the repository stayed unchanged.

| id | leg | command | result |
| --- | --- | --- | --- |
| S0-M1 | PIN | copy the repository, write `836a43a92dd59a04698648f24fa5ae94cdb532df` to the copy's PIN file, then `zsh COPY/dev/gates.sh --leg pin` | killed |
| S0-M2 | PIN-DELTA | copy the repository, write `git -C COPY/vendor/kanon show 936a43a92dd59a04698648f24fa5ae94cdb532df:lib/term.ml` to the copy's `lib/term.ml`, which dev/PIN-DELTA.md holds no row for, then `zsh COPY/dev/pin-delta.sh` | killed |
| S0-M3 | DENOMINATORS | copy dev/denominators.json and dev/DENOMINATORS.sha256, change the date field of the JSON copy from 2026-09-07 to 2026-09-08, then `shasum -c DENOMINATORS.sha256` from inside the copy directory | killed |

S0-M1.  The leg printed
`FAIL PIN pin=836a43a92dd59a04698648f24fa5ae94cdb532df gitlink=936a43a92dd59a04698648f24fa5ae94cdb532df head=936a43a92dd59a04698648f24fa5ae94cdb532df want=936a43a92dd59a04698648f24fa5ae94cdb532df`
and exited 1.  The repository PIN file still reads
936a43a92dd59a04698648f24fa5ae94cdb532df.

S0-M2.  The control run on the unchanged copy printed
`PIN-DELTA file diff expected` and `PIN-DELTA OK` at exit 0.  The
overlay file is 4,788 bytes and dev/PIN-DELTA.md holds no `| lib/` row.
The mutated copy printed `PIN-DELTA lib/term.ml NO-ROW FAIL` and then
`PIN-DELTA FAIL`, and exited 1.  The repository lib/ directory still
holds .gitkeep only.

S0-M3.  The control run printed `denominators.json: OK` at exit 0.  The
mutated copy printed `denominators.json: FAILED` and
`shasum: WARNING: 1 computed checksum did NOT match`, and exited 1.  The
digest of the repository file is still
3148d714a481696297e75ef416255618cbc2cd9b56ab84c34f5fb9464fa7cd51, the
value dev/DENOMINATORS.sha256 records.

No mutant survived.
