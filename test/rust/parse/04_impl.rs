pub struct Counter {
    count: u64,
    step: u64,
}

impl Counter {
    pub fn new(step: u64) -> Self {
        Counter { count: 0, step }
    }

    pub fn advanced(&self) -> Counter {
        Counter {
            count: self.count + self.step,
            ..self.clone_inner()
        }
    }

    fn clone_inner(&self) -> Counter {
        Counter {
            count: self.count,
            step: self.step,
        }
    }

    pub fn count(&self) -> u64 {
        self.count
    }
}

impl Default for Counter {
    fn default() -> Self {
        Counter::new(1)
    }
}
