# mechanism-lang x cuOpt

A combinatorial auction for GPU reservations, with exact VCG payments and
mechanism-lang certificates. Each tenant bids for alternatives containing named
MIG slices, HBM, CPU cores, and a time interval. At most one alternative per
tenant can win. Winner determination is a binary LP accepted by cuOpt.

The full auction and every tenant-removal counterfactual are checked by an exact
integer verifier before settlement. The kernel then checks the generic VCG
contract, each selected allocation's constraints, and its payment identities.
No CLI command provisions hardware or transfers money.

## Quick start

Initialize the pinned dependency with `git submodule update --init --recursive`.
Install OCaml 5.3 or newer, dune 3.24 or newer, zarith 1.14, zsh, and Python 3.11
or newer. For an opam-managed compiler, run
`opam install dune.3.24.0 zarith.1.14` and prefix the build command with
`opam exec --`. The build runner uses the installed dunecho where available.

From this repository:

```sh
zsh dev/dunecho.sh build
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install '.[test]'
mech-cuopt export examples/combinatorial/three-bidders.json --out /tmp/mig-model
mech-cuopt solve /tmp/mig-model --backend reference --out /tmp/mig-settlement
mech-cuopt verify /tmp/mig-model /tmp/mig-settlement/candidate.json --out /tmp/mig-rechecked
mech-cuopt audit /tmp/mig-model /tmp/mig-settlement
```

Use new output directories. If the compiler source tree is elsewhere, set
`MECH_ROOT` to it or pass `--mech-root` on export/solve/verify. The CPU package has
no third-party runtime dependencies. Python 3.11 or newer is required.
You can also use `python3 -P dev/mech-cuopt.py` in place of `mech-cuopt` without
installing the package. The Python wheel can run the GPU worker alone; the
exporter and verifier additionally need this source checkout and its built
`_build/default/bin/mech_cert.exe`.

In the example, A offers 12 for all eight slices, B offers 9 for the first four,
and C offers 7 for the last four. B and C jointly win with welfare 16. B pays 5,
C pays 3, and A pays zero. Removing either winner makes A's 12-credit bundle the
best alternative. VCG charges the externality imposed on the remaining tenants.
The [second hand case](examples/combinatorial/README.md) combines alternative
time windows with XOR bids and reuses the same slice across adjacent intervals.

## GPU worker

