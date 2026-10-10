# Escrow member classes (M8)

The files in this directory are the rung 2 proofs of the escrow-lang member
classes. A class list `cs : CSizes m` gives the size of each of `m` classes.
A configuration and a tally at `cs` have one configuration and one tally for
each class. The files use the ballots, configurations, tallies, `orbit`,
`sec`, `Decision` and `Agg` of [O1.mech](../escrow-o1/O1.mech) and the flips
of [O14.mech](../escrow-o14/O14.mech). The names start with C or c (or end
with X), because the join includes O1.mech and O14.mech.

- [Classes.mech](Classes.mech): the class model, its round trip and the
  one-class lemma.
- [O1C.mech](O1C.mech): O1.mech sections 4 to 7, stated one time over
  abstract types, then applied at `Config n` and at `CConfig m cs`.
- [O14C.mech](O14C.mech): the flip at `cs`, the orbit lemma at `cs`, and
  `flipFwd` and `flipBwd` of O14.mech, stated one time over abstract types,
  then applied at `Config n` and at `CConfig m cs`.

The files have no hole and no axiom.

## Names

| File | Names | Statement |
| --- | --- | --- |
| Classes.mech | `CPair`, `cpair`, `cMkP` | A pair type with a `case` eliminator, its constructor, and a typed constructor for a position where the type is not known. |
| Classes.mech | `CSizes m`, `CConfig m cs`, `CTally m cs` | The class lists of length `m`, and the configurations and the tallies at `cs`. |
| Classes.mech | `corbit m cs`, `csec m cs` | `orbit` and `sec` at each class. |
| Classes.mech | `cPairEqP` | Equal first components and equal second components give equal pairs. |
| Classes.mech | `csecOrbit m cs`, `corbitSec m cs` | The round trip at `cs`: `t` is equal to `corbit m cs (csec m cs t)`, and the symmetric form. |
| Classes.mech | `cOne n`, `cUnitEta` | The class list `[n]`, and each `u : MechUnit` is equal to `mechUnit`. |
| Classes.mech | `cOneT`, `cOneTo`, `cOneTBeta`, `cOneTEta` | A `Tally n` and a tally at `[n]` give each other, and the two maps are inverse. |
| Classes.mech | `cOneC`, `cOneCTo`, `cOneCBeta`, `cOneCEta` | The same for configurations. |
| Classes.mech | `cOneOrbit`, `cOneSec` | At `[n]`, `corbit` and `csec` are `orbit n` and `sec n`. |
| O1C.mech | `AggX`, `aggLX`, `orbitSecX`, `descAtX`, `facAtX`, `KanX`, `toKanX`, `fromKanX` | O1.mech section 4 over `X`, `Y`, `o : X -> Y`, `s : Y -> X` and `r : (t : Y) -> MechEq Y t (o (s t))`. |
| O1C.mech | `mkAggX`, `tripAX`, `tripKX`, `tripKUnitX` | O1.mech section 5 over the same parameters. |
| O1C.mech | `mkConstX`, `fact1X`, `govX`, `SelfX`, `fact2fwdX`, `fact3bwdX` | O1.mech section 6 over the same parameters. |
| O1C.mech | `fact2bwdX`, `fact3fwdX` | O1.mech section 7 over the same parameters. |
| O1C.mech | `cAggTo`, `cAggFrom`, `cKanTo`, `cKanFrom`, `cDescAtN` | At `Config n`: `AggX` and `KanX` are `Agg n F` and `Kan n F` (each map is the identity), and `descAtX` at `secOrbit n`. |
| O1C.mech | `cDescAtC`, `cToKanC`, `cFact1C`, `cFact2fwdC`, `cFact2bwdC`, `cFact3bwdC`, `cFact3fwdC` | The O1 facts at `CConfig m cs`, with `corbit m cs`, `csec m cs` and `csecOrbit m cs`. |
| O14C.mech | `cflipC m cs`, `cflipT m cs` | The flip on `CConfig m cs` and on `CTally m cs`: `flipC` and `flipT` at each class. |
| O14C.mech | `corbitFlip m cs` | `corbit m cs (cflipC m cs x)` is equal to `cflipT m cs (corbit m cs x)`, by `orbitFlip` at each class. |
| O14C.mech | `flipFwdX`, `flipBwdX` | For `a : AggX X Y o F`, flips `fX`, `fY`, `fD` on `X`, `Y`, `Decision`, and `o (fX x) = fY (o x)`: `F` commutes with the flips at each `x` exactly when `a.1` commutes with the flips on the image of `o`. |
| O14C.mech | `cFlipFwdN`, `cFlipBwdN` | `flipFwdX` and `flipBwdX` at `Config n`, with `flipC n`, `flipT n`, `flipD` and `orbitFlip n`. They have the types of `flipFwd` and `flipBwd` in O14.mech. |
| O14C.mech | `cFlipFwdC`, `cFlipBwdC` | `flipFwdX` and `flipBwdX` at `CConfig m cs`, with `cflipC m cs`, `cflipT m cs`, `flipD` and `corbitFlip m cs`. |

## Check

The files use the definitions in `prelude/init.mech`,
`examples/escrow-o1/O1.mech` and `examples/escrow-o14/O14.mech`. Join the
files in this order with a newline and check the result. From the repository
root:

```sh
{ cat prelude/init.mech; for f in examples/escrow-o1/O1.mech examples/escrow-o14/O14.mech examples/escrow-classes/Classes.mech examples/escrow-classes/O1C.mech examples/escrow-classes/O14C.mech; do printf '\n'; cat "$f"; done; } > /tmp/escrow-classes.mech
_bend2/bin/mech.exe check /tmp/escrow-classes.mech
_bend2/bin/mech.exe axioms /tmp/escrow-classes.mech
```

Both commands exit with 0 and write no output.

## Notes

- The checker has no eta rule for pairs, and `case` does not split a
  built-in pair. Thus the tallies and the configurations at `cs` are `CPair`
  values, and the proofs at `cs` use `case` on `CPair` in a `def rec` on `m`.
- The forms with the X suffix erase each parameter that their terms do not
  apply. The instances give the parameters at `Config n` and at
  `CConfig m cs`.
