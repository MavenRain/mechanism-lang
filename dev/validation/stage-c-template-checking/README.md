# Template checking validation

Base: ac3f35f.  Build copy: `/Users/oobi/Documents/gpt12/mechanism-universes`.
Cost baseline copy: `/Users/oobi/Documents/gpt12/mechanism-template-baseline`.
Every battery and mutation artifact here was produced in the build
copy, and `cost.json` compares the build copy with the cost baseline
copy.  The implementation changes the surface elaborator
and family catalog.  Kernel, encoder and vendor sources retain
their base bytes.  The design is
`dev/M0-STAGE-C-TEMPLATE-CHECKING.md`.

## Cost comparison

`cost.json` records eight successful checks: two runs per compiler
for a category group prefix through compFunctor, and two per
compiler for the complete category group.  The baseline checkout
uses ac3f35f with only the benchmark driver and its Dune stanza
added.  Both trees use the same source and driver hashes.

`cost.json` is an archive of the capture run.  It was produced by the
pre-fix driver, whose `driver_sha256` is
`201f2df57de075611b26ebba905fd4c97f8162879e74291ab40a209c91b9edab`, and
by the pre-fix harness.  The staged `test/template_cost.ml` now hashes
`794cf6b4bd6351246277b58cad94339449b510637569a3368e764d496a6f6141`, and
the staged `dev/template-cost.py` writes a `trees` block that this
archive does not hold.  For the same reason every run row of the archive
reports `entries` 0 and `families` 0 and holds no `members` field.  The
percentages below are those of the archived capture.  A new capture with
the staged driver and harness replaces this file and its numbers.

The complete group has these counts on both repetitions:

| Metric | Before | After |
| --- | --- | --- |
| Budget polls | 74,719,726 | 55,248,999 |
| Allocated words | 28,710,148,193 | 19,908,735,939 |

That is 26.06 percent fewer polls and 30.66 percent fewer allocated
words.  Allocation counts cumulative OCaml allocation across the check.

The prefix through compFunctor has these counts:

| Metric | Before | After |
| --- | --- | --- |
| Budget polls | 5,694,720 | 4,464,293 |
| Allocated words | 2,214,413,436 | 1,671,785,048 |

That is 21.61 percent fewer polls and 24.50 percent fewer allocated
words.  The measurements cover symbolic declaration checking after
parsing.  They do not measure specialization, erasure or execution.
The reports retain CPU and wall times, which vary substantially
with system load.  No wall-time improvement is claimed.

To reproduce, add the same `test/template_cost.ml` and its Dune
stanza to a separate baseline checkout, build both trees with
`zsh dev/dunecho.sh build`, then run from the updated tree:

```sh
python3 -I dev/template-cost.py BASELINE_TREE NEW_REPORT.json
```

## Mutation coverage

The replay uses `dev/congruence-mutations.py`.  It retains all nine
existing controls.  C-CONG-M4 follows the consolidated member check.
C-CONG-M10 fabricates a definition entry while keeping the name
collision guard, which must fail the invalid-body test.
C-CONG-M11 supplies callbacks with the original environment, which
must fail the predecessor-family test.  A scope-only trial was
discarded: the kernel independently enforces the same level scope,
so removing that one guard did not remove the refusal.

The archived replay detects all eleven controls that
`dev/congruence-mutations.py` held at the capture.  The twelfth control
C-CONG-M12, which removes the budget poll before the first member
elaboration, was added to the tool after this archive, so the staged
tool holds twelve controls.  `mutations.json`
records each change and its input and mutant hashes.
`mutations.log` records the successful replay.  The `mutations/`
directory retains each control's stdout and stderr and the passing
baseline and restored family-group and congruence suites.

## Gate results

The complete battery records 37 PASS legs of 44.  Six behavior legs
hit watchdogs, and TRUSTED-LINES reports the inherited
kernel=4208/3000 and encoder=246/900 bound.  The original transcript
is `gates.log`; `gates.json` records every verdict and measurement.

All six expired behavior legs were rerun separately with their
original limits.  DEPENDENT-CLOSURE-RUNTIME passes in 5.19 seconds
with nine cases, three hosts and the payload mutation.  The
following checks remain timed out:

| Check | Limit reached |
| --- | --- |
| PRELUDE-CATEGORY-ACCESSORS | 210 s kernel batch |
| PRELUDE-FUNCTOR-RUNTIME | 210 s kernel batch |
| PRELUDE-LEFT-KAN-RUNTIME | 210 s kernel batch |
| PRELUDE-NATTRANS | 300 s source suite |
| PRELUDE-NATTRANS-RUNTIME | 210 s kernel batch |

`rechecks/results.json` records commands, elapsed times, exit codes
and output hashes.  The corresponding stdout and stderr files are
beside it.  System load exceeded 100 during the battery, and a load
reading during the reruns was above 50.  These checks remain
incomplete.  The gate battery remains red.
No watchdog, oracle or fixture was relaxed.  The allocation
improvement does not close the category performance work.

`receipt.json` binds the source files, checker binaries and retained
artifacts to SHA-256 hashes.  The benchmark driver was added and
built while the battery ran; its successful eight-run comparison
is recorded separately.  The production compiler, runtime harnesses
and gate fixtures kept the same bytes throughout the battery and
reruns.
