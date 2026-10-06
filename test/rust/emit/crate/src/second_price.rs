//! Emitted by `mech rust-out` from `second-price.mech`.

use crate::init::*;
use crate::nat::*;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AuctionChoice {
    AuctionLose,
    AuctionWin,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AuctionOrder {
    AuctionAbove,
    AuctionBelow,
}

pub fn auction_compare(tie_wins: bool, bid: &MechNat, price: &MechNat) -> AuctionOrder {
    match bid {
        MechNat::MechZero => match price {
            MechNat::MechZero => {
                if tie_wins {
                    AuctionOrder::AuctionAbove
                } else {
                    AuctionOrder::AuctionBelow
                }
            }
            MechNat::MechSucc(_) => AuctionOrder::AuctionBelow,
        },
        MechNat::MechSucc(previous) => match price {
            MechNat::MechZero => AuctionOrder::AuctionAbove,
            MechNat::MechSucc(other) => auction_compare(tie_wins, previous, other),
        },
    }
}

pub fn auction_choice(order: AuctionOrder) -> AuctionChoice {
    match order {
        AuctionOrder::AuctionAbove => AuctionChoice::AuctionWin,
        AuctionOrder::AuctionBelow => AuctionChoice::AuctionLose,
    }
}

pub fn auction_allocate(tie_wins: bool, bid: &MechNat, price: &MechNat) -> AuctionChoice {
    auction_choice(auction_compare(tie_wins, bid, price))
}

pub fn auction_payment(price: &MechNat, choice: AuctionChoice) -> MechNat {
    match choice {
        AuctionChoice::AuctionLose => MechNat::MechZero,
        AuctionChoice::AuctionWin => price.clone(),
    }
}

pub fn auction_max(left: &MechNat, right: &MechNat) -> MechNat {
    match auction_allocate(true, left, right) {
        AuctionChoice::AuctionLose => right.clone(),
        AuctionChoice::AuctionWin => left.clone(),
    }
}

pub fn auction_priority_b(a: &MechNat, c: &MechNat) -> bool {
    match auction_allocate(true, a, c) {
        AuctionChoice::AuctionLose => true,
        AuctionChoice::AuctionWin => false,
    }
}

pub fn auction_choice_a(a: &MechNat, b: &MechNat, c: &MechNat) -> AuctionChoice {
    auction_allocate(true, a, &auction_max(b, c))
}

pub fn auction_choice_b(a: &MechNat, b: &MechNat, c: &MechNat) -> AuctionChoice {
    auction_allocate(auction_priority_b(a, c), b, &auction_max(a, c))
}

pub fn auction_choice_c(a: &MechNat, b: &MechNat, c: &MechNat) -> AuctionChoice {
    auction_allocate(false, c, &auction_max(a, b))
}

pub fn auction_payment_a(a: &MechNat, b: &MechNat, c: &MechNat) -> MechNat {
    auction_payment(&auction_max(b, c), auction_choice_a(a, b, c))
}

pub fn auction_payment_b(a: &MechNat, b: &MechNat, c: &MechNat) -> MechNat {
    auction_payment(&auction_max(a, c), auction_choice_b(a, b, c))
}

pub fn auction_payment_c(a: &MechNat, b: &MechNat, c: &MechNat) -> MechNat {
    auction_payment(&auction_max(a, b), auction_choice_c(a, b, c))
}

pub fn auction_add(left: &MechNat, right: &MechNat) -> MechNat {
    match left {
        MechNat::MechZero => right.clone(),
        MechNat::MechSucc(previous) => MechNat::MechSucc(Box::new(auction_add(previous, right))),
    }
}

pub fn auction_to_native(n: &MechNat) -> Nat {
    mech_nat_rec::<Nat>(
        &nat_small(0),
        &|_value: &MechNat, previous: &Nat| nat_add(&nat_small(1), previous),
        n,
    )
}

pub fn auction_bit(choice: AuctionChoice) -> Nat {
    match choice {
        AuctionChoice::AuctionLose => nat_small(0),
        AuctionChoice::AuctionWin => nat_small(1),
    }
}
