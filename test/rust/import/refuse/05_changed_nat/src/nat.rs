//! Kernel `Nat` for emitted crates: an unbounded natural number.

use std::fmt;

const BASE: u64 = 1_000_000_000;

/// Little-endian limbs in base 10^9 with no trailing zero limb.
#[derive(Clone, PartialEq, Eq)]
pub struct Nat(Vec<u32>);

fn limb(value: u64) -> u32 {
    u32::try_from(value % BASE).unwrap_or(0)
}

fn pushed(limbs: Vec<u32>, last: u32) -> Vec<u32> {
    limbs.into_iter().chain(std::iter::once(last)).collect()
}

fn trimmed(limbs: Vec<u32>) -> Vec<u32> {
    let kept = limbs.iter().rposition(|l| *l != 0).map_or(0, |i| i + 1);
    limbs.into_iter().take(kept).collect()
}

pub fn nat_small(value: u32) -> Nat {
    let wide = u64::from(value);
    Nat(trimmed(vec![limb(wide), limb(wide / BASE)]))
}

pub fn nat_add(left: &Nat, right: &Nat) -> Nat {
    let width = left.0.len().max(right.0.len());
    let (limbs, carry) = (0..width).fold((Vec::new(), 0u64), |(limbs, carry), i| {
        let sum = u64::from(left.0.get(i).copied().unwrap_or(0))
            + u64::from(right.0.get(i).copied().unwrap_or(0))
            + carry;
        (pushed(limbs, limb(sum)), sum / BASE)
    });
    Nat(trimmed(pushed(limbs, limb(carry))))
}

pub fn nat_decimal(n: &Nat) -> String {
    n.0.split_last().map_or_else(
        || String::from("0"),
        |(top, rest)| {
            rest.iter()
                .rev()
                .fold(top.to_string(), |text, l| format!("{text}{l:09}"))
        },
    )
}

impl fmt::Debug for Nat {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&nat_decimal(self))
    }
}

pub fn changed() {}
