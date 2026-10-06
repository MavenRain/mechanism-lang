#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Maybe<A> {
    Nothing,
    Just(A),
}

pub fn good(x: bool) -> bool {
    let y = Maybe::Nothing;
    x
}
