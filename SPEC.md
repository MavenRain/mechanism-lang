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

## D3: prenex universe levels

Stage A replaces closed integer levels with zero, successor, maximum,
impredicative maximum and global universe variables.  The public Level.t
stays abstract.  Internally, a positive Zarith successor offset represents
repeated successor, so a large closed level does not allocate a unary
chain or wrap at the machine integer limit.  This representation adds no
term former, shape or named kernel rule.

Level equality and inequality are universal over natural valuations.
The decision procedure partitions each free variable into zero or
positive.  On each partition, imax a b is zero when b is zero, and max a b
otherwise.  Normal forms are maxima of constants and variable offsets.
Variable-arm domination and the minimum of the right-hand side decide
inequality; equality checks both directions.  This is a finite exact
procedure, not a numeric sample check.  It implements the plan's semantic
comparison, rather than copying Lean.Level.isEquiv, whose source describes
its structural normal-form comparison as incomplete.

The checker passes its budget through level comparisons in conversion
and the rule packs.  Partition search and normalization poll that budget
and return Budget_exhausted when it expires.  Unlimited standalone level
comparison remains exact and imposes no universe-arity cutoff.

Universe variables bind at a global template only.  Term binders never
increase that arity.  Ordinary kernel contexts have arity zero and refuse
unbound universe variables.  Template checking uses the declared arity
and the universal comparison procedure.  This clarifies the M0 plan:
checking a template is symbolic; an ordinary instance is closed after
explicit substitution.

A scope prepass inspects all raw term fields, including annotations on
introductions that a rule may otherwise ignore.  Internal descendants
share that validation through their context; a fresh public context
starts unchecked.  This avoids repeating the full walk at each node.

The surface Poly catalog retains checked templates and their arities
outside Global.t.  Their bare names cannot resolve as ordinary globals.
Instantiation requires exactly the declared number of closed universe
arguments and a fresh output name.  It substitutes through every term,
shape payload, address, motive and branch, then rechecks the result before
installing an ordinary global.  The existing Term.Global constructor
still holds one string.  Import syntax for references between templates
belongs to Stage B.  Family templates belong to the prelude work;
Stage A requires closed family universes.  The large-elimination rule
also requires universal positivity before treating a level as non-Prop.

## Overlay and trusted-source accounting

The vendor gitlink and its working tree remain at PIN.  Unchanged modules
are copied by Dune into mechanism's build tree and compiled beside the
physical overlays.  Local namespace aliases bind the inherited surface,
backend, driver and test code to mechanism's kernel.  The inherited suites
therefore exercise the overlay, not a separately compiled vendor kernel.

TRUSTED-LINES counts each of the ten trusted logical implementations once:
the active overlay replaces the corresponding pinned implementation.
Additional local kernel implementations and interfaces count once each.
This corrects Stage 0's double-counting of replaced files.  The complete
overlay sources and their differences remain visible in PIN-DELTA.md.
The template catalog is outside the kernel: every specialization is
rechecked, just as inherited surface elaboration is rechecked.
