/// A shape.
#[derive(Clone, Debug)]
pub enum Shape {
    Circle(f64),
    Rect { w: f64, h: f64 },
    Empty,
}

enum Either<L, R> {
    Left(L),
    Right(R),
}

pub enum Event {
    /// A key press.
    Key {
        code: u32,
        shift: bool,
    },
    Resize {
        width: u32,
        height: u32,
    },
    Quit,
}
