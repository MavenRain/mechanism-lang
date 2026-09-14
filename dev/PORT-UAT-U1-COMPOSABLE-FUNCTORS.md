# PORT-UAT U1: composable heterogeneous functors

Date: 2026-09-14. Base: c1a6075. This increment supplies heterogeneous
functor composition over three shared category instances, with six
independent universe levels. It uses existing source templates and
checked proofs. Stage C and U1 remain open.

## Contract

Load `prelude/cat/category-core.mech`, then
`prelude/cat/composable-functors.mech`, before a client:

```text
specialize MechComposableFunctors (0, 1, 2, 3, 4, 5) as Chain
```

The arguments are the object and hom levels of Source, Middle and
Target, in that order. Each category core is specialized once.
The group supplies three functor records over those instances:

- `Chain_First_Functor C D c d` maps Source to Middle.
- `Chain_Second_Functor D E d e` maps Middle to Target.
- `Chain_Composite_Functor C E c e` maps Source to Target.

Each prefix also supplies `functorObj`, `functorMap`, `functorMapId`,
`functorMapComp` and `eqCongr`. Their definitions mirror the existing
heterogeneous functor template under explicit renaming. The kernel
suite checks all three mirrors against that template, including its
quantities, universe occurrences, nominal references and laws. The
mirror compares structure, and the renaming table for levels applies
to sort positions only, that is to a name after `succ`. The new
prelude thus keeps the object binder names of the template, `x`, `y`
and `z`, and keeps its own universe parameter `q`.

`Chain_compFunctor C D E c d e F G` returns the Composite functor.
Its object map is `G.1 (F.1 x)`. Its arrow map applies F, then G.
Both inputs use the same middle category record `d`; a functor over
an unrelated abstract record is refused even within the same Middle
family. Equal universe arguments do not identify the three equality
families or their category records.

Both preservation laws are source proofs. The second functor's
cross-family congruence maps the first functor's law from Middle
equality into Target equality. Target transitivity then combines
that result with the second functor's law. Object maps remain
runtime functions. Arrow endpoints and law arguments are erased.

## Validation

PRELUDE-COMPOSABLE-FUNCTORS checks parse/print round trips, symbolic
checking without escaped globals, exact definition and family
inventories, completed positive families and no new trusted entries.
The generic signatures use `(0, 1, 2, 3, 4, 5)` and pin all three
hom levels and all three functor record sorts. Generic law witnesses
retain arbitrary categories and input functors, so reflexivity alone
cannot replace a required preservation proof. An all-zero instance
checks nominal separation independently of universe differences.

Ten negative fixtures check a different middle record, different
nominal category and equality families, reversed functors, missing
laws, a false identity law, an erased object and each incorrect hom
level. The suite matches typed refusals and diagnostic substrings.
Universe arity and exhausted-budget refusals bring the total to 12.

The runtime instance uses `(0, 1, 1, 0, 2, 2)`. Object levels rise
twice while hom levels first fall, then rise. The first arrow map
specializes an erased type argument; the second introduces an erased
type argument at a higher level. This exercises both transitions in
one composite closure. The object maps add two and then double.

PRELUDE-COMPOSABLE-FUNCTORS-RUNTIME compares four exports on the
kernel, Node and Wasmtime at payloads 37 and 41. Their answers are
`(payload + 2) * 2`, `payload + 3`, `(payload + 3) * 2` and `payload`.
The third composes an increment arrow followed by a doubling arrow;
its value detects reversed arrow order. The fourth maps identity.
Both new gates use the existing SLOW, 120-second watchdog.

The mutation replay removes required proofs from both input functors,
changes an object map, replaces an arrow map with constant identity,
changes a generic universe pin, removes a negative refusal and changes
the canonical functor mirror. One more control changes the composite
functor law accessor of the new prelude, because the mirror fold stops
at its first changed row. It requires distinct named failures and
a passing restored suite. Copies contain only the required sources;
the replay records source hashes and the executable hash.

Commands and results live in `dev/M0-BUILD-LOG.md` and
`dev/validation/port-uat-u1-composable-functors/`.

## Remaining work

Sharing is explicit within this three-category template. Separate
specializations retain distinct nominal families, including separate
MechHeterogeneousFunctor instances. General reuse of category
instances across templates, identity and repeated composition APIs,
heterogeneous natural transformations and left Kan extensions, and
source-type parity remain due. This increment does not close U1,
Stage C, typed mapping or PRELUDE-CHECKED.

Kernel, surface checker, encoder and vendor source bytes, existing
gate bounds, mapping verdicts and corpus denominators are unchanged.
