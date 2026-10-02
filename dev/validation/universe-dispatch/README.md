# Universe value conversion dispatch

Base: a9eca9a. General conversion at `Value.VUniv` now uses structural
conversion directly. The predecessor already skipped its proposition
probe, then reduced the universe type and passed through subsingleton
and eta dispatch. Those branches always selected structural conversion.

Other type arguments retain the predecessor's conversion path. The
kernel conversion suite covers distinct opaque proposition types and
universe inhabitants, proof irrelevance, function eta and binder names.
The conversion budget suite compares general universe conversion with
type conversion on equivalent symbolic universe expressions. It also
requires refusal at one fewer poll and success at the measured boundary.

`polls.bend` reproduces the predecessor's general conversion path and
compares it with the new path and type conversion. This small opaque
proposition comparison uses zero polls on all three paths. This records
budget compatibility, with no whole-program performance claim.

Reproduce the poll measurement with the pinned Bend compiler:

```sh
python3 -P dev/validation/universe-dispatch/measure.py --bend /path/to/bend-2.0.27
```

Rebuild and run the kernel and frontend unit groups:

```sh
python3 -P dev/validation/universe-dispatch/validate.py --bend /path/to/bend-2.0.27
```

`validation.json` records the validated inputs as one fingerprint.
`inputs.sha256` is the SHA-256 of a JSON object with sorted keys. The
object maps each relative path to the SHA-256 of its file. It covers the
files below `bend2`, `prelude` and `test/fixtures` with the suffixes
`.bend`, `.part`, `.c`, `.js`, `.mjs`, `.mech`, `.json` and `.py`, and
also `dev/bend2-build.py` and `dev/bend2/test-manifest.json`.
`inputs.files` is the number of paths. Check the fingerprint on a clean
checkout:

```sh
python3 -P dev/validation/universe-dispatch/inputs.py
```

The host killed native driver and combined kernel shard builds during
the first attempts. Validation uses individual unit group builds.
All 20 kernel and 14 frontend unit groups pass; their records are
`kernel.json` and `frontend.json`.
Left Kan solution transport runtime validation remains open.