Install the cuOpt 26.08 Python interface following NVIDIA's
[installation guide](https://docs.nvidia.com/cuopt/user-guide/latest/install.html).
Copy the exported model directory to that host, install this Python package,
and run:

```sh
mech-cuopt propose /path/to/mig-model --out candidate.json
```

The worker does not need the mechanism compiler. Return its candidate to the
verifier and use the `verify` command above. A host with both dependencies can
instead run `solve --backend cuopt`. Every solve must report `Optimal`, and the
independent exact verifier must agree on its welfare. Time limits, fractional
solutions, infeasible results, model hash mismatches, and incomplete
counterfactuals are rejected.

The GPU interface uses [`Problem.read` for LP files](https://docs.nvidia.com/cuopt/user-guide/latest/cuopt-python/convex/convex-api.html).
The local implementation and kernel are tested on CPU. Actual NVIDIA GPU
execution requires a supported GPU host and is reported separately.

## Input and output

The [auction schema](schema/auction.schema.json) and
[solver candidate schema](schema/candidate.schema.json) describe the versioned
JSON formats. The CLI additionally checks unique IDs, cross references, per-host
CPU, per-slice HBM, time bounds, and the aggregate bid limit. Unknown fields,
duplicate JSON keys, booleans used as amounts, and non-finite numbers are errors.

Each tenant lists alternatives under `offers`. An offer contains an `id`, a
whole-credit `value`, and a `bundle` with `start`, `duration`, `slices`, and
`cpu_cores`. Slice claims name inventory IDs and requested `hbm_mib`. CPU is a
map from each requested slice's host to its reserved core count, including zero
when no CPU is required. The interval is `[start, start + duration)` in ticks.
MIG profile labels are inventory metadata, not commands to reconfigure a GPU.
Inventory may span several physical GPUs on each host.

An export contains normalized `auction.json`, checked `mechanism.mech`, a hashed
`manifest.json`, `full.lp`, and one `without_N.lp` per tenant in sorted ID order.
LP variables follow globally sorted offer IDs. `inspect` reports dimensions and
checks the exported LPs against the normalized input.

A settlement contains `settlement.json`, its exact submitted `candidate.json`,
the generated `certificate.mech`, and `receipt.json` binding their hashes.
`audit` independently replays optimization and kernel checks before comparing
the stored settlement and certificate. `check-certificate` checks only the
standalone source; use `audit` when validating an accompanying payment file.
Receipts are integrity records, not signatures. Retain the auction hash from a
trusted collection process when exchanging files across hosts.

## Mechanism

Inventory and each tenant's bundle menu are public and fixed for an auction.
Private reports assign a nonnegative integer value to each alternative. These
are XOR valuations: a tenant receives at most one of its alternatives. All
amounts are total bundle values in whole credits, not rates per slice or tick.

The objective maximizes reported welfare. Equal optima use the smallest binary
mask in lexicographically sorted offer-ID order. This fixed rule is independent
of bid values. It leaves resources unallocated when every bid is zero. A cuOpt
candidate with a different optimal tie is accepted for its welfare and
canonicalized before settlement; both selections are recorded.

For tenant i, let W be total welfare, v_i its selected offer value, and W_-i the
optimum after removing every offer of i. Its payment is W_-i - (W - v_i).
Each loser pays zero. The feasible set is downward closed, so payments are
nonnegative and no larger than the selected reported value. Under truthful
reporting this gives individual rationality.

## Proof boundary

`prelude/mechanism/vcg.mech` proves incentive compatibility from exact welfare
maximization and an opponent-only pivot. It uses signed utility, represented by
cross addition, and proves the payment bound used for individual rationality.
The specification also states the allocation optimality contract. These proofs
are generic over feasible allocation types and arbitrary structural naturals.

Each settlement includes a standalone `certificate.mech`. Its closed proofs
check binary choices, inventory and CPU constraints, tenant exclusions, welfare
arithmetic, pivot identities, and payment bounds for the full allocation and
every counterfactual. Native arbitrary-precision arithmetic avoids the original
demo's 31-credit execution limit.

Global integer optimality is verified by the Python branch-and-bound checker.
That checker, input-to-model translation, the native arithmetic boundary, and
the mechanism kernel are trusted implementation components. The `.mech` theorem
is conditional on the stated exact-optimality contract; this repository does
not claim a formally verified MIP solver or LP serializer. No source axioms are
introduced. The existing equality, transport, and category integration checks
remain available.

## Runtime limits

The input format accepts up to 128 offers, 64 tenants, and 256 inventory slices.
The sum of all bid values is at most 2^50 - 1, keeping cuOpt objective sums within
exact float64 integer range. Intervals use half-open tick ranges; resource rows
are generated at interval boundaries rather than by expanding the horizon.
Each requested HBM amount must fit its named slice. A slice is exclusively
reserved during its interval, even if less than its full HBM is requested.

Exact verification has explicit node and time budgets per optimization case.
Use `--node-limit` and `--seconds` to change them. Exhaustion returns an error
and never creates a verified settlement. This is a correctness-first research
implementation; no acceleration or production scalability claim is implied.

The schema covers already collected, closed bid profiles. Authentication,
sealed-bid collection, an accounting system, physical MIG discovery, scheduler
dispatch, robotics, and an optional LLM formulation loop are separate service
integrations. The package supplies deterministic models and settlements for
those systems to consume.

## Validation and development

After building, run `python3 -P dev/gpu-auction-check.py`. Add `--category` for
the existing category bridge and `--schemas` for the published JSON schemas
(install `.[test]` first). `--record FILE` writes a structured validation result.
`make auction-check-all` builds and runs all CPU auction checks.
The original milestone's runtime comparison also requires Node.js.

The suite compares the exact verifier against a separate exhaustive allocation
oracle on randomized small inventories, exhausts unilateral value reports on a
fixed XOR menu, and checks malicious solver output and corrupted artifacts.
Kernel mutation tests reverse utility order, remove the optimality premise,
alter allocations and prices, and inject an axiom. Each must be rejected.

The GitHub workflow runs these checks on Python 3.11 and 3.14. Its optional
manual `live_gpu` job requires a provisioned self-hosted Linux GPU runner.
Locally, `python3 -P dev/gpu-auction-check.py --gpu` requires an actual cuOpt
round trip and fails if cuOpt is unavailable. Mock adapter tests never count as
live GPU validation. The existing M0 gate remains independent and retains its
trusted-kernel size restriction. See [the validation record](dev/GPU-AUCTION-VALIDATION.md).
