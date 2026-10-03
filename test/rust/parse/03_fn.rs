/// Adds two numbers.
pub fn add(a: i64, b: i64) -> i64 {
    a + b
}

fn unit() {}

fn first<T: Clone>(xs: &[T], fallback: T) -> T {
    xs.first().cloned().map_or(fallback, |x| x)
}

pub fn describe_the_configuration_of_a_long_named_function(
    name: &str,
    verbose: bool,
    depth: usize,
) -> String {
    let prefix = name.to_string();
    let label: String = format_label(&prefix, verbose);
    label
}

fn tuple() -> (i64, bool) {
    (1, true)
}

fn negate(flag: bool) -> bool {
    !flag
}
