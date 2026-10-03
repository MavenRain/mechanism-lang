pub fn sums(xs: Vec<u32>) -> Vec<u32> {
    xs.into_iter().scan(0, |a, x| Some(x)).collect()
}
