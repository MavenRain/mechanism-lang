enum Token {
    Num(i64),
    Op { sym: char, prec: u8 },
    End,
}

fn weight(t: &Token) -> i64 {
    match t {
        Token::Num(n) if *n > 0 => *n,
        Token::Num(_) => 0,
        Token::Op { sym: '+', .. } | Token::Op { sym: '-', .. } => 1,
        Token::Op { prec, .. } => {
            let p = *prec;
            i64::from(p)
        }
        Token::End => -1,
    }
}

fn classify(n: i64) -> u8 {
    match n {
        0 => 0,
        1 | 2 | 3 => 1,
        _ => 2,
    }
}
