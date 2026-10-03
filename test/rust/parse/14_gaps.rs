/// Settings with documented fields.
pub struct Settings {
    /// The display name.
    pub name: String,

    /// How many times to retry.
    #[doc(hidden)]
    pub retries: u32,
}

pub enum Mode {
    /// Fast mode.
    Fast,

    #[allow(dead_code)]
    Slow { delay: u32 },
}

fn run(s: Settings) -> u32 {
    let a = s.retries;

    let b = a + 1;
    b
}
