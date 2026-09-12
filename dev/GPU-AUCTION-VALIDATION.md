# Combinatorial GPU auction validation

Validated on 2026-09-12 against the mechanism-lang working tree at HEAD
`7c6d50d8b27f8676c75eb781ff63d5c5b7ddddd7`, including its existing staged language
work. Implementation and initial validation used an isolated copy under
`/Users/oobi/Documents/gpt13/mechanism-cuopt`.

## Results

| Check | Result |
| --- | --- |
| Compiler and production certificate checker build | PASS, zero errors and warnings |
| Combinatorial allocation, incentives, solver candidates | PASS, 17 tests |
| Real kernel proofs, mutation rejection, artifact and CLI checks | PASS, 9 tests |
| Published JSON schemas | PASS, 2 tests |
| Original single-bundle second-price milestone | PASS, 11 tests |
| Category, allocation map, and equality bridge | PASS |
| Python package installation in an isolated virtual environment | PASS |
| Installed CLI audit outside the source directory | PASS |
| Pinned Kanon commit fetched from its GitHub origin | PASS |
| NVIDIA GPU execution | Not run on this Darwin arm64 host |
| Existing M0 trusted-line gate | FAIL, kernel 4208/3000, encoder 246/900 |

The 39 Python tests include an independent exhaustive oracle over 120 random
small resource menus and 256 unilateral reports for a two-alternative XOR
tenant. Additional cases cover half-open time intervals, CPU contention,
multiple hosts, empty menus, zero bids, deterministic ties, all alternatives
removed in a counterfactual, and exact arithmetic at the aggregate bid limit
of `2^50 - 1`.

Hostile inputs include duplicate JSON keys, non-finite values, fractional
allocations, a feasible but suboptimal solve, omitted counterfactuals, wrong
LP hashes, and model or certificate edits accompanied by rewritten hashes.
False utility order, missing optimality evidence, changed bids, false capacity
and payment equations, and an injected axiom are rejected by the real kernel.
Failed verification never publishes a settlement directory.

The three-bidder combinatorial result is welfare 16, allocation B-first and
C-last, and payments A=0, B=5, C=3. Its normalized auction SHA-256 is
`6488d2c4c73c33d455cc2294766e7227e5753d6887e8e2059d7a7bc2ad2001ea`.
The time-alternatives hand case has welfare 21 and payments A=0, B=5, C=4.

## Evidence and reproduction

Run `python3 -P dev/gpu-auction-check.py --category --schemas --record FILE`
after building and installing `.[test]`. The structured record reports each
suite's exit code and whether live GPU execution was requested. Recheck saved
settlements with `mech-cuopt audit MODEL SETTLEMENT`.

Initial captures under the isolated copy's `.kanon-exec/`:

- `run-9cXOS4`: final native build.
- `run-AZqIqi`: 28 combinatorial, kernel, and schema tests after final model changes.
- `run-1f6eFn`: complete auction runner including the original milestone and category bridge.
- `run-lEAuWr`: package and test dependency installation, Python 3.14.7.
- `run-vqubDa`: unchanged trusted-line gate result.

The installed CLI audit is in the workspace capture `run-3X8sBB`.
`mechanism-cuopt-work/all-checks.json` records the extended runner. The build
ledger could not index the isolated copy, so the build was captured directly
with the repository's normal dunecho runner.

## Scope

The generic `.mech` theorem assumes exact welfare optimality and an opponent-only
pivot. Closed certificates prove feasibility and payment arithmetic. Exact
integer optimality and JSON-to-LP translation remain trusted Python code, as
documented in [the project guide](../GPU-AUCTION.md). There are no new source
axioms or kernel edits.

Mock cuOpt API tests establish adapter behavior, not a live GPU result. The
manual CI GPU job and `--gpu` check explicitly require the real solver. No GPU
speedup, scheduler dispatch, payment transfer, or completed M0 acceptance is
claimed. Existing gate scripts and their trusted-line limit remain intact.
