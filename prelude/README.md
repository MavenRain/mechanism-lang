# Checked foundations

`init.mech` defines ten SMu families and sixteen functions without axioms or
primitives.  The test harness checks it in `Global.empty`.  This prevents
the driver's initial `Nat` axiom from supplying a hidden dependency.

The data families are `MechNat`, `MechBool`, `MechUnit`, `MechEmpty`,
`MechSum`, and `MechDecidable`.  They live at `Type 0`.  `MechTrue` and
`MechFalse` live at `Prop`.  Decidable has two data constructors with
erased proof fields.  It does not resolve instances or decide propositions.
`MechPi` and `MechSigma` expand to the existing function and dependent pair
forms.  These declarations are monomorphic at the stated universes.

`MechProofEq` is restricted to two proofs of the same proposition.
`mechProofRefl` and `mechProofJ` check for that family, and J has a
Prop-valued motive that depends on both endpoints and the equality proof.
This restricted family is not a mapping target for Lean's general `Eq`.
Proof irrelevance already identifies its endpoints.  Its constructor has
a runtime witness field, so the existing subsingleton check refuses
large elimination.

`MechEq` compares values of any carrier at `Type 0`.  It fixes the left
endpoint as an erased parameter and the right endpoint as an erased
index, with a nullary reflexivity constructor.  The checker now permits
erased data indices in Prop families.  Constructor-field bounds and the
subsingleton large-elimination criterion retain their existing behavior.
`mechJ` has a Type 0 motive depending on the right endpoint and the proof;
`mechTransport`, `mechSymm`, `mechTrans` and `mechCongr` are checked source
definitions alongside `mechRefl`.

`test/fixtures/prelude/equality.mech` checks dependent transport, J
computation, and a separate higher-carrier equality with a type-cast
example.  The `.mech` declarations remain monomorphic.  The separate
programmatic catalog below supplies polymorphic library cast.  The precise negatives
keep unequal endpoints, relevant indices, erased endpoint use and data
constructor fields outside the accepted language.

The source client in `test/fixtures/prelude/client.mech` checks after this
file.  It constructs values, exercises dependent fields and nested
constructors, and uses singleton indices to check recursor computation.
The harness also checks rejection diagnostics in `test/neg/prelude`.

Parameterized constructors use the local surface elaborator overlay.
The expected family type supplies its parameters.  Each field elaborates
at its declared type under the parameters and preceding fields.  The
kernel checks the complete constructor and its result indices.

`families.ml` supplies a separate programmatic catalog of universally
checked MechEq and MechSum templates through `Family_poly`.  MechEq takes
the carrier's Sort level, including Prop.  MechSum takes two Type levels;
the kernel levels of its carriers are their successors.  Closed instances
have fresh names and are rechecked before entering ordinary globals.
Constructor names are local to the expected family, so instances can share
`mechReflCtor`, `mechInl` and `mechInr` with the monomorphic declarations.

The PRELUDE-POLY gate installs five instances and checks
`test/fixtures/prelude/polymorphic.mech`.  Indexed witnesses force casts at
two carrier universes and mixed-universe sums to compute.  The test audits
both global entries and families, rejecting axioms, primitives, provisional
families and builtin families.  Textual universe binders and imported
source-type parity are not supplied by this catalog.

`equality.ml` supplies a separate catalog with checked member definitions.
Its MechEq takes the carrier and motive Sort levels and includes `refl`
and `transport`.  Its MechTypeEq takes one Type level and includes `refl`
and `cast`.  Specialization installs each member with the instance name
and an underscore as a prefix, then rechecks its type and body in order.
The templates never enter ordinary globals.  PRELUDE-TRANSPORT checks
seven instances, dependent computation witnesses, four rejection
diagnostics, one scope control and the absence of trusted entries.  A
member named like another template is refused as a collision.  Textual
universe binders and source-type parity remain separate work.
