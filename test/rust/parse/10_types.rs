pub const LIMIT: usize = 64;

const NAME: &str = "mech";

pub type Table = Vec<(String, u64)>;

type Callback = Box<handler::Handler>;

pub fn apply(f: impl Fn(i64) -> i64, xs: &[i64]) -> Vec<i64> {
    xs.iter().map(|x| f(*x)).collect()
}

fn unit_pair() -> ((), (i64,)) {
    ((), (1,))
}

fn infer() -> Vec<i64> {
    let xs: Vec<_> = vec![1, 2, 3];
    xs
}
