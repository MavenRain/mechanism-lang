# Escrow decision flip (O14)

[O14.mech](O14.mech) is the rung 2 proof of the escrow-lang decision flip.
The flip exchanges Release (`dyes`) and Refund (`dno`) and keeps Hold
(`dhold`). On ballots it exchanges `byes` and `bno` and keeps `babs`. The
file uses the ballots, configurations, tallies, `orbit`, `Decision` and `Agg`
of [O1.mech](../escrow-o1/O1.mech). It defines and proves:

- `flipB`, `flipD`, `flipC n` and `flipT n`: the flip on `Ballot`,
  `Decision`, `Config n` and `Tally n`. `flipT` exchanges the Yes count and
  the No count of a tally.
- `flipBInv`, `flipDInv` and `flipTInv`: each flip is an involution.
- `orbitFlip`: `orbit n (flipC n x)` is equal to `flipT n (orbit n x)`, by
  `def rec` on `n`.
- `flipFwd` and `flipBwd`: for `a : Agg n F`, `F` commutes with the flip at
  each configuration exactly when `a.1` commutes with the flip on the image
  of `orbit n`.

The file has no hole and no axiom.

## Check

O14.mech uses the definitions in `prelude/init.mech` and
`examples/escrow-o1/O1.mech`. Join the three files with a newline and check
the result. From the repository root:

```sh
{ cat prelude/init.mech; printf '\n'; cat examples/escrow-o1/O1.mech; printf '\n'; cat examples/escrow-o14/O14.mech; } > /tmp/escrow-o14.mech
_bend2/bin/mech.exe check /tmp/escrow-o14.mech
_bend2/bin/mech.exe axioms /tmp/escrow-o14.mech
```

Both commands exit with 0 and write no output.

## Notes

- The checker has no eta rule for pairs. Thus the file does not state
  `flipC n (flipC n x) = x`. The orbit lemma uses only the projections of
  `flipC (mechSucc p) x`.
- escrow-lang cannot prove the flip on `Config` or `Tally`: its `Nat` has no
  eliminator, so the exchange `a + (b + c) = b + (a + c)` at open naturals
  has no proof there. `MechNat` has `case` in a `def rec`, so this file
  proves the orbit lemma by recursion on `n`.
