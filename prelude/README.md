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
example.  The library declarations remain monomorphic.  Polymorphic
families and a general library cast remain open.  The precise negatives
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
