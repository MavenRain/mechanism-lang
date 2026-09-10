# M0 Stage C: textual polymorphic families

Date: 2026-09-10. Base: f7f7c04, textual prenex definitions.

## Source syntax

```text
poly (u) mu Box (0 A : Sort u) : Sort (succ u) with
| box : A -> Box A
specialize Box (1) as DataBox
```

The existing nonempty universe binder list scopes over the complete
family header and constructors. The parser accepts one family, including
an empty family or self recursion. Multiple-family groups and textual
member definitions remain unsupported. The ordinary mu grammar supplies
parameter and index telescopes, constructor binder sugar and either with
or :=. Canonical printing preserves all parsed level expressions.

Sort levels use the established raw convention: Sort 0 is Prop and
Sort 1 is Type 0. A family template's result must remain Prop or remain
Type for every universe assignment. For example, Sort (succ u) is a
valid data-family result; Sort u may change between Prop and Type and is
rejected. This is the existing universal family judgment.

## Checking and names

The elaborator uses an immutable temporary family to resolve constructor
self references. Family_poly.declare then checks the complete schema
universally. Neither that temporary family nor its constructors enter
ordinary globals. Definition and family templates share a source-check
lifetime and reserve their names against each other and later declarations.

Explicit specialization supplies exactly the template's number of closed
levels. A compound level keeps its own parentheses inside that list, as
in `specialize Box ((max 0 1)) as AgainBox`, because the outer
parentheses delimit the argument list. The existing
Family_poly.instantiate substitutes all levels and self references, then
rechecks the closed family and constructors. Only
the resulting family enters globals; no definition or axiom row is added.
Repeated instances keep their constructor labels, resolved through the
expected family type. A specialized constructor cannot take a template
name declared since its schema. Such an error returns no partial globals.
The caller's budget is threaded through both judgments.

This adds no kernel or prelude-catalog rule. The vendor pin, all kernel
overlays and both programmatic catalogs keep their bytes. The measured
PIN-DELTA changes are confined to syntax, parser and elaborator overlays.

## Validation

The source fixture checks from Global.empty. It covers boxes at Prop,
Type 0 and Type 1; recursive lists; sums with independent universe levels;
indexed equality over data and proofs; definition-template composition;
and a computed parameter type that exercises the elaborator's scope.
The suite checks round-trips, exact family and definition inventories,
row order, no postulates or primitives, and five independent normal forms.
Paired refusals cover scope, arity, invalid constructors, unstable result
universes, namespace collisions, catalog lifetime and budget exhaustion.
Each refusal pins the error kind as well as the message text, so a
producer cannot move to another string arm without suite motion.  A
template constructor label equal to its own family name is refused at the
template, where the cause is visible.  A specialize refuses an installed
label equal to the instance name.  A family template's own labels reserve
nothing until an instance installs them, so two independent declarations
give the same verdict in either order.

The runtime gate checks a specialized box payload and a recursive list
sum on the kernel, Node and Wasmtime. A payload mutation changes 37 to
41 while the list sum stays 12 on all hosts. Every variant checks and
has empty axiom output before emission.

`python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY --families`
builds eleven isolated controls for scope, level argument order, catalog
reservation, installation, constructor collisions, budget propagation,
payload computation, the family-template half of the post-install label
check, the budget of the specialize path, the self-named constructor rule
and the instance-name label rule. Every mutant must build cleanly and fail
the family suite with the recorded diagnostic. The original seven
definition controls remain available without --families.

The full `zsh dev/gates.sh` battery retains the existing trusted bounds.
The known TRUSTED-LINES failure is kernel=4182/3000, encoder=246/900.
This slice does not close Stage C or M0. Textual family groups and members,
category targets, checked source-type parity and the D-A-1 ruling remain
open.

Final validation: 13 families, 18 definitions, five normal forms and
30 refusals passed. The eleven family controls and the seven definition
controls passed.
The complete battery had 31 passing legs and only the inherited
TRUSTED-LINES failure. Evidence and source hashes are retained in
dev/validation/stage-c-prenex-families/.
