# M0 Stage C: checked family reuse

Date: 2026-09-15. Base: 2ff1eb9. This increment lets closed family
specializations share explicitly selected nominal identities. It supplies
the instance sharing needed for independent functors, identity and repeated
composition. Stage C and U1 remain open on source-type parity.

## Source contract

```text
specialize MechCategoryCore (0, 1) as C
specialize MechCategoryCore (2, 3) as D
specialize MechHeterogeneousFunctor (0, 1, 2, 3) as F
  with (Source := C, Target := D)
```

The optional `with` clause contains a nonempty comma-separated list of
`LOCAL := EXISTING` bindings. LOCAL is a template-internal family name,
including flattened nested names such as `Base_Source`. EXISTING must
already be a family in the caller's globals. Several local families can
bind the same existing family when their checked declarations agree.

Selected families retain their existing names. The specialization creates
no aliases named `F_Source` or `F_Target`; their category operations remain
fresh definitions such as `F_Source_Category`. All family references in
those definitions point to C and D. Unselected families retain the ordinary
fresh-prefix behavior. A root family can also be reused, but `as NAME`
must still be fresh. A specialization with no new families or members
leaves the globals unchanged.

## Checking boundary

Every template still checks universally before specialization. The reuse
path substitutes closed levels and renames all family references. Each
selected declaration and its constructors are kernel checked in a
temporary family table. Its resulting complete family certificate must
equal the existing certificate exactly, including parameter and index
telescopes, binder names, universe, constructor order, arguments, result
indices, recursion flags and positivity status. This intentionally refuses
some definitionally equivalent presentations with different syntax.

Only that temporary table removes and reconstructs the selected family.
It never escapes. Fresh families and every member are checked against the
actual caller environment; existing entries are preserved. Explicit reuse
can identify equally shaped nominal families, but ordinary specialization
still creates distinct ones. Failure is atomic. Budget polls remain active
during binding validation, traversal, family rechecking and member checks.

Unknown, forward or duplicate bindings, incompatible families, generated
name collisions and reuse on standalone definition templates are refused.
Reuse clauses inside symbolic group dependencies remain unsupported.
Constructor labels retain their existing spelling and reservation rules.

## Category API

Load `cat/category-core.mech`, `cat/heterogeneous-functor.mech` and
`cat/composable-functors.mech`. Bind the latter's Source, Middle and Target
families to the same cores used by the independently specialized functors.
Their records convert to First_Functor and Second_Functor. Bind another
composition template to the result endpoints to compose the result again.

`cat/identity-functor.mech` adds the lightweight MechIdentityFunctor group
at one object/hom universe pair. Bind its Base family to an existing core.
Its Functor representation agrees with the heterogeneous functor at that
pair, and idFunctor checks both preservation laws. This avoids loading the
larger same-pair natural-transformation and left Kan API to use identity.

## Validation

TEMPLATE-REUSE checks source round trips, exact family/definition
inventories, preserved certificates, computations, partial, nested and
dependent bindings, partial reuse of a mutual group, a level argument that
is not normal in the family level, the same level argument in a parameter
type and in a constructor argument, and no new axioms. Seventeen source
refusals, six parser refusals and six raw API controls cover malformed
bindings, nominal separation, name collisions, budget exhaustion, corrupted
certificates, the reuse of a member of a mutual group and an unknown
template name. The raw API check holds each control in one row of a list and
prints the length of that list, so the deletion of any control lowers the
count and the pinned gate line of dev/gates.sh fails. The mixed universe
contract composes arbitrary independently specialized functors twice and
checks identity at the target pair.

TEMPLATE-REUSE-RUNTIME compares four exports at payloads 37 and 41 on the
kernel, Node and Wasmtime. The object chain computes `(n + 2) * 2 + 5`;
the arrow chain computes `(n + 3) * 2`. Identity preserves the object and
the supplied arrow, yielding n and n + 7. The kernel gate and runtime gate
use the existing SLOW tier. A measured standalone kernel/emit probe took
38.45 seconds; the runtime driver caps the kernel and emit step at 50
seconds and each host run at 10 seconds, both inside its existing
110-second total budget. No existing watchdog is changed.

Evidence and reproduction commands live in `dev/validation/stage-c-reuse/`.
The kernel, encoder, vendor pin, mapping verdicts and denominators retain
their bytes. The inherited TRUSTED-LINES failure stays subject to D-A-1.

## Remaining work

Symbolic reuse within group declarations, standalone definition-template
dependencies, convenience APIs for shared natural transformations and Kan
extensions, source-type parity, typed mapping and PRELUDE-CHECKED remain
open. The general closed-family binding mechanism accepts nested family
names, but this increment validates its category clients through functor
identity and repeated composition.
