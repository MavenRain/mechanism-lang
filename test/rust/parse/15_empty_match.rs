enum Never {}

enum Outcome<A> {
    Done(A),
    Stuck(Never),
}

fn absurd<A>(e: Never) -> A {
    match e {}
}

fn settle<A>(o: Outcome<A>) -> A {
    match o {
        Outcome::Done(a) => a,
        Outcome::Stuck(e) => match e {},
    }
}
