//! Emitted by `mech rust-out` from `init.mech`.

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum MechNat {
    MechZero,
    MechSucc(Box<MechNat>),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MechUnit;

pub fn mech_unit() -> MechUnit {
    MechUnit
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MechEmpty {}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum MechSum<A, B> {
    MechInl(A),
    MechInr(B),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MechDecidable {
    MechIsFalse,
    MechIsTrue,
}

pub fn mech_bool_rec<A: Clone>(no: &A, yes: &A, b: bool) -> A {
    if b {
        yes.clone()
    } else {
        no.clone()
    }
}

pub fn mech_empty_elim<A: Clone>(e: MechEmpty) -> A {
    match e {}
}

pub fn mech_sum_rec<A: Clone, B: Clone, C: Clone>(
    left: &impl Fn(&A) -> C,
    right: &impl Fn(&B) -> C,
    s: &MechSum<A, B>,
) -> C {
    match s {
        MechSum::MechInl(a) => left(a),
        MechSum::MechInr(b) => right(b),
    }
}

pub fn mech_nat_rec<P: Clone>(zero: &P, step: &impl Fn(&MechNat, &P) -> P, n: &MechNat) -> P {
    match n {
        MechNat::MechZero => zero.clone(),
        MechNat::MechSucc(previous) => step(previous, &mech_nat_rec::<P>(zero, step, previous)),
    }
}

pub fn mech_decidable_bool(d: MechDecidable) -> bool {
    match d {
        MechDecidable::MechIsFalse => false,
        MechDecidable::MechIsTrue => true,
    }
}

pub fn mech_j<P: Clone>(refl_case: &P) -> P {
    refl_case.clone()
}

pub fn mech_transport<B: Clone>(value: &B) -> B {
    mech_j::<B>(value)
}
