pub fn ping(x: bool) -> bool {
    pong(x)
}

pub fn pong(x: bool) -> bool {
    ping(x)
}
