Source-compatible Functor record receipt, 2026-10-02, base 252733d.

Reproduce from the repository root with:
  make prelude-functor-compatibility-test

The make target uses the driver's default --export path, which exists only on
the machine that produced this receipt. On another machine, prepare the builds
and run the driver with --export:
  python3 -P dev/prelude-functor-compatibility.py \
    --export /path/to/uat.export \
    --mech "$PWD/_bend2/bin/mech.exe" --audit "$PWD/_bend2/test/prelude.exe" \
    --out "$(mktemp -d)/evidence"

The --export argument may point to any copy matching dev/denominators.json.
The driver requires a new evidence directory and checks the frozen export,
3202-declaration import graph, sources, native checker and audit build hashes.
The import graph is reproducible; import/types.ndjson is omitted from Git.
The manifest, summary, report, generated witnesses and full diagnostics remain.

Six Functor signatures and five support signatures reach NAME_AND_TYPE after
dependency discharge. The generic fixture proves four accessor computations.
Three stored-order witnesses pin the stored morphism binder order, both law
positions and the field count. Six bare-constant pins fix the outer binder
quantities of all six members. Three closed universe specializations check the
same fixture. The kernel axiom listing is empty and the empty-environment audit
passes.

All eight controls pass. Three run no kernel check: two re-read the report, and
the dependency control asserts that no checker ran. Every altered adapter
checks on its own first. Each fixture rejection has a twin without the
rejecting components, and that twin checks.

storedIdentity and storedComposition reject a consistent law reorder.
storedMorphisms rejects an adapter that stores the morphism map with binders
(y, x) and swaps them back in map. The quantity pins reject an erasure of both
category records at every site. An erasure of the source category in one member
is rejected only by that member's pin, for each of the six members. The
constructor row rejects erased nested object arguments, and it rejects the
object-level swap with the level diagnostic. Morphism levels are pinned by the
contextual target-equality row, which rejects a cross-wired adapter that the
Functor rows cannot see. Unswapped twins are accepted.

Three of the four accessor proofs have negative controls. The altered map
matches its own signature, and the map_id and map_comp rows reject it. Without
the map_id and map_comp quantity pins, the fixture rejects it at the morphism
projection. Without that projection, the fixture rejects it at those pins.
Adapters that return map_id or map_comp through a double Target_Eq_symm pass
all six Functor rows. Only identityProjection rejects the first, and only
compositionProjection rejects the second. No other well-typed object map
exists, so the object projection stays a positive check. Missing equality also
blocks the constructor and law signatures through dependency discharge. Missing
equality and Category mappings are refused before any checker runs.

gate.stdout and gate.stderr record the accepted run. That run wrote its
evidence to a temporary directory. The Evidence line in gate.stderr was
rewritten to this receipt path.

attempts/initial-status-check retains the failed first report and full command
output. Its kernel and audit checks passed; the harness used the wrong status
after dependency discharge. That attempt predates the per-component fixture.
Its adapter, equality and Category sources match the accepted run. The
computation fixture, the driver and the control file changed afterwards. That
failed run has no acceptance claim.

category-regression.json records the unchanged Category gate's successful rerun.
Its source_report_sha256 equals the committed prelude-category-compatibility
report.json, and its input hashes equal the committed Category sources.

This receipt covers signatures, stored order and projection computations.
Source theorem bodies, recursors, whole-record equality, runtime parity and
conversion to the erased core record remain open. The mapping inventory and
NEVER ledger are unchanged.
