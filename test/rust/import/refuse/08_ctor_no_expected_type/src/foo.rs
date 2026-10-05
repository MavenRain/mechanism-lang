#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Maybe<A> {
    Nothing,
    Just(A),
}

pub fn good(x: bool) -> bool {
    match Maybe::Nothing {
        Maybe::Nothing => x,
        Maybe::Just(_) => x,
    }
}
