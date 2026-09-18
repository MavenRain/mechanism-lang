# Veil migration validation

Base: ba02c070f5b18c0310008d87a123d8de4a0c267e.
Dependency: Veil a7534cedeac82d396de8e23058ee6bc990560f65.
Date: 2026-09-17.

`checks.json` records actual captured command arguments, working
directories, exits and output hashes. Each stdout and stderr stream
is retained here. `sources.sha256` records the 33 sources that the
author changed, relative to the repository root. The two files that
the review round adds, the mutation driver `dev/veil-mutations.py`
and the mutation log `dev/MUTATION-LOG.md`, stay out of the hashed
set, because the set holds the author sources only. The closer's
close paragraph in `dev/M0-BUILD-LOG.md` moves that file's row of
`sources.sha256` a third time, to `51736a38b596`; the row count
stays 33.

The record is a capture of the author's runs. It is not a replay that
this repository can make. The working directory of each captured check
is the author copy `/Users/oobi/Documents/gpt12/mechanism-veil`, its
`-baseline` sibling, or their parent directory. Two tools stay outside
this tree: `check-veil-migration.py`, which gives the `static` verdict,
and the Veil build at `/Users/oobi/Documents/veil`, which gives the
upstream erasure check. This repository ships neither tool, so those
two rows are audit material and not gates.

The build, three new Veil gates and CPU auction checks pass. The
inherited full battery has 57 passing legs and eight failures. Seven
are timeouts and the eighth is TRUSTED-LINES, kernel=5475/3000 and
encoder=246/900. `full-battery.measure` retains every leg's result.
The three Veil gates were added after the full battery started and
were tested separately against the final build.

The clean Kanon baseline reproduces five timeouts: heterogeneous
left Kan checking and runtime, category runtime, category accessors
and functor runtime. The paired checking runs both stop at 300
seconds in the same phase, using 84.72 and 85.19 seconds of user CPU
on Kanon and Veil respectively.

Natural-transformation and left Kan runtime checks pass on the
clean baseline and on subsequent Veil runs with their original
limits. Their earlier full-battery timeouts remain in the record.
No inherited gate, oracle, watchdog tier or runtime budget was
removed or relaxed.

The standalone FHC shape example's erasure refusal is reproduced
with Veil's existing binary. The host fixture still builds and runs
successfully on Node and Wasmtime. See `../../VEIL-KERNEL.md` for
scope and initialization instructions.
