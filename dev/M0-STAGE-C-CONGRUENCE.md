# M0 Stage C: congruence across independent universes

Date: 2026-09-09.  Base: a67c035, the committed dependent-prelude slice.
This increment resolves the independent-carrier congruence item in the
Stage C remaining-work contract.  Stage C and PRELUDE-CHECKED remain open.

## API

`Family_poly.declare_group` accepts a nonempty ordered family list and
optional ordered member definitions.  Families can reference predecessors,
and members can reference every family and earlier members.  Checking uses
one symbolic universe scope and discards the temporary environment.
Forward references and mutual recursion between group families are refused.

The first family names the template.  On specialization to N, it becomes
N, companion F becomes N_F, and member m becomes N_m.  All raw universe
occurrences and references specialize together.  Closed checking repeats
in declaration order.  Any collision or error returns no partial globals.
The existing single-family API retains its names and calling convention.

`Congruence.catalog Global.empty` checks the MechCongr template without
postulates, primitives or the monomorphic prelude.  It takes Sort levels
[u; v], with zero denoting Prop and successor n denoting Type n:

```text
N        : (0 A : Sort u) -> (0 x : A) -> (0 y : A) -> Prop
N_Result : (0 B : Sort v) -> (0 x : B) -> (0 y : B) -> Prop
N_congr  : (0 A : Sort u) -> (0 B : Sort v) -> (0 f : A -> B) ->
           (0 x : A) -> (0 y : A) -> N A x y -> N_Result B (f x) (f y)
```

Each equality has a `mechReflCtor` constructor resolved by expected type.
Congruence consumes the domain equality and returns codomain reflexivity.
Both carriers, the function and endpoints are erased arguments.  The proof
argument is unrestricted and the elimination consumes it at quantity One.
The implementation uses existing SMu and Elim forms without kernel changes.

These are distinct nominal families.  This API does not convert existing
Equality.catalog proofs, add cross-template references, or change that
catalog's congruence.  No source syntax or source-type parity claim is
added.

## Validation

FAMILY-GROUPS checks 22 cases: ordered constructor and member
dependencies, two disjoint instances, companion universe substitution,
scope refusals, collisions, a member that references another template,
absent dependencies during closed rechecking, wrong arity, open level
arguments, budget exhaustion, and a refused instantiation after which
the caller globals still accept a fresh instance.

PRELUDE-CONGRUENCE checks eight instances: equal data universes (twice),
both orders of Type 0 and Type 1, Prop in both positions, each Prop/data
order, and Type 2 to Prop.  Generic contracts bind distinct endpoints and
independent carrier types.  Four proofs are eliminated into independently
checked natural-number normal forms.  Five invalid uses have accepted
counterparts and diagnostic oracles: wrong function, reversed proof,
domain equality as the output, a different instance's output family, and
a codomain universe the instance does not accept.  Each oracle pins the
printed term and the printed expected type, so no oracle is a prefix of
another oracle.
The suite audits definitions and families and refuses an exhausted budget.

`python3 -I dev/congruence-mutations.py NEW_WORK_DIRECTORY` copies the
source into a new external directory and replays nine controls.  Each
must build cleanly and fail its designated suite with a pinned diagnostic.
Exact source replacements, hashes, outputs and restored-suite results are
retained with the replay.  The original repository is never mutated.

The gate battery adds FAMILY-GROUPS and PRELUDE-CONGRUENCE and retains every
previous leg.  The inherited TRUSTED-LINES bound remains 3,000 kernel lines
and 900 encoder lines.  Textual universe binders, category targets and
source-type parity remain open, alongside the existing D-A-1 ruling.

Build copy: `/Users/oobi/Documents/gpt2/mechanism-congruence`.
Validated files are staged in `/Users/oobi/Documents/mechanism-lang` only
after checking the original HEAD and file contents.  No commit is created.
