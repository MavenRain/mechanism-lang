fn collect_all(xs: &[i64]) -> Vec<i64> {
    xs.iter().copied().collect::<Vec<i64>>()
}

fn make_adder(k: i64) -> impl Fn(i64) -> i64 {
    move |x| x + k
}

fn parse_pair(a: &str, b: &str) -> Option<(i64, i64)> {
    let x = a.parse::<i64>().ok()?;
    b.parse::<i64>().ok().map(|y| (x, y))
}

fn default_vec() -> Vec<u8> {
    Vec::<u8>::new()
}
