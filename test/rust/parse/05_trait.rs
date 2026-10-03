pub trait Named {
    fn name(&self) -> String;

    fn greeting(&self) -> String {
        greet(self.name())
    }
}

pub trait Shape: Named + Clone {
    fn area(&self) -> f64;
}

trait Marker {}

struct Dog;

impl Named for Dog {
    fn name(&self) -> String {
        String::from("dog")
    }
}

fn greet(name: String) -> String {
    name
}
