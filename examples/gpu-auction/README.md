# GPU time auction with a checked mechanism

The first milestone sells **one indivisible bundle of eight MIG inventory slices**
for a fixed time slot. Three tenants submit one sealed bid each, in whole credits.
The highest bid wins; ties go to A, then B, then C. The winner pays the highest
competing bid and the losers pay zero. All-zero bids allocate to A for zero credits.
The inventory may span devices. This example does not assume eight slices fit
on one physical GPU.

The hand case in [three-bidders.mech](three-bidders.mech) has bids A = 12,
B = 9, C = 7. Its checked result is:

| Tenant | Bid | Allocated slices | Payment | Utility if truthful |
| --- | ---: | ---: | ---: | ---: |
| A | 12 | 8 | 9 | 3 |
| B | 9 | 0 | 0 | 0 |
| C | 7 | 0 | 0 | 0 |

The generated [LP snapshot](three-bidders.lp) and
[reference settlement](three-bidders.expected.json) are included for review.

## Run the milestone

From the repository root, build the existing compiler and runtime helper:

```sh
zsh dev/dunecho.sh build
python3 -P examples/gpu-auction/auction.py export --out /tmp/gpu-auction --check-wasm
python3 -P examples/gpu-auction/auction.py solve /tmp/gpu-auction --backend reference
python3 -P examples/gpu-auction/auction.py verify /tmp/gpu-auction /tmp/gpu-auction/reference-solution.json
```

Choose a new output directory. Export writes the assembled `.mech` source,
`allocation.lp`, a manifest with hashes, and twelve Wasm exports. It checks the
source with the existing kernel, audits source axioms, and evaluates every value
before writing the LP. `--check-wasm` additionally compares all exports on Node.
Node 22 or newer is needed for the inherited Wasm GC runtime.

The adapter currently accepts exactly three bidders, eight slices, and bids in
0..31. This is a practical limit for evaluation of structural naturals in the
demo. The source theorems quantify over arbitrary structural natural numbers.
Edit or supply a `.mech` program with `--program`; changing the sample bids also
requires updating its hand-case equality proofs. A stale proof fails checking.

## Send the model to cuOpt

The adapter targets the cuOpt 26.08 Python API:
[`Problem.read` accepts LP files](https://docs.nvidia.com/cuopt/user-guide/latest/cuopt-python/convex/convex-api.html).
On a supported host with cuOpt and the mechanism compiler installed:

```sh
python3 -P examples/gpu-auction/auction.py solve /tmp/gpu-auction --backend cuopt
```

Alternatively copy `auction.py` and the exported artifact to a cuOpt GPU host.
The worker only needs Python and cuOpt:

```sh
python3 -P auction.py propose /path/to/gpu-auction --out cuopt-candidate.json
```

Return that candidate file to the machine with the mechanism compiler:

```sh
python3 -P examples/gpu-auction/auction.py verify /tmp/gpu-auction cuopt-candidate.json
```

`propose` produces an unverified candidate. Verification rechecks the source and
model, checks the LP hash, requires an optimal status, validates finite binary
values and capacity, and compares the winner and objective with the exact
mechanism result. Payments come from the checked mechanism. A feasible incumbent
or time-limited answer cannot authorize settlement. No command provisions GPU
resources, charges tenants, or sends bids to a remote service.

The MIP maximizes `38 x_A + 28 x_B + 21 x_C` in the hand case, subject to
`x_A + x_B + x_C = 1`, eight-slice capacity, and binary variables. In general the
coefficients are `3 * bid + priority`, with priorities 2, 1, 0. For whole-credit
bids, this preserves welfare order exactly and breaks ties without floating-point
epsilon weights. Reported welfare remains 12 in the hand case. The ranked
objective is 38. The price is 9; LP objective weights do not enter payments.

## Proof and trust boundary

[second-price.mech](../../prelude/mechanism/second-price.mech) provides a certified
comparison and source proofs of dominant-strategy incentive compatibility,
truthful individual rationality, nonnegative payments, winner payment equal to
the competing threshold, and zero loser payments. Truthfulness is instantiated
for each bidder's threshold and tie priority. These hold with other reports
fixed, nonnegative private values, quasi-linear utility, and no budget constraints
or externalities.

`AuctionUtilityGe` is the four-case normal form of the **signed** utility ordering
for `u(win) = value - price`, `u(lose) = 0`. A losing truthful bidder compared with
a winning deviation requires `value <= price`. Overbidding can therefore incur
negative utility; the proof does not use truncated subtraction.

The hand-case allocation, payments, and eight-slice capacity are equality proofs
checked by the kernel. Per-instance feasibility, optimum, and tie order are also
checked at the solver boundary. This milestone does not prove a general compiler
correctness theorem for the Python LP serializer or a universal capacity theorem
for the three-bidder allocation. The generic economic proof and the closed
allocation certificates have distinct scopes.

The source uses the existing `MechEq` and `mechTransport` prelude. The separate
[category bridge](category-bridge.mech) checks the allocation map through the
existing category port. These are checked mechanism-lang libraries. The existing
Lean importer still distinguishes imported source types from kernel-checked
parity; this example does not upgrade NAME_ONLY mappings or claim a newly proved
Lean-to-mechanism translation. No Lean theorem is postulated as an auction axiom.

The proof core uses structural naturals. Native `Nat`, its arithmetic primitives,
the mechanism kernel, erasure, host runtime, and the bridge remain implementation
trust boundaries. No source axioms are introduced. Source data and hashes are
rechecked when verifying; a manifest alone is not a proof or an authentication
signature.

## Validation and next step

```sh
python3 -P test/gpu_auction.py
python3 -P test/gpu_auction_category.py
```

The main suite covers the hand case, every winner and tie position, all-zero
bids, 1,875 signed-utility deviations, 125 LP ordering cases, corrupted solver
responses, false source proofs, source axiom injection, and kernel/Node agreement.
The category integration check is separate because loading the complete category
group is more expensive. API adapter tests use a fake cuOpt object; they do not
claim GPU execution. See [VALIDATION.md](VALIDATION.md) for actual runs.

Next, extend winner determination to tenant-defined bundles and VCG externality
payments, including counterfactual optima. Duration, HBM, CPU, MIG topology,
tenant authentication, sealed-bid collection, and dispatch belong to that later
integration. The current CLI operates on an already collected bid profile.
