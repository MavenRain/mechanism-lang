# Left Kan cocone laws evidence

The source base is `84a122d51f22d3beeea8d2489a9854f25072d273`.
The slice adds `postCoconeId` and `postCoconeVcomp`, mixed-universe
contracts, certified applications and three refusal fixtures.

The standard native `prelude-left-kan-laws` driver passed with exactly
`PRELUDE-LEFT-KAN-LAWS-OK entries=989 families=9 computations=16 negatives=13`.
Its stdout and empty stderr are preserved in `kernel.stdout` and
`kernel.stderr`. This includes all ten existing refusals and the three new ones.

The native `native-equations.bend` checker passed the complete fixture:
989 checked entries, nine families, both mixed-universe contracts and
all 16 computations. It then rejected the three new negative fixtures.
`native-equations.stdout` records their exact diagnostics, and
`native-equations.stderr` is empty. The new `.err` oracles were extracted
from this successful run. The existing ten oracles were preserved.

`focused-source.mech` retains the new declarations and their lexical
dependencies, including both mixed-universe contracts. It keeps the
original definition bodies and group headers. `focused-extraction.json`
records the input hashes and retained declarations. Native `mech check`
passed this source with exit 0 and empty stdout and stderr, recorded in
`focused-kernel.stdout` and `focused-kernel.stderr`. This focused check
does not establish a pass for the full 989-entry gate.

`runtime.stdout` records the expanded runtime comparison with the
focused native `prelude-runtime` driver. The command that checks the
source and emits Wasm exceeded the existing 480-second deadline. The total watchdog and host deadlines
remain unchanged.

`baseline.stdout` and `baseline.stderr` record the same native driver
running the unchanged base's ten runtime exports. The baseline also
exceeded the 480-second deadline for the same command. The baseline sources were
copied from the clean original checkout, and the native driver was
identical for both comparisons.

These runtime observations establish a pre-existing deadline failure
under the observed execution conditions. They do not establish that
the expanded runtime comparison passes, nor that the change preserves
its execution time. Canonical Bend acceptance and R3 measurements
remain open as recorded in `MIGRATION-BEND2.md`.

`javascript-equations.stderr` records the full JavaScript equation
check exhausting its default heap. Trailing spaces in the captured log were
removed for repository whitespace checks. The native checks above succeeded.

After review, the `post-vcomp-missing-transformation` negative declares
the `Laws_CoconeEq` conclusion of its positive twin `postVcompLaw`, and
its body still omits `Beta`. Its `.err` oracle and its
`native-equations.stdout` row were regenerated with
`bend native-equations.bend -o native-equations.c`,
`clang -std=c11 -O1 native-equations.c -lpthread -lm -o native-equations`
and `./native-equations <root>` (Bend 2.0.25). The other two rows did not
change. The new diagnostic compares the Pi over `sigma` with the
`CoconeEq` Pi over `x`. The rebuilt Bend 2.0.27 native
`prelude-left-kan-laws` driver then printed
`PRELUDE-LEFT-KAN-LAWS-OK entries=989 families=9 computations=16 negatives=13`.
