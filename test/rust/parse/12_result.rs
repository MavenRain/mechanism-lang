#[derive(Debug)]
pub enum Error {
    Parse(String),
    Empty,
}

pub fn parse_all(lines: Vec<String>) -> Result<Vec<i64>, Error> {
    let first = lines.first().ok_or(Error::Empty)?;
    parse_one(first).map(|n| vec![n, n + 1])
}

fn parse_one(s: &str) -> Result<i64, Error> {
    s.parse::<i64>().map_err(|e| Error::Parse(e.to_string()))
}

fn boxed(n: i64) -> Box<i64> {
    Box::new(n)
}
