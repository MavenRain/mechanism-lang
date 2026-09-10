# M0 Stage C: polymorphic dependent functions and pairs

Date: 2026-09-09.  Base: d59008b, the committed equality operations slice.
This increment supplies the polymorphic Pi and Sigma prelude targets
named by M0 plan section 7, plus checked pair operations.  Stage C and
PRELUDE-CHECKED remain open.

## Definitions

`Dependent.catalog` checks six independent Poly templates from the
supplied globals.  Checking from `Global.empty` needs no postulates,
primitives or monomorphic prelude.  The following Sort notation describes
the programmatic API; it is not new textual syntax.

```text
MechPi.{u,v} (0 A : Sort u) (0 B : A -> Sort v) : Sort (imax u v)
MechSigma.{u,v} (0 A : Type u) (0 B : A -> Type v) : Type (max u v)
mechSigmaMk.{u,v} (0 A : Type u) (0 B : A -> Type v)
  (x : A) (y : B x) : MechSigma A B
mechSigmaFst.{u,v} (0 A : Type u) (0 B : A -> Type v)
  (p : MechSigma A B) : A
mechSigmaSnd.{u,v} (0 A : Type u) (0 B : A -> Type v)
  (p : MechSigma A B) : B (mechSigmaFst A B p)
mechSigmaRec.{u,v,w} (0 A : Type u) (0 B : A -> Type v)
  (0 P : MechSigma A B -> Sort w)
  (step : (x : A) -> (y : B x) -> P (x, y))
  (p : MechSigma A B) : P p
```

Pi supports Prop-valued functions at arbitrary domain Sorts.  Sigma's
carrier and fiber are Type-valued, and the recursor independently supports
a Prop or Type motive.  The pair components and step have quantity Many.
The eliminators consume their pair scrutinee at quantity One.

Pair operations expand to the existing point-shaped Lan, In and Elim;
Pi expands to Ran.  No new form, kernel rule or trusted entry is added.
Each template embeds these forms directly, avoiding references between
templates.  Every closed specialization checks again and enters ordinary
globals under its chosen name.  Unlike separate SMu family instances,
equal pair specializations unfold to the same type.

## Validation contract

PRELUDE-DEPENDENT checks the catalog symbolically from Global.empty,
then installs 29 closed definitions.  Four groups each instantiate all
six templates, including both orders of mixed carrier universes and a
higher pair universe.  Five additional Pi instances exercise Prop in
both positions and higher carriers.  Generic source contracts check exact
result universes, variable fibers, and a motive depending on the pair.

Twelve independently written normal forms check projections, elimination,
type-valued computations and proof-valued functions.  Indexed source
witnesses also require the checker to observe computation.  The Up
witness checks a value against the type-valued second projection, so
that projection must compute to the carrier.  Six invalid uses check the
wrong fiber, domain, projection, dependent step, Pi body and universe.
Each first checks an accepted counterpart and then pins the refusal
diagnostic.  The suite also refuses an exhausted caller budget, and it
counts the installed instances in the global environment instead of
computing them from its own list.  The environment audit rejects axioms,
primitives, provisional families and builtin families.

Run `python3 -I dev/dependent-mutations.py NEW_WORK_DIRECTORY` to replay
ten isolated controls.  Each mutant must compile cleanly before its
specific test failure counts.  The script records exact replacements,
hashes and outputs, restores every source file, and checks the restored
suite.  The full gate battery retains all prior legs and adds the new one.

The kernel, vendor pin, trusted-line bounds and mapping verdicts retain
their existing behavior.  Textual universe binders, independent-carrier
congruence, category targets and source-type parity remain open, as do
the inherited D-A-1 ruling and programmatic-only catalog plan question.

Build copy: `/Users/oobi/Documents/gpt2/mechanism-dependent`.
Integration checks the canonical HEAD and original file contents before
copying validated changes into `/Users/oobi/Documents/mechanism-lang`
and staging them.  No commit is created.
