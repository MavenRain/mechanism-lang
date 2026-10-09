# Escrow vote aggregation (O1)

[O1.mech](O1.mech) is a port of the escrow-lang vote model: ballots,
configurations, tallies, the orbit map and its section, and aggregations
that factor a decision through the tally. It defines the local Kan record,
`toKan` and `fromKan`, the two round trips, and facts 1, 2 and 3 in both
directions. Each law has a definition. The file has no hole and no axiom.
`Decision` has three distinct outcomes: `dyes` (Release), `dno` (Refund),
and `dhold` (Hold), as in escrow-lang.

## Check

O1.mech uses the definitions in `prelude/init.mech`. Join the two files with
a newline and check the result. From the repository root:

```sh
{ cat prelude/init.mech; printf '\n'; cat examples/escrow-o1/O1.mech; } > /tmp/escrow-o1.mech
_bend2/bin/mech.exe check /tmp/escrow-o1.mech
_bend2/bin/mech.exe axioms /tmp/escrow-o1.mech
```

The check exits with 0. `examples/gpu-auction/category-bridge.mech` is checked
the same way (`test/gpu_auction_category.py`).

Check [Regression.mech](Regression.mech) after the proof to verify Hold,
the Kan round trip on the rule and unit, and a ballot permutation:

```sh
{ cat prelude/init.mech; printf '\n'; cat examples/escrow-o1/O1.mech; printf '\n'; cat examples/escrow-o1/Regression.mech; } > /tmp/escrow-o1-regression.mech
_bend2/bin/mech.exe check /tmp/escrow-o1-regression.mech
```

## Notes

- The checker has no eta rule for pairs. Thus `tripA` states the round trip
  pointwise: `fromKan n F (toKan n F a)` is equal to `mkAgg n F a.1 a.2`.
- `tripK` and `tripKUnit` state that `toKan n F (fromKan n F k)` preserves
  each rule value and each unit component of `k`. The `desc`, `fac`, and
  `uniq` fields are reconstructed by `toKan`.
- The conversion does not apply proof irrelevance to the indices of
  `MechProofEq`. Thus `uip` is in transport form: for each `P` and each two
  erased proofs `q1` and `q2` of `MechEq Decision a b`, `P q1` gives `P q2`.
- The last field of `Kan n F` quantifies over `P : MechEq Decision a b -> Type 0`.
  Thus `Kan n F` is in `Type 1`.
