fn total(xs: Vec<i64>) -> i64 {
    xs.iter().map(|x| x * 2).sum()
}

fn names(people: Vec<Person>) -> Vec<String> {
    people
        .iter()
        .filter(|p| p.age > 18)
        .map(|p| p.name.clone())
        .collect()
}

fn folded(xs: Vec<i64>) -> i64 {
    xs.iter().fold(0, |acc, x| {
        let next = acc + x;
        next * 2
    })
}

struct Person {
    name: String,
    age: u32,
}
