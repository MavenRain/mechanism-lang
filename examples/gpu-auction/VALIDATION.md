# GPU auction milestone validation

Validated on 2026-09-12 in `/Users/oobi/Documents/gpt13/mechanism-cuopt`,
an isolated copy of the mechanism-lang working tree. The source checkout reported
HEAD `7c6d50d8b27f8676c75eb781ff63d5c5b7ddddd7` and already contained staged work.
This milestone changes the README and adds auction source, example, adapter,
and focused tests. It does not change the compiler or existing gate scripts.

## Completed checks

| Check | Result |
| --- | --- |
| `zsh dev/dunecho.sh build` | PASS, zero errors and warnings |
| General auction source | Kernel check PASS, no source axioms |
| Hand case export with `--check-wasm` | PASS, twelve kernel/Node values agree |
| Reference solve and separate `verify` | PASS, A receives 8 slices and pays 9 |
| `python3 -P test/gpu_auction.py` | PASS, 11 tests, final revision including maximum bids, 9.445 s |
| `python3 -P test/gpu_auction_category.py` | PASS, category composition and equality certificates |
| cuOpt unavailable path | Expected exit 2, explicit unavailable diagnostic, no candidate written |

The main suite includes 125 bid profiles for LP objective ordering, 1,875
signed-utility deviations, every winner and tie position, all-zero bids,
false utility/payment/equality proofs, injected source axioms, LP tampering,
non-optimal statuses, non-finite/fractional/invalid allocations, wrong objectives,
and responses carrying the wrong LP hash. The cuOpt API test uses a fake object.

Captured evidence under the repository's `.kanon-exec/`:

- Build: `run-eowkpN`
- Main suite, final revision: `run-HH0nFz`
- Category integration: `run-dU1gH9`
- Reference solve: `run-gw3lgU`
- Separate verification: `run-bA0u3l`
- Expected unavailable cuOpt diagnostic: `run-PG4EIa`

The full hand-case artifact, including assembled source and Wasm modules, is
in `/Users/oobi/Documents/gpt13/mechanism-cuopt-work/demo`. Its LP SHA-256 is
`fdc03a282ece0a339267c5b768321719eebcdf92a9d409cbeced550191f89e76`.
`three-bidders.lp` and `three-bidders.expected.json` were copied from this run.

## Limits of this validation

The host reports `Darwin arm64`, and cuOpt is not installed. No NVIDIA GPU solve,
cuOpt LP parser execution, acceleration measurement, resource provisioning, or
payment transaction was performed. The cuOpt candidate path is ready to run on
a supported host and return its JSON for local verification.

The proof scope and implementation trust boundaries are described in README.md.
The inherited repository-wide M0 gate has an existing trusted-kernel size issue;
this scoped milestone does not claim to close that gate or Lean mapping parity.

During implementation, loading the complete category group into every auction
run hit a 60-second adapter timeout. Category integration now has a separate
check; normal auction export retains the data/equality/transport library and
the auction proof. Failed proof drafts were rejected by the kernel and replaced
with checked definitions. The eleven-test suite and category check above pass on
the resulting implementation.

A stress profile `(255, 255, 254)` exceeded the 60-second runtime budget
(`run-jlr8jy`). The adapter now enforces a 31-credit maximum, with a regression
at that declared boundary. This changes the executable demo's input limit,
not the universally quantified source theorem.
The `(31, 31, 30)` boundary profile passes both kernel and Node checks in
the final suite.
