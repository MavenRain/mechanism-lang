pub fn choose(x: bool) -> bool {
    if {
        let y: bool = x;
        y
    } {
        true
    } else {
        false
    }
}

pub enum Light {
    Red,
    Green,
}

pub fn go(x: Light) -> bool {
    match {
        let y: Light = x;
        y
    } {
        Light::Red => false,
        Light::Green => true,
    }
}

pub enum Void {}

pub fn consume(x: Void) -> bool {
    match {
        let y: Void = x;
        y
    } {}
}
