# M0 Stage C: symbolic family reuse

Date: 2026-09-16. Base: 65faa7e. This increment extends checked family
reuse to dependencies inside polymorphic groups. Stage C and U1 remain
open on source-type parity.

## Source contract

Dependencies accept the same optional `with (LOCAL := EXISTING, ...)`
clause as closed specialization. EXISTING names a caller family or a
family imported by an earlier dependency. Bindings cannot refer forward,
including to another family introduced by the same dependency.

```text
poly (u, v) group Shared where
  specialize MechCategoryCore (u, v) as Base
  specialize MechIdentityFunctor (u, v) as Identity with (Base := Base)
end
specialize Shared (0, 0) as Instance
```

The group retains one Base family. Identity members refer to that family,
and no Identity_Base family is introduced. Specializing Shared preserves
this sharing. Nested groups flatten their surviving families in order,
and closed reuse can bind those families again.

## Checking boundary

Every binding is checked under the complete symbolic universe scope.
The renamed declaration and constructors are kernel checked in a temporary
table. Their complete certificate must agree with the existing family,
using the same conservative equality as closed reuse. Agreement at just
one future closed universe assignment is insufficient.

Only fresh families enter the resulting schema. Each group must introduce
at least one family. All imported definitions and local members recheck
against the shared family table. The symbolic table is discarded, so
failed groups leave caller globals and the template catalog unchanged.

Constructor labels, dependency prefixes and generated member names retain
their reservation rules. Standalone definition-template dependencies and
definitionally equal but structurally different certificate telescopes
remain outside this increment.

## Validation

The symbolic reuse suite checks universal rejection, dependent and nested
sharing, inventories, source round trips, closed specialization and budget
exhaustion. The cancellation check finds the smallest poll allowance that
reports the certificate mismatch, then shows that one poll less refuses
with budget exhaustion. That allowance is larger than three, so it is
beyond the entry poll, the dependency poll and the mapping poll, and the
check also shows that no member callback runs. A category fixture uses
independent functors, repeated composition and identity through one
symbolic group. Its four exports are compared on the kernel, Node and
Wasmtime at payloads 37 and 41. The two arrow exports pin the accessors
only, because every functor of the fixture maps arrows with the identity
function.

The existing closed reuse and composition gates remain regression checks.
The full battery retains the inherited TRUSTED-LINES bound and vendor pin.

`dev/reuse-mutations.py WORK --symbolic` rebuilds four controls in an
isolated copy: omitted certificate checking, a lost symbolic universe scope,
dropped parsed bindings and an omitted duplicate-binding check. A compiler
failure does not count as a killed mutation. Both the baseline and restored
suite must print the complete pinned success line.
