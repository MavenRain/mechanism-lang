# M0 Stage A: prenex universe levels

Entry: 71f47449f943f54efb819a7bbaba175b3eb08703, the clean committed
Stage 0.  The vendor pin stays 936a43a92dd59a04698648f24fa5ae94cdb532df.
This brief implements M0-PLAN.md sections 4, 5, 8 and 10, using the
ratified overlay decision D-M0-2.  All final changes are staged; an agent
does not commit.

## Scope and integration decisions

- D-A-1: the trusted-kernel bound requires the user's outstanding ruling.
  Stage 0 measured 3,816 inherited lines against 3,000.  The encoder
  bound remains 900.  No passing result is claimed for an unmet bound.
- Count active trusted implementations once.  A full physical overlay
  replaces its vendor counterpart in the count.  Additional local kernel
  helpers and interfaces remain counted.  This accounts for the code
  actually compiled without hiding the complete overlay in generated
  textual patches.
- Compile unchanged source into mechanism libraries with Dune copy rules.
  Namespace aliases preserve the carried source while selecting the new
  kernel.  Execute the mechanism-linked inherited tests explicitly.
- Check global templates symbolically under a declared universe arity.
  Keep templates outside ordinary globals.  Recheck each closed instance
  before insertion.  This resolves the plan's conflicting descriptions of
  symbolic comparison and a monomorphic kernel after instantiation.
- Preserve the term and shape sums.  A separate Poly catalog supplies the
  global arity and specialization boundary that the pinned Global and
  Term.Global records do not contain.  References with universe arguments
  are a Stage B import representation, not a new term constructor.
- Rules delegates imax to the level implementation.  Conversion and rule
  annotation comparisons use budget-aware level equality through the
  caller's context.  The conv.ml overlay carries this propagation.
  Ordinary family universes stay closed in Stage A; universal positivity
  also guards large elimination.
- The five-form level grammar has compact successor offsets internally.
  Zarith prevents overflow and avoids allocating a unary chain for a
  large integer level.  The public level type remains abstract.
- The inherited executable has a top-level dispatch call.  The physical
  bin/kanon.ml overlay removes that call; mech.ml supplies the entry point.
  This preserves dispatch behavior without running it at module loading.

## Required evidence

Build through dev/dunecho.sh, with warnings as errors.  Run the inherited
kernel, surface and WASM suites linked to mechanism, plus test/levels.exe.
Check R0 counts and dispatch, vendor pin, overlay differences, frozen
denominators and the active trusted-source bound.  Preserve watchdog
tiers.  Report failed attempts and never count them as passing.

Level tests cover exact imax identities, zero/positive distinctions,
domination, high closed levels and scope.  Checker tests accept a valid
universal template and reject a template that passes a bounded set of
numeric samples.  Instantiation tests cover arity, closed arguments,
fresh names, separate instances, and unchanged input environments on
failure.  Ordinary inference must reject free universe variables.
The scope tests include ignored introduction-shape fields.  Budget tests
must expire during symbolic comparison as well as at checker entry.

In separate scratch copies, mutate imax to max, remove a zero/positive
partition, and remove the ordinary inference scope guard.  Require the
corresponding tests to fail and record final-source hashes unchanged.
Record full validation and mutation evidence in M0-BUILD-LOG.md and
MUTATION-LOG.md before staging the canonical tree.
