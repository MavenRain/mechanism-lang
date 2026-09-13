# M0 Stage C: ordered template checking

Date: 2026-09-12.  Base: ac3f35f.  This increment removes a repeated
kernel check of each symbolic family-group member.  It addresses
the template checking cost recorded by the left Kan increment.
Stage C and U1 remain open.

## Contract

The surface previously elaborated and checked every member to
build the environment for subsequent members.  It then passed all
members to the family catalog, which checked them again.

`Family_poly.declare_group_elaborated` combines those two passes.
It checks the ordered families, then requests one raw member at a
time through a callback.  The callback reads the checked families
and preceding checked members.  The catalog traverses the returned
syntax for universe scope and forbidden template references.  It
checks the member with `Check.check_decl_at` before installing it
in the temporary environment for the next callback.

A callback cannot return a certified entry or replace that
environment.  Postulates, missing bodies, name collisions and
ill-typed members are refused.  A callback error, scope error,
kernel error or exhausted budget stops later callbacks.  The
catalog is returned only when the whole group succeeds.

The raw `declare_group` API uses the same checker with callbacks
that return its supplied declarations.  The surface keeps its
constructor-name and cross-catalog reservations.  Each closed
specialization still renames, validates and kernel-checks every
member against the caller's current globals.  Family formation,
kernel rules, source grammar and installed names retain their
contracts.

## Validation

The family-group suite adds eight cases.  They check callback
ordering, complete predecessor families, installed-program parity
with the raw API, invalid bodies, postulates, hidden universe
annotations, callback errors, invalid groups, cancellation between
callbacks and cancellation before the first callback.  The last
case measures the polls of the family phase with an empty member
list, then cancels on the next poll.
The refusal checks compare exact error values.  One more case pins the
refusal precedence of two failing members: the group names the first
member, and the raw `declare` entry point names the later traversal
refusal.  `surface/family_poly.mli` states both orders.  Existing prenex
and category suites exercise the source path and closed instances.

`dev/congruence-mutations.py` retains its nine controls and adds
C-CONG-M10, C-CONG-M11 and C-CONG-M12.  They install a member
without checking its body, supply callbacks with the original
environment, and remove the budget poll that guards the first
callback.
The member-checking control C-CONG-M4 follows the consolidated
checking call.  Every control must compile, produce its named
failure, and leave a passing restored suite.

`test/template_cost.ml` measures successful checking with a polling
budget.  It reports the number of checked members, poll calls,
allocated words and process CPU time after parsing.  With
`--through compFunctor`, it checks the category group through the
functor members.  Without that option, it checks the full group,
including left Kan extensions.  A source that is not one family
group is refused.  It does not specialize the group, so zero output
entries and families are expected.  The member count follows the
checked prefix, and `dev/template-cost.py` refuses a scope whose
member counts differ between the two trees.  The benchmark measures
declaration work only.  The battery runs the driver in the MED leg
TEMPLATE-COST, which checks the group through `Hom` and requires the
refusal of an unknown member name.

For a before/after comparison, use a separate checkout of ac3f35f
with the same pinned vendor tree.  Copy `test/template_cost.ml` and
the updated `test/dune` there.  The harness builds both checkouts
itself.  From the updated checkout, run:

```sh
python3 -I dev/template-cost.py BASELINE_TREE NEW_REPORT.json
```

The harness builds the driver in each tree before the first
measurement, so every timed executable comes from the sources of
its own tree.  It verifies identical source and driver hashes,
records both executable hashes, records the base commit and the
`git status --porcelain` lines of each tree under `trees`, and
alternates run order.  It measures two
runs per version and scope by default.  Wall and CPU time depend
on host load; poll and allocation counts supplement those times.
The report and gate evidence live under
`dev/validation/stage-c-template-checking/`.

No watchdog or gate predicate changes.  Kernel, erasure, encoder
and vendor sources retain their bytes.  The inherited trusted-line
bound remains open at kernel=4208/3000 and encoder=246/900.

## Remaining work

Closed specialization and erasure still repeat work for the large
category group.  This increment removes one redundant symbolic
member check; it does not solve the full performance problem.
Functors across separate universe pairs, source-type parity and
the M0 mapping gate remain due.
