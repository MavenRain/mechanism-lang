//! Structs: named, unit, tuple, docs and derives.

/// A point in the plane.
#[derive(Clone, Debug, PartialEq)]
pub struct Point {
    pub x: i64,
    pub y: i64,
}

pub struct Unit;

pub(crate) struct Meters(pub u64);

struct Pair(i64, String);

#[derive(Debug)]
struct Wrapper<T: Clone> {
    /// The wrapped value.
    inner: T,

    count: usize,
}
