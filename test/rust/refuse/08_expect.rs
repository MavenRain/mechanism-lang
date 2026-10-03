pub fn first(x: Option<u32>) -> u32 {
    x.expect("present")
}
