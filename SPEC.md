# mechanism-lang specification

## R0 counts

The block below is inherited from kanon at PIN
(936a43a92dd59a04698648f24fa5ae94cdb532df, pin SPEC.md:176-183) and it
is copied byte for byte.  M0 changes no row of it.  `mech spec-count`
prints this block and dev/r0-count.sh diffs the two.  A count that grows
fails the R0-COUNT gate leg.

```
formers 2: Lan Ran
schema constructors 4: In Elim Sec Out
shapes declared 5: SPi SColl SPar SMu SNu
shapes admitted 3: SPi SColl SMu
named rules declared 3: proof-irrelevance subsingleton-large-elimination literal-fast-path
named rules present 3: proof-irrelevance subsingleton-large-elimination literal-fast-path
eta rows 3: Ran-SPi Lan-SPi Ran-SColl
no eta 3: Lan-SColl Ran-SMu Lan-SMu
```

Every number in the block is the length of the list printed after it.

The block moves at M2 on exactly two rows, shapes admitted 3 to 4 and no
eta 3 to 4, when SPar is admitted for Quot (M0-PLAN.md:108).  Any
earlier growth is an R0-COUNT failure.
