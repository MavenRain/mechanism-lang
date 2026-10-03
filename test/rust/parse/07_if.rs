fn sign(n: i64) -> i64 {
    if n > 0 {
        1
    } else if n < 0 {
        -1
    } else {
        0
    }
}

fn pick(flag: bool) -> u8 {
    let v = if flag { 1 } else { 2 };
    v
}

fn head(xs: Option<i64>) -> i64 {
    if let Some(x) = xs {
        x
    } else {
        0
    }
}

fn check(ok: bool) -> Result<(), String> {
    if !ok {
        return_err("bad");
    }
    Ok(())
}

fn return_err(msg: &str) {}
